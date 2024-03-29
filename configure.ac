dnl -------------------------------------------------------------------------
dnl Autoconf startup.
dnl -------------------------------------------------------------------------

AC_PREREQ([2.63])
AC_INIT([truffle], [0.2.1.GIT], [https://github.com/hroptatyr/truffle])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADER([src/config.h])
AC_CONFIG_MACRO_DIR([m4])

dnl -------------------------------------------------------------------------
dnl Local copyright notices.
dnl -------------------------------------------------------------------------

AC_COPYRIGHT([dnl
#### Configuration script for truffle.
#### Copyright (C) 2011-2020 Sebastian Freundt

### Don't edit this script!
### This script was automatically generated by the `autoconf' program
### from the file `./configure.ac'.
### To rebuild it, execute the command
###     autoreconf
])

AM_INIT_AUTOMAKE([foreign parallel-tests dist-xz color-tests subdir-objects])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

## the build chain
AC_PROG_CC([cc icc gcc])
SXE_CHECK_CC([gnu11 c11])
SXE_CHECK_CFLAGS
AC_CHECK_TOOLS([AR], [xiar ar], [false])
AC_C_BIGENDIAN
AC_PROG_RANLIB

## dfp754 helpers
SXE_CHECK_DFP754

SXE_CHECK_ASM_CORUS

## coroutine provider
AM_CONDITIONAL([USE_ASM_CORUS], [test "${use_asm_corus}" = "yes"])
if test "${use_asm_corus}" = "yes"; then
	AC_DEFINE([USE_ASM_CORUS], [1], [Whether to use asm backed coroutines])
fi

## check for yuck helper
AX_CHECK_YUCK([with_included_yuck="yes"])
AX_YUCK_SCMVER([version.mk])
AC_CONFIG_LINKS([GNUmakefile:GNUmakefile])

## set our cli-testing harness up
AX_CHECK_CLITORIS

## trivial, no special stuff needed
apps="${apps} truffle"

if test "${use_asm_corus}" = "yes"; then
	coru=" asm"
else
	coru=" setjmp/longjmp/ucontext"
fi

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([build-aux/Makefile])
AC_CONFIG_FILES([src/Makefile])
AC_CONFIG_FILES([info/Makefile])
AC_CONFIG_FILES([test/Makefile])
AC_OUTPUT

echo
echo
echo "Build summary"
echo "============="
echo
echo "Build apps:${apps}"
echo "Coroutines:${coru}"
echo

dnl configure.ac ends here
