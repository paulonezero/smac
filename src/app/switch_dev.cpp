// The Nintendo Switch's development loop (switch_dev.h).
#include "app/switch_dev.h"

#include <SDL.h>
#include <switch.h>
#include <zlib.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "platform/ui_platform.h"

namespace switchdev {
namespace {

constexpr int kLoaderPort = 28280;  // the NetLoader's, so nxlink.py sends to either
constexpr int kCommandPort = 28282;
const char kDir[] = "sdmc:/switch/smac/";
const char kSelf[] = "sdmc:/switch/smac/opensmac.nro";
const char kShot[] = "sdmc:/switch/smac/user/dev-shot.bmp";

std::atomic<bool> g_stop{false}, g_quit{false}, g_next_set{false};
std::atomic<int> g_loader_socket{-1}, g_command_socket{-1};
std::thread g_loader, g_commands;
KeysFn g_keys = nullptr;
bool g_started = false;

bool recv_all(int s, void* data, size_t size) {
    auto* p = static_cast<uint8_t*>(data);
    while (size) {
        const ssize_t n = recv(s, p, size, 0);
        if (n <= 0) return false;
        p += n;
        size -= (size_t)n;
    }
    return true;
}

bool send_all(int s, const void* data, size_t size) {
    auto* p = static_cast<const uint8_t*>(data);
    while (size) {
        const ssize_t n = send(s, p, size, 0);
        if (n <= 0) return false;
        p += n;
        size -= (size_t)n;
    }
    return true;
}

bool recv_int(int s, int32_t* v) { return recv_all(s, v, 4); }  // little-endian on both ends
bool send_int(int s, int32_t v) { return send_all(s, &v, 4); }
void reply(int s, const std::string& text) { send_all(s, text.data(), text.size()); }

int listen_on(int port) {
    const int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return -1;
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t)port);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, reinterpret_cast<sockaddr*>(&a), sizeof(a)) != 0 || listen(s, 2) != 0) {
        close(s);
        return -1;
    }
    return s;
}

void request_quit() {
    g_quit = true;
    SDL_Event e{};
    e.type = SDL_QUIT;
    SDL_PushEvent(&e);
}

// A name below the game's folder on the card, with no way out of it ("": refused).
std::string card_path(const std::string& name) {
    if (name.empty() || name.front() == '/' || name.find("..") != std::string::npos ||
        name.find(':') != std::string::npos)
        return "";
    return kDir + name;
}

// One upload, as the NetLoader takes it (nx-hbmenu common/netloader.c): int32 name length, name,
// int32 size; reply 0; a zlib stream in chunks, each after its int32 length; reply 0; int32
// argument bytes and the arguments, each ending in a NUL. True when it is the next program.
bool receive_build(int s, const sockaddr_in& from) {
    int32_t name_length = 0, file_length = 0;
    if (!recv_int(s, &name_length) || name_length <= 0 || name_length > 1024) return false;
    std::string name((size_t)name_length, '\0');
    if (!recv_all(s, name.data(), name.size()) || !recv_int(s, &file_length) || file_length <= 0) return false;
    if (name.find("..") != std::string::npos) return false;
    const std::string path = "sdmc:/switch/" + name;
    FILE* f = std::fopen(path.c_str(), "wb");
    send_int(s, f ? 0 : -1);
    if (!f) return false;
    std::fprintf(stderr, "dev: receiving %s, %d bytes\n", path.c_str(), (int)file_length);
    z_stream z{};
    inflateInit(&z);
    std::vector<uint8_t> in, out(256 * 1024);
    int status = Z_OK;
    size_t written = 0;
    while (status != Z_STREAM_END) {
        int32_t chunk = 0;
        if (!recv_int(s, &chunk) || chunk <= 0 || chunk > (1 << 20)) break;
        in.resize((size_t)chunk);
        if (!recv_all(s, in.data(), in.size())) break;
        z.next_in = in.data();
        z.avail_in = (uInt)in.size();
        do {
            z.next_out = out.data();
            z.avail_out = (uInt)out.size();
            status = inflate(&z, Z_NO_FLUSH);
            if (status != Z_OK && status != Z_STREAM_END && status != Z_BUF_ERROR) break;
            written += std::fwrite(out.data(), 1, out.size() - z.avail_out, f);
        } while (z.avail_in > 0 && status != Z_STREAM_END);
        if (status != Z_OK && status != Z_STREAM_END && status != Z_BUF_ERROR) break;
    }
    inflateEnd(&z);
    std::fclose(f);
    const bool complete = status == Z_STREAM_END && written == (size_t)file_length;
    send_int(s, complete ? 0 : -1);
    if (!complete) {
        std::fprintf(stderr, "dev: upload incomplete (%zu of %d bytes)\n", written, (int)file_length);
        return false;
    }
    int32_t args_length = 0;
    std::string args;
    if (recv_int(s, &args_length) && args_length > 0 && args_length < 8192) {
        args.resize((size_t)args_length);
        if (!recv_all(s, args.data(), args.size())) args.clear();
    }
    std::string command = "\"" + path + "\"";
    for (size_t i = 0; i < args.size();) {
        const size_t end = args.find('\0', i);
        const std::string a = args.substr(i, end == std::string::npos ? std::string::npos : end - i);
        if (!a.empty()) command += " \"" + a + "\"";
        if (end == std::string::npos) break;
        i = end + 1;
    }
    // The sender's address, as the Homebrew Menu adds it: nxlinkStdio sends the output there.
    char host[32];
    std::snprintf(host, sizeof(host), " %08x_NXLINK_", (unsigned)from.sin_addr.s_addr);
    command += host;
    if (!envHasNextLoad()) {
        std::fprintf(stderr, "dev: this loader cannot start a next program: start it by hand\n");
        return false;
    }
    const Result rc = envSetNextLoad(path.c_str(), command.c_str());
    std::fprintf(stderr, "dev: next: %s (0x%x); quitting into it\n", command.c_str(), (unsigned)rc);
    if (R_SUCCEEDED(rc)) g_next_set = true;
    return R_SUCCEEDED(rc);
}

