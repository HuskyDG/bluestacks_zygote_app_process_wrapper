#include <unistd.h>
#include <sys/mount.h>
#include <android/log.h>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <sched.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <signal.h>

using namespace std;

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "exec_wrapper", __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "exec_wrapper", __VA_ARGS__)

char buf[256];

void unmount_orig(int num) {
    LOGD("zygote died");
    umount2(buf, MNT_DETACH);
    exit(num);
}

int parse_ppid(int pid = -1) {
    char path[32];
    int ppid;
    if (pid > 0) sprintf(path, "/proc/%d/stat", pid);
    else sprintf(path, "/proc/self/stat");
    FILE *pfstat = fopen(path, "re");
    if (!pfstat)
        return -1;
    // PID COMM STATE PPID .....
    fscanf(pfstat, "%*d %*s %*c %d", &ppid);
    fclose(pfstat);
    return ppid;
}

static bool is_valid_environment_variable(const char* name) {
    // According to the kernel source, by default the kernel uses 32*PAGE_SIZE
    // as the maximum size for an environment variable definition.
    const int MAX_ENV_LEN = 32*4096;

    if (name == nullptr) {
        return false;
    }

    // Parse the string, looking for the first '=' there, and its size.
    int pos = 0;
    int first_equal_pos = -1;
    while (pos < MAX_ENV_LEN) {
        if (name[pos] == '\0') {
            break;
        }
        if (name[pos] == '=' && first_equal_pos < 0) {
            first_equal_pos = pos;
        }
        pos++;
    }

    // Check that it's smaller than MAX_ENV_LEN (to detect non-zero terminated strings).
    if (pos >= MAX_ENV_LEN) {
        return false;
    }

    // Check that it contains at least one equal sign that is not the first character
    if (first_equal_pos < 1) {
        return false;
    }

    return true;
}

static const char* env_match(const char* envstr, const char* name) {
    size_t i = 0;

    while (envstr[i] == name[i] && name[i] != '\0') {
        ++i;
    }

    if (name[i] == '\0' && envstr[i] == '=') {
        return envstr + i + 1;
    }

    return nullptr;
}

static bool is_unsafe_environment_variable(const char* name) {
    // None of these should be allowed when the AT_SECURE auxv
    // flag is set. This flag is set to inform userspace that a
    // security transition has occurred, for example, as a result
    // of executing a setuid program or the result of an SELinux
    // security transition.
    static constexpr const char* UNSAFE_VARIABLE_NAMES[] = {
            "ANDROID_DNS_MODE",
            "GCONV_PATH",
            "GETCONF_DIR",
            "HOSTALIASES",
            "JE_MALLOC_CONF",
            "LD_AOUT_LIBRARY_PATH",
            "LD_AOUT_PRELOAD",
            "LD_AUDIT",
            "LD_CONFIG_FILE",
            "LD_DEBUG",
            "LD_DEBUG_OUTPUT",
            "LD_DYNAMIC_WEAK",
            "LD_LIBRARY_PATH",
            "LD_ORIGIN_PATH",
//            "LD_PRELOAD",
            "LD_PROFILE",
            "LD_SHOW_AUXV",
            "LD_USE_LOAD_BIAS",
            "LIBC_DEBUG_MALLOC_OPTIONS",
            "LIBC_HOOKS_ENABLE",
            "LOCALDOMAIN",
            "LOCPATH",
            "MALLOC_CHECK_",
            "MALLOC_CONF",
            "MALLOC_TRACE",
            "NIS_PATH",
            "NLSPATH",
            "RESOLV_HOST_CONF",
            "RES_OPTIONS",
            "SCUDO_OPTIONS",
            "TMPDIR",
            "TZDIR",
    };
    for (const auto& unsafe_variable_name : UNSAFE_VARIABLE_NAMES) {
        if (env_match(name, unsafe_variable_name) != nullptr) {
            return true;
        }
    }
    return false;
}

static void sanitize_environment_variables(char** env) {
    char** src = env;
    char** dst = env;
    for (; src[0] != nullptr; ++src) {
        if (!is_valid_environment_variable(src[0])) {
            continue;
        }
        // Remove various unsafe environment variables if we're loading a setuid program.
        if (is_unsafe_environment_variable(src[0])) {
            continue;
        }
        dst[0] = src[0];
        ++dst;
    }
    dst[0] = nullptr;
}


int main(int argc, char **argv) {
    LOGD("exec wrapper start");
    bool zygote = false;
    memset(buf, 0, sizeof(buf));
    if (readlink("/proc/self/exe", buf, sizeof(buf)) < 0) {
        LOGE("stat:%s", strerror(errno));
        exit(-1);
    }
    for (int i=0;i<argc;i++) {
    	if (argv[i] == "--zygote"sv) {
    		zygote = (parse_ppid() == 1);
    		break;
    	}
    }
    char *_realpath = realpath(buf, nullptr);
    if (_realpath == nullptr) _realpath = strdup(buf);
    string bak = string(_realpath) + ".orig";
    delete _realpath;
    if (zygote && getuid() == 0 && !mount(bak.data(), buf, nullptr, MS_BIND, nullptr)) {
        if (fork() == 0){
            // handle zygote restart
            LOGD("Fork handle");
            signal(SIGUSR1, unmount_orig);
            signal(SIGTERM, SIG_IGN);
            prctl(PR_SET_PDEATHSIG, SIGUSR1);
            while (true) pause();
            _exit(1);
        }
        int fd = open("/proc/self/attr/current", O_WRONLY);
        if (fd >= 0){
            if (write(fd, "u:r:init:s0", sizeof("u:r:init:s0")) > 0)
                LOGD("switch to init context");
            close(fd);
        }
        sanitize_environment_variables(environ);
        LOGD("exec:%s", buf);
        if (execve(buf, argv, environ)) {
            LOGE("execve:%s", strerror(errno));
        }
    }
    LOGD("exec:%s", buf);
    if (execve(bak.data(), argv, environ)) {
        LOGE("execve:%s", strerror(errno));
    }
    return -1;
}
