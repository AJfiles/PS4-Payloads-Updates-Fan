/* Solo las syscalls necesarias para cargar/resolver libkernel y comprobar sandbox. */
#include "payload_runtime.h"
#include "syscall.h"

SYSCALL(getFunctionAddressByName, 591);
SYSCALL(payload_sys_load_module, 594);
SYSCALL(payload_in_sandbox, 585);