void loader_thread() {
    const int listener = listen_on(kLoaderPort);
    if (listener < 0) {
        std::fprintf(stderr, "dev: cannot listen on %d\n", kLoaderPort);
        return;
    }
    g_loader_socket = listener;
    while (!g_stop) {
        sockaddr_in from{};
        socklen_t length = sizeof(from);
        const int s = accept(listener, reinterpret_cast<sockaddr*>(&from), &length);
        if (s < 0) {
            if (g_stop) break;
            svcSleepThread(100'000'000);
            continue;
        }
        const bool next = receive_build(s, from);
        shutdown(s, SHUT_RDWR);
        close(s);
        if (next) {
            request_quit();
            break;
        }
    }
    if (g_loader_socket.exchange(-1) >= 0) close(listener);
}

void push_mouse(uint32_t type, int x, int y) {
    SDL_Event e{};
    e.type = type;
    if (type == SDL_MOUSEMOTION) {
        e.motion.which = SDL_TOUCH_MOUSEID;
        e.motion.x = x;
        e.motion.y = y;
    } else {
        e.button.which = SDL_TOUCH_MOUSEID;
        e.button.button = SDL_BUTTON_LEFT;
        e.button.state = type == SDL_MOUSEBUTTONDOWN ? SDL_PRESSED : SDL_RELEASED;
        e.button.clicks = 1;
        e.button.x = x;
        e.button.y = y;
    }
    SDL_PushEvent(&e);
}

long file_size(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 ? (long)st.st_size : -1;
}

// A file's size (int32; -1: none), then its bytes.
void send_file(int s, const char* path) {
    FILE* f = std::fopen(path, "rb");
    const long size = f ? file_size(path) : -1;
    send_int(s, (int32_t)size);
    if (!f) return;
    std::vector<uint8_t> buf(64 * 1024);
    size_t n;
    while ((n = std::fread(buf.data(), 1, buf.size(), f)) > 0)
        if (!send_all(s, buf.data(), n)) break;
    std::fclose(f);
}

std::string read_line(int s) {
    std::string line;
    char c = 0;
    while (line.size() < 4096 && recv(s, &c, 1, 0) == 1 && c != '\n') line.push_back(c);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return line;
}

