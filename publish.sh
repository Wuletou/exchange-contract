#!/usr/bin/env bash

TX_FILENAME=.tx

ARGUMENT_LIST=(
	"ACCOUNT"
	"NODEOS_URL"
	"KEOSD_URL"
	"ABI_FILENAME"
	"WASM_FILENAME"
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
	echo "--ABI_FILENAME - filename of abi"
	echo "--WASM_FILENAME - filename of wasm"
	echo "Example:"
	echo "./publish.sh --ACCOUNT wulettest --NODEOS_URL https://api-wulet.unblocking.io/ --KEOSD_URL http://127.0.0.1:8900/ --ABI_FILENAME build/contract.abi --WASM_FILENAME build/contract.wasm"
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

function combine_transaction() {
	ABI=$(cleos -u ${NODEOS_URL} set abi ${ACCOUNT} ${ABI_FILENAME} -jd 2> /dev/null | grep \"data\" | cut -c 16- | rev | cut -c 2- | rev)
    BYTECODE=$(cat ${WASM_FILENAME} | xxd -p | tr -d '\n')
    SET_CODE_ACTION=$'{"account": "eosio", "name": "setcode", "authorization": [{"actor": "'${ACCOUNT}$'", "permission": "active"}], "data": {"account": "'${ACCOUNT}$'", "vmtype": 0, "vmversion": 0, "code": "'${BYTECODE}$'"}}'
    SET_ABI_ACTION=$'{"account": "eosio", "name": "setabi", "authorization": [{"actor": "'${ACCOUNT}$'", "permission": "active"}], "data": "'${ABI}$'"}'
    TX=$'{"actions": ['${SET_CODE_ACTION}$','${SET_ABI_ACTION}$']}'
    echo ${TX} > ${TX_FILENAME}
}

function send_transaction() {
    cleos -u ${NODEOS_URL} --wallet-url ${KEOSD_URL} push transaction ${TX_FILENAME}
    rm ${TX_FILENAME}
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

		--ABI_FILENAME)
			ABI_FILENAME=$2
			shift 2
			;;

		--WASM_FILENAME)
			WASM_FILENAME=$2
			shift 2
			;;
		*)
			break
			;;
	esac
done

check_input
combine_transaction
send_transaction