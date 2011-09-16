#!/bin/sh

TRUFFLE="../src/truffle"
TEST="${srcdir}/"$(basename "${1}" .truftest)

new=$(mktemp)

md5()
{
	cat "${1}" | md5sum
}

"${TRUFFLE}" --series "${TEST}.series" --schema "${TEST}.schema" >"${new}" 2>&1

if test "$(md5 ${new})" = "$(md5 ${TEST}.result)"; then
	i=0
else
	i=1
	echo "${TRUFFLE}" --series "${TEST}.series" --schema "${TEST}.schema"
	cat "${new}"
	echo "  vs"
	cat "${TEST}.result"
fi

## -f tests
"${TRUFFLE}" -f --series "${TEST}.series" --schema "${TEST}.schema" >"${new}" 2>&1

if test "$(md5 ${new})" = "$(md5 ${TEST}.-f.result)"; then
	i=0
else
	i=1
	echo "${TRUFFLE}" -f --series "${TEST}.series" --schema "${TEST}.schema"
	cat "${new}"
	echo "  vs"
	cat "${TEST}.-f.result"
fi

rm -f -- "${new}"
exit ${i}