void handle(int s) {
    const std::string line = read_line(s);
    int x = 0, y = 0, size = 0, at = 0;
    if (line.rfind("keys ", 0) == 0) {
        reply(s, g_keys && g_keys(line.substr(5)) ? "ok\n" : "unknown key (or no game)\n");
    } else if (line == "shot") {
        // The game's thread writes it when it next reads its input: waited for here.
        std::remove(kShot);
        bool ok = g_keys && g_keys("SHOT:dev-shot.bmp");
        long last = -2;
        for (int i = 0; ok && i < 150 && !g_stop; i++) {
            svcSleepThread(100'000'000);
            const long now = file_size(kShot);
            if (now > 0 && now == last) break;
            last = now;
            if (i == 149) ok = false;
        }
        if (ok) send_file(s, kShot);
        else send_int(s, -1);
    } else if (std::sscanf(line.c_str(), "put %d %n", &size, &at) == 1 && at > 0 && size >= 0) {
        const std::string path = card_path(line.substr((size_t)at));
        FILE* f = path.empty() ? nullptr : std::fopen(path.c_str(), "wb");
        std::vector<uint8_t> in(64 * 1024);
        bool ok = f != nullptr;
        for (int left = size; left > 0;) {
            const size_t n = std::min<size_t>(in.size(), (size_t)left);
            if (!recv_all(s, in.data(), n)) {
                ok = false;
                break;
            }
            ok = ok && std::fwrite(in.data(), 1, n, f) == n;
            left -= (int)n;
        }
        if (f) ok = std::fclose(f) == 0 && ok;
        std::fprintf(stderr, "dev: put %s, %d bytes: %s\n", path.c_str(), size, ok ? "ok" : "failed");
        reply(s, ok ? "ok\n" : "failed\n");
    } else if (line.rfind("get ", 0) == 0) {
        const std::string path = card_path(line.substr(4));
        if (path.empty()) send_int(s, -1);
        else send_file(s, path.c_str());
    } else if (std::sscanf(line.c_str(), "tap %d %d", &x, &y) == 2) {
        push_mouse(SDL_MOUSEMOTION, x, y);
        push_mouse(SDL_MOUSEBUTTONDOWN, x, y);
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        push_mouse(SDL_MOUSEBUTTONUP, x, y);
        reply(s, "ok\n");
    } else if (std::sscanf(line.c_str(), "key %d", &x) == 1) {
        platform::key_inject(x);
        reply(s, "ok\n");
    } else if (line == "quit") {
        request_quit();
        reply(s, "ok\n");
    } else {
        reply(s, "commands: keys TOKENS | tap X Y | key CODE | shot | put SIZE NAME | get NAME | quit\n");
    }
}

void command_thread() {
    const int listener = listen_on(kCommandPort);
    if (listener < 0) {
        std::fprintf(stderr, "dev: cannot listen on %d\n", kCommandPort);
        return;
    }
    g_command_socket = listener;
    while (!g_stop) {
        const int s = accept(listener, nullptr, nullptr);
        if (s < 0) {
            if (g_stop) break;
            svcSleepThread(100'000'000);
            continue;
        }
        handle(s);
        shutdown(s, SHUT_RDWR);
        close(s);
    }
    if (g_command_socket.exchange(-1) >= 0) close(listener);
}

}  // namespace

void start(KeysFn keys) {
    g_keys = keys;
    if (g_started) return;
    if (__nxlink_host.s_addr == 0) return;  // started from the console's menus: no listeners
    g_started = true;
    g_loader = std::thread(loader_thread);
    g_commands = std::thread(command_thread);
    std::fprintf(stderr, "dev: new builds on TCP %d, commands on TCP %d\n", kLoaderPort, kCommandPort);
}

void stop() {
    if (!g_started) return;
    g_stop = true;
    for (std::atomic<int>* socket : {&g_loader_socket, &g_command_socket})
        if (const int s = socket->exchange(-1); s >= 0) {
            shutdown(s, SHUT_RDWR);
            close(s);
        }
    if (g_loader.joinable()) g_loader.join();
    if (g_commands.joinable()) g_commands.join();
    // A game that ended by itself (not one replaced by a build): a copy that waits for the next
    // build follows, so the loop needs no hand. HOME, then Close, ends it for good.
    if (!g_next_set && envHasNextLoad()) {
        char command[128];
        std::snprintf(command, sizeof(command), "\"%s\" --dev-wait %08x_NXLINK_", kSelf, (unsigned)__nxlink_host.s_addr);
        envSetNextLoad(kSelf, command);
    }
}

int wait_for_build() {
    start(nullptr);
    std::fprintf(stderr, "dev: waiting for a new build\n");
    while (!g_quit && g_started && appletMainLoop()) svcSleepThread(100'000'000);
    return 0;
}

}  // namespace switchdev
