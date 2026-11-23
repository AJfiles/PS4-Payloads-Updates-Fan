/* Wrappers mínimos FreeBSD/Orbis tomados de los números usados por el SDK. */
#include "syscall.h"
#include "module.h"

int sys_load_module(const char *name, int zero, int *module_id, int flags);

SYSCALL(open, 5);
SYSCALL(close, 6);
SYSCALL(write, 4);       /* solo el fallback USB verificado */
SYSCALL(stat, 188);
SYSCALL(getdents, 272); /* solo lectura: cuenta entradas de directorios */
SYSCALL(getfsstat, 395); /* solo lectura: verifica el montaje USB */

/* syscall 591 resuelve símbolos; 594 carga módulos con cuatro argumentos. */
SYSCALL(getFunctionAddressByName, 591);
SYSCALL(sys_load_module, 594);

int loadModule(const char *name, int *module_id) {
  return sys_load_module(name, 0, module_id, 0);
}
