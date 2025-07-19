// SPDX-License-Identifier: GPL-3.0-or-later

#define __NO_START_FUNCTION
#include "mosapi.h"

#include <errno.h>
#include <fcntl.h>
#include <mos/syscall/number.h>
#include <mos/syscall/usermode.h>
#include <mos_stdio.hpp>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>
#include <stdarg.h>

struct _FILE
{
    int fd;
};

static struct _FILE __stdin = { .fd = 0 };
static struct _FILE __stdout = { .fd = 1 };
static struct _FILE __stderr = { .fd = 2 };

FILE *stdin = &__stdin;
FILE *stdout = &__stdout;
FILE *stderr = &__stderr;

typedef struct thread_start_args
{
    thread_entry_t entry;
    void *arg;
} thread_start_args_t;

u64 __stack_chk_guard = 0xdeadbeefdeadbeef;

[[noreturn]] void __stack_chk_fail(void)
{
    puts("stack smashing detected...");
    syscall_exit(-1);
    __builtin_unreachable();
}

void __stack_chk_fail_local(void)
{
    __stack_chk_fail();
}

void __printf(1, 2) fatal_abort(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vdprintf(stderr->fd, fmt, ap);
    va_end(ap);
    abort();
}

[[noreturn]] void abort(void)
{
    raise(SIGABRT);
    syscall_exit(-1);
}

fd_t open(const char *path, open_flags flags)
{
    if (path == NULL)
        return -1;

    return openat(AT_FDCWD, path, flags);
}

long close(fd_t fd)
{
    if (fd < 0)
        return -1;

    return syscall_io_close(fd);
}

fd_t openat(fd_t fd, const char *path, open_flags flags)
{
    return syscall_vfs_openat(fd, path, flags);
}

int raise(signal_t sig)
{
    syscall_signal_thread(syscall_get_tid(), sig);
    return 0;
}

bool lstatat(int fd, const char *path, file_stat_t *buf)
{
    return syscall_vfs_fstatat(fd, path, buf, FSTATAT_NOFOLLOW) == 0;
}

bool chdir(const char *path)
{
    return syscall_vfs_chdirat(AT_FDCWD, path) == 0;
}

bool unlink(const char *path)
{
    return syscall_vfs_unlinkat(AT_FDCWD, path) == 0;
}

int printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int size = vdprintf(stdout->fd, fmt, ap);
    va_end(ap);
    return size;
}

int fprintf(FILE *stream, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int size = vdprintf(stream->fd, fmt, ap);
    va_end(ap);
    return size;
}

int vdprintf(int fd, const char *fmt, va_list ap)
{
    char buffer[256];
    int size = vsnprintf(buffer, sizeof(buffer), fmt, ap);
    syscall_io_write(fd, buffer, size);
    return size;
}

int putchar(int c)
{
    char ch = c;
    syscall_io_write(stdout->fd, &ch, 1);
    return c;
}

int puts(const char *s)
{
    syscall_io_write(stdout->fd, s, strlen(s));
    return putchar('\n');
}

int fputs(const char *restrict s, FILE *restrict file)
{
    return syscall_io_write(file->fd, s, strlen(s));
}

size_t fwrite(const void *__restrict ptr, size_t size, size_t nmemb, FILE *__restrict stream)
{
    return syscall_io_write(stream->fd, ptr, size * nmemb) / size;
}

int fdgets(char *s, int size, fd_t fd)
{
    if (size <= 0 || !s)
        return 0;

    size_t read = syscall_io_read(fd, s, size - 1);
    if (read == 0)
        return 0;

    s[read] = '\0'; // null-terminate the string
    return read;
}

