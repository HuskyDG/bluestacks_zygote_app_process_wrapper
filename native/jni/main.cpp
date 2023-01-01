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
#include <sys/types.h>
#include <sys/wait.h>

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
        int fork_a = fork();
        if (fork_a == 0){
            execl("/system/bin/magisk", "magisk", "--daemon", (char*)0);
            _exit(-1);
        } else if (fork_a > 0) {
            waitpid(fork_a, 0, 0);
        }
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
            if (write(fd, "u:r:zygote:s0", sizeof("u:r:zygote:s0")) > 0)
                LOGD("switch to zygote context");
            close(fd);
        }
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
