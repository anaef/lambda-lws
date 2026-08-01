#!/usr/bin/env bash
set -euo pipefail

workspace="${LWS_WORKSPACE:-/workspaces/lambda-lws}"

if [[ ! -f "${workspace}/.env" ]]; then
	echo "No .env file found."
	echo "You can create one when needed and adjust the settings as needed: cp .env.defaults .env"
fi

echo "Lambda LWS development container is ready."
