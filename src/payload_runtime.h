#pragma once

#include <stdint.h>
#include "kernel.h"
#include "network.h"
#include "file.h"

#ifndef REPORT_HOST
#error "Compile con -DREPORT_HOST=\"IPv4_DEL_PC\""
#endif
#ifndef REPORT_PORT
#define REPORT_PORT 9090
#endif

#define REPORT_CAP 2048

extern char payload_report[REPORT_CAP];
extern size_t payload_report_len;
extern int *(*__error)(void);
extern int (*payload_getuid)(void);
extern int (*payload_getgid)(void);
extern int (*payload_sysctlbyname)(char *, char *, size_t *, char *, size_t);
extern int (*payload_get_sw_version)(SceFwInfo *);
extern int (*payload_sce_load_module)(const char *, size_t, const void *, unsigned int, int, int);

void report_start(const char *name);
void report_text(const char *text);
void report_char(char c);
void report_u64(uint64_t value);
void report_i64(int64_t value);
void report_hex(uint8_t value);
void report_bytes(const char *key, const uint8_t *bytes, size_t len);
int payload_errno(void);
void payload_log_firmware(void);
int payload_check_runtime(void);
int payload_send_report(void);
void payload_load_network_api(void);
int payload_notify(const char *message);
void payload_notify_fan(unsigned int target, unsigned int before, int is_default);
void payload_notify_updates(int enabling, int success);

/* Syscalls mínimos compartidos; no incluyen syscall genérica ni syscall 11. */
int payload_open(const char *path, int flags, int mode);
int payload_close(int fd);
int payload_lstat(const char *path, struct stat *st);
int payload_unlink(const char *path);
int payload_mkdir(const char *path, mode_t mode);
int payload_rmdir(const char *path);
int payload_getdents(int fd, char *buffer, int size);
int payload_umask(int mask);
int payload_ioctl(int fd, unsigned long request, void *data);
int payload_in_sandbox(void);
int payload_sys_load_module(const char *name, int zero, int *module_id, int flags);
