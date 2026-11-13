#!/usr/bin/env python3
"""Drives the Switch build over Wi-Fi without touching the console (src/app/switch_dev.h).
Needs a build made with DEV=ON packaging/switch/build.sh.

    dev.py send [opensmac args...]  send build/switch/opensmac.nro and start it with these arguments. Works
                                    with the Homebrew Menu's NetLoader (press Y) and with a running build,
                                    which quits into the new one. Output: work/switch/dev.out.
    dev.py keys TOKENS              scripted input, the tokens of --keys: ENTER,b,CMD:0x105,WAIT:500 ...
    dev.py tap X Y                  a touch at X,Y of the 1280x720 screen (through SDL, as a finger)
    dev.py key CODE                 a key press through SDL (an SDL keycode; a character's is its own)
    dev.py shot [file.png]          a screenshot (default work/switch/shot.png)
    dev.py put FILE NAME            copy a local file to /switch/smac/NAME on the card
    dev.py get NAME [FILE]          fetch /switch/smac/NAME (default: its own name in work/switch)
    dev.py quit                     end the running game (the console then waits for the next build)
    dev.py log [lines]              the end of the console's output

The console's address: SWITCH_IP, else packaging/switch/.switch-ip (--ip ADDRESS writes it).
"""
import os
import signal
import socket
import struct
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
NRO = ROOT / "build/switch/opensmac.nro"
RUNS = ROOT / "work/switch"
OUT = RUNS / "dev.out"
IP_FILE = HERE / ".switch-ip"
COMMAND_PORT = 28282


def console_ip(args):
    if "--ip" in args:
        i = args.index("--ip")
        ip = args[i + 1]
        del args[i:i + 2]
        IP_FILE.write_text(ip + "\n")
        return ip
    ip = os.environ.get("SWITCH_IP") or (IP_FILE.read_text().strip() if IP_FILE.exists() else "")
    if not ip:
        sys.exit("the console's address: --ip ADDRESS once, or SWITCH_IP")
    return ip


def recv_exact(s, n):
    data = b""
    while len(data) < n:
        part = s.recv(n - len(data))
        if not part:
            raise ConnectionError("the console closed the connection")
        data += part
    return data


def command(ip, line, timeout=20):
    """One command; its one-line answer."""
    with socket.create_connection((ip, COMMAND_PORT), timeout=timeout) as s:
        s.sendall(line.encode() + b"\n")
        return s.recv(4096).decode(errors="replace").strip()


def fetch(ip, line, timeout=30):
    """A command answered by a size and that many bytes; None when the console has nothing."""
    with socket.create_connection((ip, COMMAND_PORT), timeout=timeout) as s:
        s.sendall(line.encode() + b"\n")
        size = struct.unpack("<i", recv_exact(s, 4))[0]
        return recv_exact(s, size) if size >= 0 else None


def stop_old_sender():
    """The previous send's output server holds TCP 28771: the new build connects back there."""
    pid_file = RUNS / ".dev-sender.pid"
    if pid_file.exists():
        try:
            os.killpg(int(pid_file.read_text()), signal.SIGTERM)
        except (ProcessLookupError, ValueError, PermissionError):
            pass
        pid_file.unlink()
        time.sleep(0.5)


def send(ip, args):
    if not NRO.exists():
        sys.exit(f"no build at {NRO}: DEV=ON packaging/switch/build.sh")
    RUNS.mkdir(parents=True, exist_ok=True)
    stop_old_sender()
    out = open(OUT, "w")
    sender = subprocess.Popen([sys.executable, "-u", str(HERE / "nxlink.py"), "-a", ip, "-r", "30", "-p",
                               "smac/opensmac.nro", "-s", str(NRO), "--"] + args,
                              stdout=out, stderr=subprocess.STDOUT, start_new_session=True)
    (RUNS / ".dev-sender.pid").write_text(str(sender.pid))
    for _ in range(120):  # until the upload is through, so a failure shows here
        text = OUT.read_text(errors="replace")
        if "server active" in text or sender.poll() is not None:
            break
        time.sleep(0.5)
    print(OUT.read_text(errors="replace").strip()[-600:])
    print(f"(output continues in {OUT})")


def bmp_to_png(bmp, target):
    """sips (macOS) converts; elsewhere the BMP is kept under the target's name with .bmp."""
    tmp = target.with_suffix(".bmp")
    tmp.write_bytes(bmp)
    if target.suffix.lower() == ".bmp":
        return tmp
    done = subprocess.run(["sips", "-s", "format", "png", str(tmp), "--out", str(target)],
                          capture_output=True) if sys.platform == "darwin" else None
    if done and done.returncode == 0:
        tmp.unlink()
        return target
    return tmp


def main():
    args = sys.argv[1:]
    if not args or args[0] in ("-h", "--help"):
        sys.exit(__doc__)
    ip = console_ip(args)
    verb, rest = args[0], args[1:]
    if verb == "send":
        send(ip, rest)
    elif verb == "keys" and len(rest) == 1:
        print(command(ip, f"keys {rest[0]}"))
    elif verb == "tap" and len(rest) == 2:
        print(command(ip, f"tap {int(rest[0])} {int(rest[1])}"))
    elif verb == "key" and len(rest) == 1:
        code = ord(rest[0]) if len(rest[0]) == 1 and not rest[0].isdigit() else int(rest[0], 0)
        print(command(ip, f"key {code}"))
    elif verb == "shot":
        target = Path(rest[0]) if rest else RUNS / "shot.png"
        bmp = fetch(ip, "shot")
        if not bmp:
            sys.exit("no screenshot: the game did not read its input within 15 s (busy, or waiting for a build)")
        target.parent.mkdir(parents=True, exist_ok=True)
        print(bmp_to_png(bmp, target))
    elif verb == "put" and len(rest) == 2:
        data = Path(rest[0]).read_bytes()
        with socket.create_connection((ip, COMMAND_PORT), timeout=120) as s:
            s.sendall(f"put {len(data)} {rest[1]}\n".encode() + data)
            print(s.recv(4096).decode(errors="replace").strip())
    elif verb == "get" and len(rest) in (1, 2):
        data = fetch(ip, f"get {rest[0]}", timeout=120)
        if data is None:
            sys.exit(f"no /switch/smac/{rest[0]} on the card")
        target = Path(rest[1]) if len(rest) == 2 else RUNS / Path(rest[0]).name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        print(f"{target} ({len(data)} bytes)")
    elif verb == "quit":
        print(command(ip, "quit"))
    elif verb == "log":
        lines = int(rest[0]) if rest else 30
        print("\n".join(OUT.read_text(errors="replace").splitlines()[-lines:]) if OUT.exists() else "no output yet")
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
