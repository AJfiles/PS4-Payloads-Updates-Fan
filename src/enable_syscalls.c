/* El BIN enable no contiene unlink ni otra eliminación recursiva. */
#include "payload_runtime.h"
#include "syscall.h"

SYSCALL(payload_open, 5);
SYSCALL(payload_close, 6);
SYSCALL(payload_lstat, 190);
SYSCALL(payload_mkdir, 136); /* rollback seguro de un rmdir parcial */
SYSCALL(payload_rmdir, 137);
SYSCALL(payload_getdents, 272);
SYSCALL(payload_umask, 60);
