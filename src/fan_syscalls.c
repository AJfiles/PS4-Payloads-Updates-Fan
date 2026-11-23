/* Este BIN fan no incluye syscalls de borrado ni creación de directorios. */
#include "payload_runtime.h"
#include "syscall.h"

SYSCALL(payload_open, 5);
SYSCALL(payload_close, 6);
SYSCALL(payload_lstat, 190);
SYSCALL(payload_ioctl, 54);
