dnl Provides option to change library probes.
dnl
dnl This file provides RRA_ENABLE_REDUCED_DEPENDS, which adds the configure
dnl option --enable-reduced-depends to request that library probes assume
dnl shared libraries are in use and dependencies of libraries should not be
dnl probed.  If this option is given, the shell variable rra_reduced_depends
dnl is set to true; otherwise, it is set to false.
dnl
dnl This macro doesn't do much but is defined separately so that other macros
dnl can require it with AC_REQUIRE.
dnl
dnl Written by Russ Allbery <rra@stanford.edu>
dnl Copyright 2005, 2006, 2007
dnl     Board of Trustees, Leland Stanford Jr. University
dnl
dnl See LICENSE for licensing terms.

AC_DEFUN([RRA_ENABLE_REDUCED_DEPENDS],
[rra_reduced_depends=false
AC_ARG_ENABLE([reduced-depends],
    [AS_HELP_STRING([--enable-reduced-depends],
        [Try to minimize shared library dependencies])],
    [AS_IF([test x"$enableval" = xyes], [rra_reduced_depends=true])])])
