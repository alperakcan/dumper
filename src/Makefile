
SYNC_USE_FUTEX ?= n

target-y = \
	dumper

dumper_sync.c_cflags-${SYNC_USE_FUTEX} = \
	-DSYNC_USE_FUTEX

dumper_files-y = \
	dumper.c \
	sync.c \
	socket.c \
	file.c

dumper_ldflags-y = \
	-lpthread

include ../Makefile.lib
