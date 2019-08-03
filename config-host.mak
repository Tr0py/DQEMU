# Automatically generated by configure - do not modify

all:
prefix=/usr/local
bindir=${prefix}/bin
libdir=${prefix}/lib
libexecdir=${prefix}/libexec
includedir=${prefix}/include
mandir=${prefix}/share/man
sysconfdir=${prefix}/etc
qemu_confdir=${prefix}/etc/qemu
qemu_datadir=${prefix}/share/qemu
qemu_firmwarepath=${prefix}/share/qemu-firmware
qemu_docdir=${prefix}/share/doc/qemu
qemu_moddir=${prefix}/lib/qemu
qemu_localstatedir=${prefix}/var
qemu_helperdir=${prefix}/libexec
qemu_localedir=${prefix}/share/locale
libs_softmmu=-lpixman-1 -lutil -lnuma -lbluetooth -lpng16 -lz -ljpeg -lsasl2 -lnettle -lgnutls -lxenctrl -lxenstore -lxenguest -lxenforeignmemory -lxengnttab -lxenevtchn -lxendevicemodel  -lfdt -lspice-server
GIT=git
GIT_SUBMODULES=ui/keycodemapdb capstone
GIT_UPDATE=yes
ARCH=x86_64
STRIP=strip
CONFIG_POSIX=y
CONFIG_LINUX=y
CONFIG_SLIRP=y
CONFIG_SMBD_COMMAND="/usr/sbin/smbd"
CONFIG_L2TPV3=y
CONFIG_LIBCAP=y
CONFIG_AUDIO_DRIVERS=oss
CONFIG_AUDIO_OSS=m
ALSA_LIBS=
PULSE_LIBS=
COREAUDIO_LIBS=
DSOUND_LIBS=
OSS_LIBS=
CONFIG_BDRV_RW_WHITELIST=
CONFIG_BDRV_RO_WHITELIST=
CONFIG_VNC=y
CONFIG_VNC_SASL=y
CONFIG_VNC_JPEG=y
CONFIG_VNC_PNG=y
CONFIG_FNMATCH=y
CONFIG_XFS=y
VERSION=3.0.0
PKGVERSION=
SRC_PATH=/home/tropping/DQEMU
TARGET_DIRS=arm-linux-user
BUILD_DOCS=yes
CONFIG_X11=y
X11_CFLAGS=
X11_LIBS=-lX11
CONFIG_SDL=m
CONFIG_SDLABI=1.2
SDL_CFLAGS=-D_GNU_SOURCE=1 -D_REENTRANT -I/usr/include/SDL -Wno-undef 
SDL_LIBS=-lSDL -lX11
CONFIG_CURSES=m
CURSES_CFLAGS=-DNCURSES_WIDECHAR -D_GNU_SOURCE -D_DEFAULT_SOURCE -I/usr/include/ncursesw
CURSES_LIBS=-lncursesw -ltinfo
CONFIG_PIPE2=y
CONFIG_ACCEPT4=y
CONFIG_SPLICE=y
CONFIG_EVENTFD=y
CONFIG_MEMFD=y
CONFIG_FALLOCATE=y
CONFIG_FALLOCATE_PUNCH_HOLE=y
CONFIG_FALLOCATE_ZERO_RANGE=y
CONFIG_POSIX_FALLOCATE=y
CONFIG_SYNC_FILE_RANGE=y
CONFIG_FIEMAP=y
CONFIG_DUP3=y
CONFIG_PPOLL=y
CONFIG_PRCTL_PR_SET_TIMERSLACK=y
CONFIG_EPOLL=y
CONFIG_EPOLL_CREATE1=y
CONFIG_SENDFILE=y
CONFIG_TIMERFD=y
CONFIG_SETNS=y
CONFIG_CLOCK_ADJTIME=y
CONFIG_SYNCFS=y
CONFIG_INOTIFY=y
CONFIG_INOTIFY1=y
CONFIG_SEM_TIMEDWAIT=y
HAVE_STRCHRNUL=y
CONFIG_BYTESWAP_H=y
CONFIG_CURL=m
CURL_CFLAGS=-I/usr/include/x86_64-linux-gnu
CURL_LIBS=-lcurl
CONFIG_BRLAPI=y
BRLAPI_LIBS=-lbrlapi
CONFIG_BLUEZ=y
BLUEZ_CFLAGS=
CONFIG_HAS_GLIB_SUBPROCESS_TESTS=y
CONFIG_TLS_PRIORITY="NORMAL"
CONFIG_GNUTLS=y
CONFIG_GNUTLS_RND=y
CONFIG_NETTLE=y
CONFIG_NETTLE_VERSION_MAJOR=3
CONFIG_NETTLE_KDF=y
CONFIG_TASN1=y
HAVE_IFADDRS_H=y
HAVE_FSXATTR=y
HAVE_COPY_FILE_RANGE=y
CONFIG_XEN_BACKEND=y
CONFIG_XEN_CTRL_INTERFACE_VERSION=40900
CONFIG_LINUX_AIO=y
CONFIG_ATTR=y
CONFIG_VHOST_SCSI=y
CONFIG_VHOST_NET_USED=y
CONFIG_VHOST_CRYPTO=y
CONFIG_VHOST_VSOCK=y
CONFIG_VHOST_USER=y
INSTALL_BLOBS=yes
CONFIG_IOVEC=y
CONFIG_PREADV=y
CONFIG_FDT=y
CONFIG_SIGNALFD=y
CONFIG_TCG=y
CONFIG_FDATASYNC=y
CONFIG_MADVISE=y
CONFIG_POSIX_MADVISE=y
CONFIG_POSIX_MEMALIGN=y
CONFIG_SPICE=y
CONFIG_USB_LIBUSB=y
LIBUSB_CFLAGS=-I/usr/include/libusb-1.0
LIBUSB_LIBS=-lusb-1.0
CONFIG_USB_REDIR=y
USB_REDIR_CFLAGS=
USB_REDIR_LIBS=-lusbredirparser
CONFIG_MALLOC_TRIM=y
CONFIG_AVX2_OPT=y
CONFIG_LIBISCSI=m
LIBISCSI_CFLAGS=
LIBISCSI_LIBS=-liscsi
CONFIG_SECCOMP=y
SECCOMP_CFLAGS=
SECCOMP_LIBS=-lseccomp
CONFIG_QOM_CAST_DEBUG=y
CONFIG_RBD=m
RBD_CFLAGS=
RBD_LIBS=-lrbd -lrados
CONFIG_COROUTINE_BACKEND=ucontext
CONFIG_COROUTINE_POOL=1
CONFIG_OPEN_BY_HANDLE=y
CONFIG_LINUX_MAGIC_H=y
CONFIG_PRAGMA_DIAGNOSTIC_AVAILABLE=y
CONFIG_HAS_ENVIRON=y
CONFIG_CPUID_H=y
CONFIG_INT128=y
CONFIG_ATOMIC64=y
CONFIG_VECTOR16=y
CONFIG_GETAUXVAL=y
CONFIG_LIVE_BLOCK_MIGRATION=y
CONFIG_TPM=$(CONFIG_SOFTMMU)
CONFIG_TPM_PASSTHROUGH=y
CONFIG_TPM_EMULATOR=y
TRACE_BACKENDS=log
CONFIG_TRACE_LOG=y
CONFIG_TRACE_FILE=trace
CONFIG_RTNETLINK=y
CONFIG_REPLICATION=y
CONFIG_AF_VSOCK=y
CONFIG_SYSMACROS=y
CONFIG_STATIC_ASSERT=y
HAVE_UTMPX=y
CONFIG_IVSHMEM=y
CONFIG_CAPSTONE=y
CONFIG_THREAD_SETNAME_BYTHREAD=y
CONFIG_PTHREAD_SETNAME_NP=y
TOOLS=qemu-ga ivshmem-client$(EXESUF) ivshmem-server$(EXESUF) qemu-nbd$(EXESUF) qemu-img$(EXESUF) qemu-io$(EXESUF) 
ROMS=
MAKE=make
INSTALL=install
INSTALL_DIR=install -d -m 0755
INSTALL_DATA=install -c -m 0644
INSTALL_PROG=install -c -m 0755
INSTALL_LIB=install -c -m 0644
PYTHON=python -B
CC=cc
IASL=iasl
HOST_CC=cc
CXX=c++
OBJCC=cc
AR=ar
ARFLAGS=rv
AS=as
CCAS=cc
CPP=cc -E
OBJCOPY=objcopy
LD=ld
RANLIB=ranlib
NM=nm
WINDRES=windres
CFLAGS=-O0 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -g -Wno-error
CFLAGS_NOPIE=
QEMU_CFLAGS=-I/usr/include/pixman-1  -pthread -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -fPIE -DPIE -m64 -mcx16 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -Wstrict-prototypes -Wredundant-decls -Wall -Wundef -Wwrite-strings -Wmissing-prototypes -fno-strict-aliasing -fno-common -fwrapv  -Wexpansion-to-defined -Wendif-labels -Wno-shift-negative-value -Wno-missing-include-dirs -Wempty-body -Wnested-externs -Wformat-security -Wformat-y2k -Winit-self -Wignored-qualifiers -Wold-style-declaration -Wold-style-definition -Wtype-limits -fstack-protector-strong  -I/usr/include/p11-kit-1    -I/usr/include/libpng16 -I/usr/include/spice-server -I/usr/include/spice-1 -I$(SRC_PATH)/capstone/include
QEMU_CXXFLAGS= -D__STDC_LIMIT_MACROS -pthread -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -fPIE -DPIE -m64 -mcx16 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -Wall -Wundef -Wwrite-strings -fno-strict-aliasing -fno-common -fwrapv -Wexpansion-to-defined -Wendif-labels -Wno-shift-negative-value -Wno-missing-include-dirs -Wempty-body -Wformat-security -Wformat-y2k -Winit-self -Wignored-qualifiers -Wtype-limits -fstack-protector-strong -I/usr/include/p11-kit-1 -I/usr/include/libpng16 -I/usr/include/spice-server -I/usr/include/spice-1 -I$(SRC_PATH)/capstone/include
QEMU_INCLUDES=-iquote $(SRC_PATH)/tcg -iquote $(SRC_PATH)/tcg/i386 -I$(SRC_PATH)/linux-headers -I/home/tropping/DQEMU/linux-headers -iquote . -iquote $(SRC_PATH) -iquote $(SRC_PATH)/accel/tcg -iquote $(SRC_PATH)/include
AUTOCONF_HOST := 
LDFLAGS=-Wl,--warn-common -lxenctrl -lxenstore -lxenguest -lxenforeignmemory -lxengnttab -lxenevtchn -lxendevicemodel -Wl,-z,relro -Wl,-z,now -pie -m64 -g 
LDFLAGS_NOPIE=
QEMU_LDFLAGS= 
LD_REL_FLAGS=-r
LD_I386_EMULATION=
LIBS+=-L$(BUILD_DIR)/capstone -lcapstone -lm -lgthread-2.0 -pthread -lglib-2.0  -lz -lrt
LIBS_TOOLS+=-lcap-ng -lnettle -lgnutls 
PTHREAD_LIB=
EXESUF=
DSOSUF=.so
LDFLAGS_SHARED=-shared
LIBS_QGA+=-lm -lgthread-2.0 -pthread -lglib-2.0  -lrt
TASN1_LIBS=-ltasn1
TASN1_CFLAGS=
POD2MAN=pod2man --utf8
TRANSLATE_OPT_CFLAGS=
HAVE_USER_DOCKER=y
config-host.h: subdir-capstone
LIBCAPSTONE=libcapstone.a
CONFIG_NUMA=y
