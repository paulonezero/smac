// The Nintendo Switch's entry point (homebrew: devkitA64, libnx). The game itself is
// opensmac_main (app/opensmac.cpp).
//
// Output: to `nxlink -s` on the computer that sent the program, else to opensmac.log in the
// per-user folder on the card. The game runs on a thread with a desktop's stack: the loader's main
// thread has far less than the engine's recursive searches get elsewhere.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <pthread.h>
#include <sys/stat.h>

#include <switch.h>

#ifdef OPENSMAC_SWITCH_DEV
#include "app/switch_dev.h"
#endif

int opensmac_main(int argc, char** argv);

// "CPU Boost" (the ini; OPENSMAC_BOOST in the environment): the system's boost mode, the CPU at
// 1785 MHz in place of 1020 with the GPU at its lowest (the game draws in software and only asks
// the GPU to show the frame). Faster by about two thirds, at the battery's cost.
void switch_cpu_boost(bool on) {
    if (const char* e = std::getenv("OPENSMAC_BOOST")) on = std::atoi(e) != 0;
    const Result rc = appletSetCpuBoostMode(on ? ApmCpuBoostMode_FastLoad : ApmCpuBoostMode_Normal);
    std::fprintf(stderr, "cpu boost %d: result %x\n", (int)on, (unsigned)rc);
}

namespace {
struct Args {
    int argc;
    char** argv;
    int result;
};
void* run(void* p) {
    Args* a = static_cast<Args*>(p);
    a->result = opensmac_main(a->argc, a->argv);
    appletSetCpuBoostMode(ApmCpuBoostMode_Normal);
    return nullptr;
}
}  // namespace

int main(int argc, char** argv) {
    socketInitializeDefault();
    if (nxlinkStdio() < 0) {
        mkdir("/switch/smac", 0777);
        mkdir("/switch/smac/user", 0777);
        std::freopen("/switch/smac/user/opensmac.log", "w", stdout);
        std::freopen("/switch/smac/user/opensmac.log", "a", stderr);
    }
    setvbuf(stderr, nullptr, _IONBF, 0);
    // A strategy game sits still for minutes: the console must not doze off under the player.
    appletSetAutoSleepDisabled(true);
    // Development: --setenv NAME=VALUE before the game's own options (there is no shell to set it).
    static std::vector<char*> kept;
    for (int i = 0; i < argc; i++) {
        if (std::strcmp(argv[i], "--setenv") == 0 && i + 1 < argc) {
            if (char* eq = std::strchr(argv[i + 1], '=')) {
                *eq = 0;
                setenv(argv[i + 1], eq + 1, 1);
            }
            i++;
            continue;
        }
        kept.push_back(argv[i]);
    }
    argc = (int)kept.size();
    kept.push_back(nullptr);
    argv = kept.data();
#ifdef OPENSMAC_SWITCH_DEV
    // A game that ended is followed by this: no game, only the wait for the next build.
    for (int i = 1; i < argc; i++)
        if (std::strcmp(argv[i], "--dev-wait") == 0) {
            const int r = switchdev::wait_for_build();
            switchdev::stop();
            socketExit();
            return r;
        }
#endif
    Args a{argc, argv, 1};
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 16u << 20);
    pthread_t thread;
    if (pthread_create(&thread, &attr, run, &a) != 0) {
        std::fprintf(stderr, "opensmac: cannot start the game thread\n");
        socketExit();
        return 1;
    }
    pthread_join(thread, nullptr);
    pthread_attr_destroy(&attr);
#ifdef OPENSMAC_SWITCH_DEV
    switchdev::stop();  // before the sockets close
#endif
    std::fflush(stdout);
    std::fflush(stderr);
    socketExit();
    return a.result;
}
