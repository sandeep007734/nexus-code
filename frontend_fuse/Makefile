include ../build.mk

buildpath = ../build

libs := -Wl,--whole-archive ../libnexus.a -Wl,--no-whole-archive -luuid -lpthread -lcurl -lfuse3

LDFLAGS := -L$(SGX_SDK)/lib64

INCLUDES := -I../libnexus
CFLAGS = -g -W -Wall -Werror -D_GNU_SOURCE -Wno-nonnull -Wno-unused-parameter -std=gnu11\
         -lulockmgr -D_FILE_OFFSET_BITS=64 $(INCLUDES)

CC = gcc
AR = ar


exec_objs := \
	vfs.o \
	dentry.o \
	fuse_interface.o \
	frontend_handler.o \
	io.o \
	commander.o \
	main.o

execs := nexus-fuse


ifeq ($(SGX_MODE), SIM)
	libs += -lsgx_urts_sim -lsgx_uae_service_sim
else ifeq ($(SGX_MODE), HW)
	libs += -lsgx_urts -lsgx_uae_service
else
        $(error Invalid SGX MODE)
endif


build = \
        @if [ -z "$V" ]; then \
                echo '    [$1]  $@'; \
                $2; \
        else \
                echo '$2'; \
                $2; \
        fi


all: frontend_fuse.a nexus-fuse

% : %.c
	$(call build,CC,$(CC) $(CFLAGS) $< -o $@)

%.o : %.c
	$(call build,CC,$(CC) $(CFLAGS) -c $<  -o $@)

%.a : %.o
	$(call build,AR,$(AR) rcs $@ $^)

frontend_fuse.a: $(exec_objs)
	$(call build,AR,$(AR) rcs $@ $^)
	@cp $@ $(buildpath)

nexus-fuse: $(exec_objs)
	$(call build,CC,$(CC) $(CFLAGS) $^ $(LDFLAGS) $(libs) -o $@)



define cscope-all-sources
	( find . $(RCS_FIND_IGNORE) \
		\( -name '*.[chS]' -o -name '*.cc' \) -print; \
	)
endef


cscope:
	$(call build,FILELIST,(echo \-k; echo \-q; $(cscope-all-sources)) > cscope.files)
	$(call build,MAKE,cscope -b)



.PHONY: clean
clean:
	rm -f $(wildcard  $(exec_objs) cscope.* $(execs) *.o *.a)