const char *strerror(int e)
{
    const char *s;
    switch (e)
    {
        case EAGAIN: s = "Operation would block (EAGAIN)"; break;
        case EACCES: s = "Access denied (EACCESS)"; break;
        case EBADF: s = "Bad file descriptor (EBADF)"; break;
        case EEXIST: s = "File exists already (EEXIST)"; break;
        case EFAULT: s = "Access violation (EFAULT)"; break;
        case EINTR: s = "Operation interrupted (EINTR)"; break;
        case EINVAL: s = "Invalid argument (EINVAL)"; break;
        case EIO: s = "I/O error (EIO)"; break;
        case EISDIR: s = "Resource is directory (EISDIR)"; break;
        case ENOENT: s = "No such file or directory (ENOENT)"; break;
        case ENOMEM: s = "Out of memory (ENOMEM)"; break;
        case ENOTDIR: s = "Expected directory instead of file (ENOTDIR)"; break;
        case ENOSYS: s = "Operation not implemented (ENOSYS)"; break;
        case EPERM: s = "Operation not permitted (EPERM)"; break;
        case EPIPE: s = "Broken pipe (EPIPE)"; break;
        case ESPIPE: s = "Seek not possible (ESPIPE)"; break;
        case ENXIO: s = "No such device or address (ENXIO)"; break;
        case ENOEXEC: s = "Exec format error (ENOEXEC)"; break;
        case ENOSPC: s = "No space left on device (ENOSPC)"; break;
        case ENOTSOCK: s = "Socket operation on non-socket (ENOTSOCK)"; break;
        case ENOTCONN: s = "Transport endpoint is not connected (ENOTCONN)"; break;
        case EDOM: s = "Numerical argument out of domain (EDOM)"; break;
        case EILSEQ: s = "Invalid or incomplete multibyte or wide character (EILSEQ)"; break;
        case ERANGE: s = "Numerical result out of range (ERANGE)"; break;
        case E2BIG: s = "Argument list too long (E2BIG)"; break;
        case EADDRINUSE: s = "Address already in use (EADDRINUSE)"; break;
        case EADDRNOTAVAIL: s = "Cannot assign requested address (EADDRNOTAVAIL)"; break;
        case EAFNOSUPPORT: s = "Address family not supported by protocol (EAFNOSUPPORT)"; break;
        case EALREADY: s = "Operation already in progress (EALREADY)"; break;
        case EBADMSG: s = "Bad message (EBADMSG)"; break;
        case EBUSY: s = "Device or resource busy (EBUSY)"; break;
        case ECANCELED: s = "Operation canceled (ECANCELED)"; break;
        case ECHILD: s = "No child processes (ECHILD)"; break;
        case ECONNABORTED: s = "Software caused connection abort (ECONNABORTED)"; break;
        case ECONNREFUSED: s = "Connection refused (ECONNREFUSED)"; break;
        case ECONNRESET: s = "Connection reset by peer (ECONNRESET)"; break;
        case EDEADLK: s = "Resource deadlock avoided (EDEADLK)"; break;
        case EDESTADDRREQ: s = "Destination address required (EDESTADDRREQ)"; break;
        case EDQUOT: s = "Disk quota exceeded (EDQUOT)"; break;
        case EFBIG: s = "File too large (EFBIG)"; break;
        case EHOSTUNREACH: s = "No route to host (EHOSTUNREACH)"; break;
        case EIDRM: s = "Identifier removed (EIDRM)"; break;
        case EINPROGRESS: s = "Operation now in progress (EINPROGRESS)"; break;
        case EISCONN: s = "Transport endpoint is already connected (EISCONN)"; break;
        case ELOOP: s = "Too many levels of symbolic links (ELOOP)"; break;
        case EMFILE: s = "Too many open files (EMFILE)"; break;
        case EMLINK: s = "Too many links (EMLINK)"; break;
        case EMSGSIZE: s = "Message too long (EMSGSIZE)"; break;
        case EMULTIHOP: s = "Multihop attempted (EMULTIHOP)"; break;
        case ENAMETOOLONG: s = "File name too long (ENAMETOOLONG)"; break;
        case ENETDOWN: s = "Network is down (ENETDOWN)"; break;
        case ENETRESET: s = "Network dropped connection on reset (ENETRESET)"; break;
        case ENETUNREACH: s = "Network is unreachable (ENETUNREACH)"; break;
        case ENFILE: s = "Too many open files in system (ENFILE)"; break;
        case ENOBUFS: s = "No buffer space available (ENOBUFS)"; break;
        case ENODEV: s = "No such device (ENODEV)"; break;
        case ENOLCK: s = "No locks available (ENOLCK)"; break;
        case ENOLINK: s = "Link has been severed (ENOLINK)"; break;
        case ENOMSG: s = "No message of desired type (ENOMSG)"; break;
        case ENOPROTOOPT: s = "Protocol not available (ENOPROTOOPT)"; break;
        case ENOTEMPTY: s = "Directory not empty (ENOTEMPTY)"; break;
        case ENOTRECOVERABLE: s = "Sate not recoverable (ENOTRECOVERABLE)"; break;
        case ENOTSUP: s = "Operation not supported (ENOTSUP)"; break;
        case ENOTTY: s = "Inappropriate ioctl for device (ENOTTY)"; break;
        case EOVERFLOW: s = "Value too large for defined datatype (EOVERFLOW)"; break;
#if EOPNOTSUPP != ENOTSUP
        /* these are aliases on the mlibc abi */
        case EOPNOTSUPP: s = "Operation not supported (EOPNOTSUP)"; break;
#endif
        case EOWNERDEAD: s = "Owner died (EOWNERDEAD)"; break;
        case EPROTO: s = "Protocol error (EPROTO)"; break;
        case EPROTONOSUPPORT: s = "Protocol not supported (EPROTONOSUPPORT)"; break;
        case EPROTOTYPE: s = "Protocol wrong type for socket (EPROTOTYPE)"; break;
        case EROFS: s = "Read-only file system (EROFS)"; break;
        case ESRCH: s = "No such process (ESRCH)"; break;
        case ESTALE: s = "Stale file handle (ESTALE)"; break;
        case ETIMEDOUT: s = "Connection timed out (ETIMEDOUT)"; break;
        case ETXTBSY: s = "Text file busy (ETXTBSY)"; break;
        case EXDEV: s = "Invalid cross-device link (EXDEV)"; break;
        case ENODATA: s = "No data available (ENODATA)"; break;
        case ETIME: s = "Timer expired (ETIME)"; break;
        case ENOKEY: s = "Required key not available (ENOKEY)"; break;
        case ESHUTDOWN: s = "Cannot send after transport endpoint shutdown (ESHUTDOWN)"; break;
        case EHOSTDOWN: s = "Host is down (EHOSTDOWN)"; break;
        case EBADFD: s = "File descriptor in bad state (EBADFD)"; break;
        case ENOMEDIUM: s = "No medium found (ENOMEDIUM)"; break;
        case ENOTBLK: s = "Block device required (ENOTBLK)"; break;
        case ENONET: s = "Machine is not on the network (ENONET)"; break;
        case EPFNOSUPPORT: s = "Protocol family not supported (EPFNOSUPPORT)"; break;
        case ESOCKTNOSUPPORT: s = "Socket type not supported (ESOCKTNOSUPPORT)"; break;
        case ESTRPIPE: s = "Streams pipe error (ESTRPIPE)"; break;
        case EREMOTEIO: s = "Remote I/O error (EREMOTEIO)"; break;
        case ERFKILL: s = "Operation not possible due to RF-kill (ERFKILL)"; break;
        case EBADR: s = "Invalid request descriptor (EBADR)"; break;
        case EUNATCH: s = "Protocol driver not attached (EUNATCH)"; break;
        case EMEDIUMTYPE: s = "Wrong medium type (EMEDIUMTYPE)"; break;
        case EREMOTE: s = "Object is remote (EREMOTE)"; break;
        case EKEYREJECTED: s = "Key was rejected by service (EKEYREJECTED)"; break;
        case EUCLEAN: s = "Structure needs cleaning (EUCLEAN)"; break;
        case EBADSLT: s = "Invalid slot (EBADSLT)"; break;
        case ENOANO: s = "No anode (ENOANO)"; break;
        case ENOCSI: s = "No CSI structure available (ENOCSI)"; break;
        case ENOSTR: s = "Device not a stream (ENOSTR)"; break;
        case ETOOMANYREFS: s = "Too many references: cannot splice (ETOOMANYREFS)"; break;
        case ENOPKG: s = "Package not installed (ENOPKG)"; break;
        case EKEYREVOKED: s = "Key has been revoked (EKEYREVOKED)"; break;
        case EXFULL: s = "Exchange full (EXFULL)"; break;
        case ELNRNG: s = "Link number out of range (ELNRNG)"; break;
        case ENOTUNIQ: s = "Name not unique on network (ENOTUNIQ)"; break;
        case ERESTART: s = "Interrupted system call should be restarted (ERESTART)"; break;
        case EUSERS: s = "Too many users (EUSERS)"; break;

#ifdef EIEIO
        case EIEIO: s = "Computer bought the farm; OS internal error (EIEIO)"; break;
#endif

        default: s = "Unknown error code (?)";
    }
    return s;
}
