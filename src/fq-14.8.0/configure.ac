#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.71])
AC_INIT([fq],[14.8.1],[gwisang.choi@gmail.com])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign])
LT_INIT
AC_PROG_RANLIB
AC_STRUCT_ST_BLOCKS
AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB

# Checks for libraries.
# FIXME: Replace `main' with a function in `-ldl':
AC_CHECK_LIB([dl], [main])
# FIXME: Replace `main' with a function in `-lm':
AC_CHECK_LIB([m], [main])
# FIXME: Replace `main' with a function in `-lnsl':
AC_CHECK_LIB([nsl], [main])
# FIXME: Replace `main' with a function in `-lrt':
AC_CHECK_LIB([rt], [main])
# FIXME: Replace `main' with a function in `-lsocket':
AC_CHECK_LIB([socket], [main])
AC_CHECK_LIB([curses], [main])
AC_CHECK_LIB([pthread], [main])
AC_CHECK_LIB([socket], [main])

AC_HEADER_RESOLV
AC_HEADER_MAJOR


# Checks for header files.
# Use it when you want to stop with error message, if some library does not exist.
# AC_CHECK_HEADER([curses.h],
#                [AC_DEFINE([HAVE_CURSES_H])],
#                [AC_MSG_ERROR([Sorry, can't do anything for you])])

AC_CHECK_HEADERS([snprintf.h stdlib.h string.h pthread.h errno.h resolv.h kstat.h dirent.h poll.h sys/poll.h dos.h sys/types.h ctype.h features.h setjmp.h endian.h sys/param.h netinet/in_systm.h sys/stat.h inet.h arpa/inet.h arpa/nameser.h time.h assert.h fcntl.h float.h limits.h locale.h strops.h varargs.h netdb.h stddef.h stdint.h strings.h sys/sockio.h sys/file.h sys/ioctl.h sys/socket.h sys/time.h sys/timeb.h sys/mkdev.h sys/ytsname.h termio.h unistd.h wchar.h signal.h math.h sys/mman.h sys/shm.h sys/msg.h sys/ipc.h sys/sem.h stropts.h sys/varargs.h curses.h stdarg.h sys/select.h sys/resource.h in.h netinet/in.h mqueue.h sys/wait.h link.h sys/pstat.h sys/dk.h ncurses.h xcurses.h syslog.h termios.h utime.h libgen.h netinet/ip_icmp.h net/if.h malloc.h sys/filio.h sys/socketvar.h sys/shm.h sys/sem.h])
AC_CHECK_HEADERS([sys/statvfs.h sys/vfs.h])

AC_CHECK_MEMBERS([struct stat.st_blksize struct stat.st_rdev])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_HEADER_MAJOR
AC_HEADER_RESOLV
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UID_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_CHECK_TYPES([ptrdiff_t])

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_FUNC_MKTIME
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_FUNC_STRTOD
#
# Checks for library functions.
AC_CHECK_FUNCS(va_start, break)
AC_CHECK_FUNCS([fdatasync])
AC_CHECK_FUNCS([getmntent])
AC_CHECK_HEADERS([mntent.h])
AC_CHECK_HEADERS([sys/statfs.h])
AC_CHECK_FUNCS([dup2 memmove select setlocale socket strcspn strdup strerror strstr strtol])
AC_CHECK_FUNCS([alarm bzero floor getcwd gethostbyname inet_ntoa modf pow setenv strcasecmp gettimeofday sigemptyset sigaddset pthread_sigmask localtime_r localtime log2l pthread_mutexattr_init pthread_mutexattr_setpshared pthread_mutexattr_setrobust_np pthread_mutex_lock pthread_mutex_consistent_np mkdir chmod ftruncate fchmod mmap munmap access pthread_mutexattr_init pthread_mutexattr_setpshared pthread_mutex_init pthread_condattr_init shm_open pthread_cond_init shm_unlink unlink pthread_cond_timedwait msync feof getc ungetc feof clock_gettime random fstat strncasecmp strchr tcgetattr gethostname atof popen uuid_generate uuid_unparse uuid_clear uname sysconf kstat_open kstat_lookup kstat_read kstat_data_lookup kstat_close fread readdir closedir getuid getpwuid getrlimit freopen setvbuf getrlimit setrlimit scandir basename difftime pthread_mutex_timedlock_np pthread_mutex_timedlock pthread_mutex_trylock atexit ldiv div getpagesize memset rmdir  ])
AC_FUNC_ERROR_AT_LINE
AC_FUNC_GETMNTENT

echo "==================  Notice to the follows ================="
#check libraries.
ld -lcurses 2>/dev/null && echo "libcurses exists" || echo "libcurses not found"
ld -lsocket 2>/dev/null && echo "libsocket exists" || echo "libsocket not found"
ld -luuid 2>/dev/null && echo "libuuid exists" || echo "libuuid not found"

