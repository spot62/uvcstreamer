#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
#

AC_PREREQ([2.69])
AC_INIT(uvcstreamer, 0.1.0, spot62@yandex.ru)
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/uvcstreamer.c])
#AC_CONFIG_HEADERS([config.h])
AC_OUTPUT(Makefile src/Makefile)

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB([pthread], [pthread_create], [], [AC_MSG_ERROR([pthread_create was not found in libpthread])])
AC_CHECK_LIB([jpeg], [jpeg_start_compress], [], [AC_MSG_ERROR([jpeg_mem_src was not found in libjpeg])])
# for jpeg softencoder off:
#AC_CHECK_LIB(jpeg, jpeg_start_compress,
#        [AC_CHECK_HEADERS([jpeglib.h],
#         AM_CONDITIONAL(HAVE_JPEG, true)
#         AC_DEFINE(HAVE_JPEG,1,[Have libjpeg]),
#         AM_CONDITIONAL(HAVE_JPEG, false))],
#         AM_CONDITIONAL(HAVE_JPEG, false))

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h netdb.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_CHECK_FUNCS([bzero memmove memset munmap select socket strchr strdup strerror strndup strspn strstr strtol])

AC_OUTPUT
