#!/usr/bin/env bash

ARGUMENT_LIST=(
	"ACCOUNT"
	"NODEOS_URL"
	"KEOSD_URL"
	"CONTRACT_DIR",
	"OWNER_PUB_KEY"
)

opts=$(getopt \
	--longoptions "$(printf "%s:," "${ARGUMENT_LIST[@]}")" \
	--name "$(basename "$0")" \
	--options "" \
	-- "$@"
)

function usage() {
	echo "Usage: ./configure.sh [ARGS]"
	echo "--ACCOUNT - account of contract to deploy"
	echo "--NODEOS_URL - the http/https URL where nodeos is running"
	echo "--KEOSD_URL - the http/https URL where keosd is running"
	echo "--CONTRACT_DIR - the path containing the .wasm and .abi"
	echo "--OWNER_PUB_KEY - exchange account public owner key"
	echo "Example:"
	echo "./publish.sh --ACCOUNT wuletexchacc --NODEOS_URL https://api-wulet.unblocking.io/ --KEOSD_URL http://127.0.0.1:8900/ --CONTRACT_DIR ../exchange --OWNER_PUB_KEY EOS5jgGJpxrCpLh4rryzEfw7VPRLmY5ydcXqDqcNVBSkMrAumyHG4"
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
		exit 1
	fi
}

function send_transaction() {
    res="$( cleos -u ${NODEOS_URL} --wallet-url ${KEOSD_URL} set contract ${ACCOUNT} ${CONTRACT_DIR} 2>&1 )"
    exit_code=$?
    echo "exit_code: "${exit_code}
    echo ${res}
    if [[ exit_code != 0 && !($res = *"Contract is already running this version of code"*) && !($res = *"existing abi"*) && !($res = *"existing code"*) ]]; then
        exit 1
    fi
}

function set_eosio_code_permission() {
    res="$( cleos -u ${NODEOS_URL} --wallet-url ${KEOSD_URL} set account permission ${ACCOUNT} active \'{\"threshold\":1,\"keys\":[{\"key\":\"${OWNER_PUB_KEY}\",\"weight\":1}],\"accounts\":[{\"permission\":{\"actor\":\"${ACCOUNT}\",\"permission\":\"eosio.code\"},\"weight\":1}]}\' owner )"
    exit_code=$?
    echo "exit_code: "${exit_code}
    echo ${res}
}

eval set --$opts
while [[ $# -gt 0 ]]; do
	case "$1" in
		--ACCOUNT)
			ACCOUNT=$2
			shift 2
			;;

		--NODEOS_URL)
			NODEOS_URL=$2
			shift 2
			;;

		--KEOSD_URL)
			KEOSD_URL=$2
			shift 2
			;;

		--CONTRACT_DIR)
			CONTRACT_DIR=$2
			shift 2
			;;

		--OWNER_PUB_KEY)
			OWNER_PUB_KEY=$2
			shift 2
			;;
		*)
			break
			;;
	esac
done

check_input
send_transaction
set_eosio_code_permission