AC_INIT([magnetwm], [0.0.1], [marcin@nerdy.cat])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_PROG_CC
AC_PROG_YACC

AC_CHECK_FILE([/usr/X11R6/include], [CPPFLAGS="$CPPFLAGS -I/usr/X11R6/include"])
AC_CHECK_FILE([/usr/X11R6/lib], [LDFLAGS="$LDFLAGS -L/usr/X11R6/lib"])

AC_CHECK_FUNC(strtonum, [AC_DEFINE(HAVE_STRTONUM)])

AC_CHECK_HEADERS(X11/Xlib.h,LIBS="${LIBS} -lX11")
AC_CHECK_HEADERS(X11/extensions/Xrandr.h,LIBS="${LIBS} -lXrandr")
AC_CHECK_HEADERS(X11/extensions/Xrender.h,LIBS="${LIBS} -lXrender")
PKG_CHECK_MODULES(XFT, xft, [
	LIBS="$LIBS $XFT_LIBS"
	CPPFLAGS="$XFT_CFLAGS $CPPFLAGS"
], AC_MSG_ERROR("Xft not found"))
AC_CHECK_HEADER(X11/Xft/Xft.h,,AC_MSG_ERROR("Xft.h not found"))

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
