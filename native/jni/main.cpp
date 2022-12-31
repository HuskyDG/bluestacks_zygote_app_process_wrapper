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

using namespace std;

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "exec_wrapper", __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "exec_wrapper", __VA_ARGS__)

int main(int argc, char **argv) {
    LOGD("exec wrapper start");
    char buf[256];
    memset(buf, 0, sizeof(buf));
    if (readlink("/proc/self/exe", buf, sizeof(buf)) < 0) {
        LOGE("stat:%s", strerror(errno));
        exit(-1);
    }
    int fd = open("/proc/self/attr/current", O_WRONLY);
    if (fd >= 0){
        if (write(fd, "u:r:zygote:s0", sizeof("u:r:zygote:s0")) > 0)
            LOGD("switch to zygote context");
        close(fd);
    }
    LOGD("exe:%s", buf);
    char *_realpath = realpath(argv[0], nullptr);
    if (_realpath == nullptr) _realpath = strdup(argv[0]);
    string bak = string(_realpath) + ".orig";
    delete _realpath;
    if (execve(bak.data(), argv, environ)) {
        LOGE("execve:%s", strerror(errno));
    }
    return -1;
}
