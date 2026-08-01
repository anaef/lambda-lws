#!/usr/bin/env bash
set -euo pipefail

exec valgrind \
	--error-exitcode=99 \
	--errors-for-leak-kinds=definite,indirect,possible \
	--leak-check=full \
	--read-var-info=yes \
	--show-leak-kinds=definite,indirect,possible \
	--track-fds=yes \
	--track-origins=yes \
	/var/runtime/bootstrap.valgrind "$@"
