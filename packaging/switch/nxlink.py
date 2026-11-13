#!/usr/bin/env python3
"""Sends an NRO to the Switch Homebrew Menu's NetLoader (press Y), and with --server prints the
program's stdout/stderr, which libnx's nxlinkStdio() sends back to the sender.

A standard-library reimplementation of devkitPro's nxlink protocol (switchbrew/switch-tools
src/nxlink.c): it runs natively on the Mac, where the Docker copy of nxlink cannot reach the LAN.

    python3 packaging/switch/nxlink.py -a CONSOLE_ADDRESS -p smac/opensmac.nro --server \\
        build/switch/opensmac.nro -- --new --human 1

Protocol (TCP 28280 on the console): int32 name length, name, int32 file size; reply int32 0;
one zlib stream sent as chunks of at most 16 KiB, each preceded by its int32 length; reply int32;
int32 argument bytes, then the arguments, each ending in a NUL. All integers little-endian.
With --server the console connects back to TCP 28771 on this machine.
"""
import argparse
import os
import socket
import struct
import sys
import time
import zlib

SERVER_PORT = 28280  # the console's NetLoader
CLIENT_PORT = 28771  # this machine, for the program's output
CHUNK = 16 * 1024


def recv_exact(sock, n):
    data = b''
    while len(data) < n:
        part = sock.recv(n - len(data))
        if not part:
            raise ConnectionError('console closed the connection')
        data += part
    return data


def recv_int(sock):
    return struct.unpack('<i', recv_exact(sock, 4))[0]


def connect(address, retries):
    for attempt in range(retries):
        try:
            return socket.create_connection((address, SERVER_PORT), timeout=5)
        except OSError as e:
            if attempt == retries - 1:
                raise SystemExit('cannot reach the NetLoader at %s:%d (%s). Is the Homebrew Menu on Y?' % (address, SERVER_PORT, e))
            time.sleep(1)


def send_nro(address, path, data, args, retries):
    sock = connect(address, retries)
    sock.settimeout(60)
    name = path.encode()
    sock.sendall(struct.pack('<I', len(name)) + name + struct.pack('<I', len(data)))
    reply = recv_int(sock)
    if reply != 0:
        raise SystemExit({-1: 'console: failed to create file', -2: 'console: insufficient space',
                          -3: 'console: insufficient memory'}.get(reply, 'console refused: %d' % reply))
    print('Sending %s, %d bytes' % (path, len(data)), flush=True)
    z = zlib.compressobj(zlib.Z_DEFAULT_COMPRESSION)
    sent = blocks = 0

    def send_compressed(buf):
        nonlocal sent, blocks
        for i in range(0, len(buf), CHUNK):
            piece = buf[i:i + CHUNK]
            sock.sendall(struct.pack('<I', len(piece)) + piece)
            sent += len(piece)
            blocks += 1

    for i in range(0, len(data), CHUNK):
        send_compressed(z.compress(data[i:i + CHUNK]))
    send_compressed(z.flush(zlib.Z_FINISH))
    print('%d sent (%.2f%%), %d blocks' % (sent, sent * 100.0 / max(len(data), 1), blocks), flush=True)
    recv_int(sock)
    cmd = b''.join(a.encode() + b'\0' for a in args)
    sock.sendall(struct.pack('<I', len(cmd)) + cmd)
    sock.shutdown(socket.SHUT_WR)
    sock.close()


def serve_output(listener):
    print('server active ...', flush=True)
    conn, _ = listener.accept()
    listener.close()
    out = sys.stdout.buffer
    while True:
        try:
            data = conn.recv(4096)
        except ConnectionResetError:
            print('\n[nxlink.py] the console dropped the connection: the program crashed or was closed', flush=True)
            break
        if not data:
            break
        out.write(data)
        out.flush()
    conn.close()
    print('exiting ...', flush=True)


def main():
    p = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    p.add_argument('-a', '--address', required=True, help="console's IPv4 address")
    p.add_argument('-p', '--path', help='where the NetLoader saves the NRO on the card (default: its name)')
    p.add_argument('-r', '--retries', type=int, default=10, help='connection attempts, one a second')
    p.add_argument('-s', '--server', action='store_true', help="print the program's output after sending")
    p.add_argument('nro')
    p.add_argument('args', nargs=argparse.REMAINDER, help='after --: arguments for the program')
    o = p.parse_args()
    args = o.args[1:] if o.args[:1] == ['--'] else o.args
    with open(o.nro, 'rb') as f:
        data = f.read()
    listener = None
    if o.server:
        # Listen before sending: a program that starts fast connects back at once.
        listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(('', CLIENT_PORT))
        listener.listen(1)
    send_nro(o.address, o.path or os.path.basename(o.nro), data, args, o.retries)
    if listener:
        serve_output(listener)


if __name__ == '__main__':
    main()
