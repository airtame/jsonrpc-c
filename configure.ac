AC_INIT([jsonrpc-c], [0.1], [hmng@farol.pt])
AC_CONFIG_SRCDIR([src/jsonrpc-c.c])

LT_INIT
AM_INIT_AUTOMAKE([-Wall foreign])
AC_PROG_CC
AM_PROG_CC_C_O

AC_LIBTOOL_WIN32_DLL
AM_PROG_LIBTOOL

AC_SUBST([LIBEV_CFLAGS])
AC_SUBST([LIBEV_LIBS])

AM_CONDITIONAL([MINGW32], [test "x$host_os" = xmingw32])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
 Makefile
 include/Makefile
 src/Makefile
 example/Makefile
])
AC_OUTPUT

