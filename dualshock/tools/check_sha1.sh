#!/bin/sh
# check_sha1.sh <built file> <original file> : print OK/MISMATCH, exit 1 on mismatch.
want=$(sha1sum "$2" | cut -d' ' -f1)
have=$(sha1sum "$1" | cut -d' ' -f1)
if [ "$want" = "$have" ]; then
    echo "OK        $1"
else
    echo "MISMATCH  $1 (built $have, original $want)"
    exit 1
fi
