// Minimal syscalls for bare-metal STM32L0 test-app (newlib)
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>

extern int uart_write(uint8_t c);   // your app's uart byte TX

__attribute__((weak)) int _write(int fd, const char *buf, int len) {
    (void)fd;
    for (int i = 0; i < len; i++) uart_write((uint8_t)buf[i]);
    return len;
}

__attribute__((weak)) int _read(int fd, char *buf, int len) {
    (void)fd; (void)buf; (void)len;
    errno = EAGAIN; return -1;
}

__attribute__((weak)) int _close(int fd)       { (void)fd; errno = ENOSYS; return -1; }
__attribute__((weak)) int _lseek(int fd, int o, int w){ (void)fd;(void)o;(void)w; errno=ENOSYS; return -1; }
__attribute__((weak)) int _fstat(int fd, struct stat *st){ (void)fd; st->st_mode = S_IFCHR; return 0; }
__attribute__((weak)) int _isatty(int fd)      { (void)fd; return 1; }
__attribute__((weak)) int _kill(int pid, int s){ (void)pid;(void)s; errno = ENOSYS; return -1; }
__attribute__((weak)) int _getpid(void)        { return 1; }
__attribute__((weak,noreturn)) void _exit(int status){ (void)status; for(;;){} }

// Optional: if you use malloc/newlib, provide a basic heap via _sbrk
extern char _end;           // provided by linker (end of .bss)
static char *heap = &_end;
__attribute__((weak)) void* _sbrk(int incr) {
    (void)incr;
    errno = ENOMEM;
    return (void*)-1;
}