#!/bin/bash

usage() {
	echo "Usage: $0 <server> [<mode>]"
	echo "  mode can be one of:"
	echo "    NORMAL: Everything except tools (default)"
	echo "    TOOLS: Only the tools"
	echo "    FULL: Everything"
	exit 1
}

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
	usage
fi

server=$1

DEPLOY_NORMAL=1
DEPLOY_TOOLS=0

if [ "$#" -eq 2 ]; then
	if [ "$2" == "TOOLS" ]; then
		DEPLOY_NORMAL=0
		DEPLOY_TOOLS=1
	elif [ "$2" == "FULL" ]; then
		DEPLOY_NORMAL=1
		DEPLOY_TOOLS=1
	elif [ "$2" != "NORMAL" ]; then
		echo "Mode $2 is invalid."
		usage
	fi
fi

ssh $server rm -r "~/ARHE"
ssh $server mkdir "~/ARHE"

if [ "$DEPLOY_NORMAL" -eq 1 ]; then
	scp -r ARHE/Dockerfile "$server:~/ARHE/"
	scp -r ARHE/README.md "$server:~/ARHE/"
	scp -r ARHE/components "$server:~/ARHE/"
	scp -r ARHE/conf.cfg "$server:~/ARHE/"
	scp -r ARHE/docker-run.sh "$server:~/ARHE/"
	scp -r ARHE/main.sh "$server:~/ARHE/"
	scp -r ARHE/run.sh "$server:~/ARHE/"
fi

if [ "$DEPLOY_TOOLS" -eq 1 ]; then
	scp -r ARHE/Tools "$server:~/ARHE/"
fi
