dnl $Id$
dnl config.m4 for extension parsekit

PHP_ARG_ENABLE(parsekit, whether to enable parsekit support,
[  --enable-parsekit           Enable parsekit support])

if test "$PHP_PARSEKIT" != "no"; then
  AC_DEFINE(HAVE_PARSEKIT, 1, [ Parser Toolkit ])
  PHP_NEW_EXTENSION(parsekit, parsekit.c, $ext_shared)
fi
