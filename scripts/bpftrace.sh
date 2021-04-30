#!/usr/bin/env bash
if [ $# -lt 2 ]; then
	echo "usage: $0 <pid> <script>"
	exit 1
fi
SCRIPTS_DIR=$(readlink -f $(dirname $0))
BIN_PATH=$(readlink -f /proc/$1/exe)
BPF_SCRIPT=$(sed "s#__EXE__#${BIN_PATH}#g" "${@:2}" | sed "s#__PID__#${1}#g")
bpftrace -p $1 -e "$BPF_SCRIPT"
