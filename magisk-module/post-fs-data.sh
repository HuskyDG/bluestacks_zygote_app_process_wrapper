MODDIR="${0%/*}"
MAGISKTMP="$(magisk --path)" || MAGISKTMP=/sbin
MIRROR="$MAGISKTMP/.magisk/mirror"
for bit in 32 64; do
    if [ -x /system/bin/app_process$bit ]; then
        touch "$MODDIR/system/bin/app_process$bit.orig"
        mount --bind "$MIRROR/system/bin/app_process$bit" "$MIRROR/$MODDIR/system/bin/app_process$bit.orig"
    fi
done
