## app_process wrapper
app_process wrapper for Bluestacks to run Zygisk provided by [Magisk Delta](http://huskydg.github.io/magisk-files)

> Update: Bluestacks Android 9+ doesn't need this workaround. But Ptrace/Logcat MagiskHide and SuList will not work!

### How does it work?
- Bluestacks' `zygote` does not set to `u:r:zygote:s0`, which causes Magisk to reject it from socket and Zygisk failed to inject zygote. The wrapper set selinux context to `u:r:zygote:s0` before executing real `app_process` so Magisk will inject Zygisk into zygote.

### Installation
- Download in [releases page](https://github.com/HuskyDG/app_process_wrapper/releases) and install it from Magisk app


