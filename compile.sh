#!/usr/bin/env bash
set -e

CONFIG_FILENAME="config.h"
BUILD_DIR=build
CPP_FILENAME=exchange.cpp
ABI_FILENAME=exchange.abi
WAST_FILENAME=exchange.wast

ARGUMENT_LIST=(
	"LT_ACCOUNT"
	"WU_ACCOUNT"
	"WU_SYMBOL"
	"WU_DECIMALS"
)

opts=$(getopt \
	--longoptions "$(printf "%s:," "${ARGUMENT_LIST[@]}")" \
	--name "$(basename "$0")" \
	--options "" \
	-- "$@"
)

function usage() {
	echo "Usage: ./compile.sh [ARGS]"
	echo "--LT_ACCOUNT - account of Loyalty Token contract"
	echo "--WU_ACCOUNT - account of WU Token contract"
	echo "--WU_SYMBOL - WU token symbol"
	echo "--WU_DECIMALS - WU token decimals"
	echo "Example:"
	echo "./compile.sh --LT_ACCOUNT lt.deployer --WU_ACCOUNT wu.deployer --WU_SYMBOL WU --WU_DECIMALS 4"
}

function check_input() {
	for field in ${ARGUMENT_LIST[@]}; do
		if [[ -z "${!field}" ]]; then
			>&2 echo "missing --$field"
			err=1
		fi
	done
	if [[ -n "$err" ]]; then
		usage
		exit 0
	fi
}

function out() {
	echo "#define LT_ACCOUNT $LT_ACCOUNT" > ${CONFIG_FILENAME}
	echo "#define WU_ACCOUNT $WU_ACCOUNT" >> ${CONFIG_FILENAME}
	echo "#define WU_SYMBOL $WU_SYMBOL" >> ${CONFIG_FILENAME}
	echo "#define WU_DECIMALS $WU_DECIMALS" >> ${CONFIG_FILENAME}
}

function compile() {
	mv ${BUILD_DIR}/${ABI_FILENAME} .
	rm -rf ${BUILD_DIR}
	mkdir ${BUILD_DIR}
	mv ${ABI_FILENAME} ${BUILD_DIR}
	eosiocpp -o ${BUILD_DIR}/${WAST_FILENAME} ${CPP_FILENAME}
}

eval set --$opts
while [[ $# -gt 0 ]]; do
	case "$1" in
		--LT_ACCOUNT)
			LT_ACCOUNT=$2
			shift 2
			;;

		--WU_ACCOUNT)
			WU_ACCOUNT=$2
			shift 2
			;;

		--WU_SYMBOL)
			WU_SYMBOL=$2
			shift 2
			;;

		--WU_DECIMALS)
			WU_DECIMALS=$2
			shift 2
			;;
		*)
			break
			;;
	esac
done

check_input
out
compile
