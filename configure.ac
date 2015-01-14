AC_INIT([jsonrpc-c], [0.1], [hmng@farol.pt])
AC_CONFIG_SRCDIR([src/jsonrpc-c.c])

AM_PROG_AR
LT_INIT
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_PROG_CC
AM_PROG_CC_C_O

AC_SUBST([LIBEV_CFLAGS])
AC_SUBST([LIBEV_LIBS])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
 Makefile
 include/Makefile
 src/Makefile
 example/Makefile
])
AC_OUTPUT

