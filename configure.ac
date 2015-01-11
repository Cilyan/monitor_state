AC_PREREQ([2.69])
AC_INIT(monitor_state, 0.2, gaknar@gmail.com)
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/monitor_state.c])

# Checks for programs.
AC_PROG_CC
PKG_PROG_PKG_CONFIG([0.28])

# Checks for libraries.
PKG_CHECK_MODULES([DBUS], [dbus-1])

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([alarm memset])

AC_OUTPUT(Makefile src/Makefile)
