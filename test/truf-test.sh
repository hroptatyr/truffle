#!/bin/sh

CLINE=$(getopt -o _ --long builddir:,srcdir:,hash: -n "${0}" -- "${@}")
eval set -- "${CLINE}"
while true; do
	case "${1}" in
	"--builddir")
		builddir="${2}"
		shift 2
		;;
	"--srcdir")
		srcdir="${2}"
		shift 2
		;;
	"--hash")
		hash="${2}"
		shift 2
		;;
	--)
		shift
		break
		;;
	*)
		echo "could not parse options" >&2
		exit 1
		;;
	esac
done

## setup
fail=0
tool_stdout=$(mktemp)
tool_stderr=$(mktemp)
pwd=$(pwd)

## source the check
. "${1}" || fail=1

myexit()
{
	rm -f -- "${stdin}" "${stdout}" "${stderr}" "${tool_stdout}" "${tool_stderr}"
	cd "${pwd}"
	exit ${1:-1}
}

## check if everything's set
if test -z "${TOOL}"; then
	echo "variable \${TOOL} not set" >&2
	myexit 1
fi

## set finals
if test -z "${srcdir}"; then
	srcdir=$(dirname "${0}")
fi
if test -x "${builddir}/${TOOL}"; then
	TOOL=$(readlink -e "${builddir}/${TOOL}")
fi

cd "${srcdir}"
eval "${TOOL}" "${CMDLINE}" \
	< "${stdin:-/dev/null}" \
	> "${tool_stdout}" 2> "${tool_stderr}" || myexit 1

if test -r "${stdout}"; then
	diff -u "${stdout}" "${tool_stdout}" || fail=1
fi
if test -r "${stderr}"; then
	diff -u "${stderr}" "${tool_stderr}" || fail=1
fi

myexit ${fail}

## truf-test.sh ends here
