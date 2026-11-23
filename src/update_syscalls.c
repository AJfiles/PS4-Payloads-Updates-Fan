/* Syscalls filesystem limitadas a la implementación de los dos señuelos. */
#include "payload_runtime.h"
#include "syscall.h"

SYSCALL(payload_open, 5);
SYSCALL(payload_close, 6);
SYSCALL(payload_lstat, 190);
SYSCALL(payload_unlink, 10);
SYSCALL(payload_mkdir, 136);
SYSCALL(payload_getdents, 272);
SYSCALL(payload_umask, 60);
