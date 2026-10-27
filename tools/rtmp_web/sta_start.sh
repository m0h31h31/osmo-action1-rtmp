#!/bin/sh
# Custom STA is owned solely by oa_network; system mode uses DJI scripts.
[ "$(cat /tmp/oa_network.mode 2>/dev/null)" = system ] || exit 0
exec /usr/local/share/script/sta_start.dji.sh "$@"