ld -lcurses 2>/dev/null && CURSES_FLAG=yes || CURSES_FLAG=no
ld -lsocket 2>/dev/null && SOCKET_FLAG=yes || SOCKET_FLAG=no
ld -luuid 2>/dev/null && UUID_FLAG=yes || UUID_FLAG=no

# ------------------------------------------------------------------------
# Architecture/OS detection
# ------------------------------------------------------------------------
# Backward compatibility until we get ride of arch settings

if test `uname` = "SunOS"
then
	ARCH="OS_SOLARIS"
	CFLAGS="$CFLAGS -D_REENTRANT -D_GNU_SOURCE -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D$ARCH -D_POSIX_PTHREAD_SEMANTICS"
	SOCKET_FLAG=yes
	if test `isainfo -b` = "64"
	then
		CFLAGS="$CFLAGS -m64"
	else
		CFLAGS="$CFLAGS -m32"
	fi
elif test `uname` = "Linux"
then
	ARCH="OS_LINUX"
	CFLAGS="$CFLAGS -D_REENTRANT -D_GNU_SOURCE -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D$ARCH -Wno-unused-result -Wfatal-errors"
	SOCKET_FLAG=no
	if test `uname -p` = "x86_64"
	then
		CFLAGS="$CFLAGS -m64"
	fi
elif test `uname` = "HP-UX"
then
	ARCH="OS_HPUX"
#	CFLAGS="$CFLAGS +DD64 -D_XOPEN_SOURCE -D_XOPEN_SOURCE_EXTENDED=1 -D_REENTRANT -D_GNU_SOURCE -DSYSV3 -DHPUX -D_HPUX_SOURCE -D$ARCH"
	CFLAGS="$CFLAGS +DD64 -D_REENTRANT -D_GNU_SOURCE -DSYSV3 -DHPUX -D_HPUX_SOURCE -D$ARCH"
	SOCKET_FLAG=no
elif test `uname` = "AIX"
then
	ARCH="OS_AIX"
#  In ShinHan Invest, -q64 option is not available. AIM-AIX
#	CFLAGS="$CFLAGS -D_REENTRANT -D_GNU_SOURCE -D$ARCH -q64"
#
	CFLAGS="$CFLAGS -D_REENTRANT -D_GNU_SOURCE -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D$ARCH"
	SOCKET_FLAG=no
else
   { echo "$as_me:$LINENO: WARNING: Architecture not supported: \`uname\`." >&5
	echo "$as_me: WARNING: Architecture not supported: \`uname\`." >&2;}
	CFLAGS="$CFLAGS -D_REENTRANT"
	ARCH="UNKNOWN"
	SOCKET_FLAG=no
fi

echo "- Your OS is $ARCH."
echo "- CFLAGS : ${CFLAGS}."
echo "- curses_library=${CURSES_FLAG}"
echo "- socket_library=${SOCKET_FLAG}"
echo "- uuid_library=${UUID_FLAG}"
echo "==================  Notice to the uppeer  ================="

AM_CONDITIONAL([HAVE_SOCKET], [test "${SOCKET_FLAG}" = yes])
AM_CONDITIONAL([HAVE_CURSES], [test "${CURSES_FLAG}" = yes])
AM_CONDITIONAL([HAVE_UUID], [test "${UUID_FLAG}" = yes])

AC_CONFIG_FILES([Makefile
                 src/fq_mon_cli/Makefile
                 src/fq_mon_svr/Makefile
                 src/lib/Makefile
                 src/mon/Makefile
                 src/monitor/Makefile
                 src/shm_monitor/Makefile
                 src/tst/Makefile
                 src/utl/Makefile
                 src/webgate/Makefile
                 src/json_simulate/Makefile
				 src/all_queue_test/Makefile
				 src/fqpm/Makefile
				 src/fqpm/fqpm_clear/Makefile
				 src/import/Makefile
				 src/fq_callback_svr/Makefile
				 src/fq_handler/svr/Makefile
				 src/fq_handler/cli/Makefile
				 src/fq_handler/cli/fq_put_agent/Makefile
				 src/fq_handler/cli/fq_get_agent/Makefile
				 src/log_notifier/Makefile
				 src/webgate_client/Makefile
				 src/fq_handler_json/svr/Makefile
				 src/fq_handler_json/cli/Makefile
				 src/fq_handler_json/put_agent/Makefile
				 src/fq_handler_json/get_agent/Makefile
				 src/fq_handler_json/fq_agent_threads/Makefile
				 src/subQ_distribute/Makefile
				 src/fq_alarm_svr/Makefile
				 src/fq_alarm_client/Makefile
				 src/fq_deadlock_release/Makefile
				 src/nmon/Makefile
				 src/wmonitor/Makefile])
AC_OUTPUT
