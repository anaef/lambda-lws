#!/usr/bin/env bash

LWS_WORKSPACE="${LWS_WORKSPACE:-/workspaces/lambda-lws}"
LWS_ENV_FILE="${LWS_ENV_FILE:-${LWS_WORKSPACE}/.env}"
LWS_ENV_DEFAULTS="${LWS_ENV_DEFAULTS:-${LWS_WORKSPACE}/.env.defaults}"
LWS_COMPOSE_PROJECT_NAME="${LWS_COMPOSE_PROJECT_NAME:-lambda-lws}"
LWS_RUNTIME_HOST="${LWS_RUNTIME_HOST:-127.0.0.1}"

lws_usage_mode() {
	local command="$1"
	echo "Usage: ${command} [default|raw]" >&2
}

lws_mode() {
	case "${1:-default}" in
		default|raw)
			printf '%s\n' "${1:-default}"
			;;
		*)
			return 2
			;;
	esac
}

lws_service() {
	local mode="$1"
	local variant="${2:-normal}"

	if [[ "${variant}" == "valgrind" ]]; then
		printf 'lambda-lws-examples-%s-valgrind\n' "${mode}"
	else
		printf 'lambda-lws-examples-%s\n' "${mode}"
	fi
}

lws_require_env() {
	if [[ ! -f "${LWS_ENV_FILE}" ]]; then
		echo "Missing ${LWS_ENV_FILE}." >&2
		echo "You can copy the provided defaults and adjust the settings as needed:" >&2
		echo "  cp .env.defaults .env" >&2
		return 1
	fi
}

lws_port() {
	local port="${LWS_PORT:-}"

	if [[ -z "${port}" && -f "${LWS_ENV_FILE}" ]]; then
		port="$(
			awk -F= '
				$1 == "LWS_PORT" {
					gsub(/^[[:space:]"]+|[[:space:]"]+$/, "", $2)
					print $2
					exit
				}
			' "${LWS_ENV_FILE}"
		)"
	fi
	port="${port:-8080}"

	if [[ ! "${port}" =~ ^[0-9]+$ ]] || ((port < 1 || port > 65535)); then
		echo "Invalid LWS_PORT: ${port}" >&2
		return 1
	fi
	printf '%s\n' "${port}"
}

lws_runtime_url() {
	printf 'http://%s:%s\n' "${LWS_RUNTIME_HOST}" "$(lws_port)"
}

lws_browser_url() {
	printf 'http://localhost:%s\n' "$(lws_port)"
}

lws_compose() {
	local env_file="${LWS_ENV_FILE}"

	if [[ ! -f "${env_file}" ]]; then
		env_file="${LWS_ENV_DEFAULTS}"
	fi

	(
		cd "${LWS_WORKSPACE}"
		docker compose \
			--project-directory "${LWS_WORKSPACE}" \
			--project-name "${LWS_COMPOSE_PROJECT_NAME}" \
			--env-file "${env_file}" \
			-f docker-compose.yml \
			-f .devcontainer/docker-compose.valgrind.yml \
			"$@"
	)
}

lws_running_services() {
	lws_compose ps --status running --services
}

lws_finalize_valgrind_runtimes() {
	local container_id
	local mode
	local service
	local status

	for mode in default raw; do
		service="$(lws_service "${mode}" valgrind)"
		if ! lws_running_services | grep -Fxq "${service}"; then
			continue
		fi

		container_id="$(lws_compose ps --quiet "${service}")"
		if [[ -z "${container_id}" ]]; then
			continue
		fi

		if docker exec "${container_id}" /bin/sh -c '
			for path in /proc/[0-9]*/comm; do
				read -r name < "${path}" || continue
				case "${name}" in
					memcheck-*)
						pid="${path#/proc/}"
						pid="${pid%/comm}"
						kill -TERM "${pid}"
						attempts=300
						while kill -0 "${pid}" 2>/dev/null; do
							if [ "${attempts}" -eq 0 ]; then
								exit 11
							fi
							attempts=$((attempts - 1))
							sleep 0.1
						done
						exit 0
						;;
				esac
			done
			exit 10
		'; then
			echo "Finalized Valgrind report for ${mode} runtime."
		else
			status=$?
			if [[ ${status} -ne 10 ]]; then
				echo "Warning: could not finalize Valgrind report for ${mode} runtime." >&2
			fi
		fi
	done
}

lws_wait_for_runtime() {
	local service="$1"
	local attempt
	local runtime_url

	runtime_url="$(lws_runtime_url)"

	for attempt in {1..100}; do
		if curl --silent --show-error --output /dev/null --max-time 1 \
				"${runtime_url}/" 2>/dev/null; then
			return 0
		fi
		if ! lws_running_services | grep -Fxq "${service}"; then
			break
		fi
		sleep 0.1
	done

	echo "${service} did not become ready at ${runtime_url}." >&2
	lws_compose logs --tail 80 "${service}" >&2 || true
	return 1
}
