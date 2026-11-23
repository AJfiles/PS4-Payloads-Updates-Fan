/*
 * Sonda ampliada de solo lectura para GoldHEN / PS4 9.00 (Fase 2b/2c).
 * Consulta simultánea en hardware de:
 *  - 0x19800000 SCE_REGMGR_ENT_KEY_NP_env (GetStr, 17 bytes)
 *  - 0x0780B007 SCE_REGMGR_ENT_KEY_USER_01_16_NP_env(1) (GetStr, 17 bytes, contraste)
 *  - 0x02800500 SCE_REGMGR_ENT_KEY_SYSTEM_UPDATE_auto_download (GetInt, Int32)
 *
 * REGLAS DE SEGURIDAD Y PIC (Regla 12.3):
 * - Estrictamente de SOLO LECTURA: no invoca sceRegMgrSetStr, sceRegMgrSetInt ni ninguna API de escritura.
 * - Sin tablas de punteros absolutos; variables escalares y arrays contiguos.
 * - Sin kexec, jailbreak ni syscall 11.
 */
#include <stdint.h>
#include "types.h"
#include "kernel.h"
#include "file.h"
#include "network.h"
#include "syscall.h"
#include "module.h"

#define STATFS_SIZE       0x1D8
#define MNT_NOWAIT        2
#define MOUNT_MAX         192
#define SF_FSTYPENAME     0x118
#define SF_MNTFROMNAME    0x128
#define SF_MNTONNAME      0x180
#define SF_NAME_LEN       88
#define SF_TYPE_LEN       16
#define REPORT_CAP        8192
#define USB_REPORT_PATH   "/mnt/usb0/ps4_np_readonly_dual_probe.txt"

#ifndef PROBE_REPORT_IP
#error "Compile con -DPROBE_REPORT_IP=\"IPv4_DEL_PC\""
#endif
#ifndef PROBE_REPORT_PORT
#define PROBE_REPORT_PORT 9090
#endif
#define PC_REPORT_IP      PROBE_REPORT_IP

/* Símbolos del SDK */
extern int libNet;
extern int libNetCtl;
extern int *(*sceNetErrnoLoc)(void);
extern int (*sceNetSocket)(const char *, int, int, int);
extern int (*sceNetSocketClose)(int);
extern int (*sceNetConnect)(int, struct sockaddr *, int);
extern int (*sceNetSend)(int, const void *, size_t, int);
extern int (*sceNetInetPton)(int, const char *, void *);
extern uint16_t (*sceNetHtons)(uint16_t);
extern void initNetwork(void);
extern int getfsstat(void *, long, int);

/* Tipos para RegMgr */
typedef int32_t (*sceRegMgrGetStr_t)(int32_t optionId, char *outString, size_t stringLength);
typedef int32_t (*sceRegMgrGetInt_t)(int32_t optionId, int32_t *outInt);

typedef struct {
  uint8_t bytes[STATFS_SIZE];
} ps4_statfs_record;

static ps4_statfs_record mount_table[MOUNT_MAX] = {{{0xA5}}};
static char report[REPORT_CAP] = {0xA5};
static size_t report_len = 1;

/* Claves escalares (uint32_t contiguo) */
#define KEY_NP_ENV_SYS         0x19800000U
#define KEY_NP_ENV_USER1       0x0780B007U
#define KEY_UPDATE_AUTODL      0x02800500U

static int saved_errno(void) {
  return (__error != NULL && __error() != NULL) ? *__error() : 0;
}

static void set_errno_zero(void) {
  if (__error != NULL && __error() != NULL) {
    *__error() = 0;
  }
}

static void add_char(char c) {
  if (report_len + 1 < REPORT_CAP) {
    report[report_len++] = c;
    report[report_len] = 0;
  }
}

static void add_text(const char *s) {
  if (s == NULL) return;
  while (*s != 0) add_char(*s++);
}

static void add_unsigned(uint64_t n) {
  char tmp[24];
  unsigned int used = 0;
  do {
    tmp[used++] = (char)('0' + (n % 10));
    n /= 10;
  } while (n != 0 && used < sizeof(tmp));
  while (used != 0) add_char(tmp[--used]);
}

static void add_signed(int64_t n) {
  if (n < 0) {
    add_char('-');
    add_unsigned((uint64_t)(-(n + 1)) + 1);
  } else {
    add_unsigned((uint64_t)n);
  }
}

static void add_hex_nibble(uint8_t n) {
  n &= 0x0F;
  add_char((char)(n < 10 ? ('0' + n) : ('a' + n - 10)));
}

static void add_hex_byte(uint8_t b) {
  add_hex_nibble((uint8_t)(b >> 4));
  add_hex_nibble(b);
}

static void add_hex_u32(uint32_t v) {
  add_hex_byte((uint8_t)(v >> 24));
  add_hex_byte((uint8_t)(v >> 16));
  add_hex_byte((uint8_t)(v >> 8));
  add_hex_byte((uint8_t)v);
}

static void add_hex_span(const char *label, const uint8_t *src, size_t len) {
  add_text(label); add_char('=');
  for (size_t i = 0; i < len; ++i) {
    add_hex_byte(src[i]);
  }
  add_char('\n');
}

static void add_errno_field(const char *prefix, int err) {
  add_text(prefix); add_text(".errno=");
  add_signed(err);
  add_char('\n');
}

static void zero_bytes(void *dst, size_t n) {
  uint8_t *p = (uint8_t *)dst;
  while (n-- > 0) *p++ = 0;
}

static int fixed_field_equals(const uint8_t *base, size_t offset, size_t max_len, const char *lit) {
  const char *field = (const char *)(base + offset);
  size_t i = 0;
  for (; i < max_len && lit[i] != 0; ++i) {
    if (field[i] != lit[i]) return 0;
  }
  if (lit[i] != 0) return 0;
  return i < max_len ? field[i] == 0 : 1;
}

static int fixed_field_prefix(const uint8_t *base, size_t offset, size_t max_len, const char *lit_prefix) {
  const char *field = (const char *)(base + offset);
  for (size_t i = 0; i < max_len && lit_prefix[i] != 0; ++i) {
    if (field[i] != lit_prefix[i]) return 0;
  }
  return 1;
}

static void report_firmware(void) {
  if (sceKernelGetSystemSwVersion == NULL) {
    add_text("firmware.sw_version=UNKNOWN\n");
    return;
  }
  uint8_t sw_version[0x1C];
  zero_bytes(sw_version, sizeof(sw_version));
  int rc = sceKernelGetSystemSwVersion(sw_version);
  add_text("firmware.get_sw_version_rc="); add_signed(rc); add_char('\n');
  if (rc == 0) {
    add_text("firmware.sw_version_text=");
    for (size_t i = 4; i < sizeof(sw_version) && sw_version[i] != 0; ++i) {
      char c = (char)sw_version[i];
      if (c >= 32 && c <= 126) add_char(c);
      else add_char('?');
    }
    add_char('\n');
  }
}

static int usb0_is_mounted(void) {
  if (getfsstat == NULL) return 0;
  int count = getfsstat(mount_table, (long)sizeof(mount_table), MNT_NOWAIT);
  if (count <= 0) return 0;
  if (count > MOUNT_MAX) count = MOUNT_MAX;
  for (int i = 0; i < count; ++i) {
    const uint8_t *r = mount_table[i].bytes;
    if (!fixed_field_equals(r, SF_MNTONNAME, SF_NAME_LEN, "/mnt/usb0")) continue;
    int usb_device = fixed_field_prefix(r, SF_MNTFROMNAME, SF_NAME_LEN, "/dev/da");
    int fat_filesystem = fixed_field_equals(r, SF_FSTYPENAME, SF_TYPE_LEN, "exfat") ||
                         fixed_field_equals(r, SF_FSTYPENAME, SF_TYPE_LEN, "msdosfs");
    if (usb_device && fat_filesystem) return 1;
  }
  return 0;
}

static int write_all(int fd, const char *data, size_t n) {
  size_t sent = 0;
  while (sent < n) {
    long rc = (long)write(fd, data + sent, n - sent);
    if (rc <= 0) return -1;
    sent += (size_t)rc;
  }
  return 0;
}

static int send_tcp_report(void) {
  initNetwork();
  if (sceNetSocket == NULL || sceNetConnect == NULL || sceNetSend == NULL ||
      sceNetSocketClose == NULL || sceNetInetPton == NULL || sceNetHtons == NULL ||
      sceNetErrnoLoc == NULL) return -1;
  struct in_addr address;
  zero_bytes(&address, sizeof(address));
  if (sceNetInetPton(AF_INET, PC_REPORT_IP, &address) != 1) return -2;
  struct sockaddr_in peer;
  zero_bytes(&peer, sizeof(peer));
  peer.sin_len = sizeof(peer);
  peer.sin_family = AF_INET;
  peer.sin_port = sceNetHtons(PROBE_REPORT_PORT);
  peer.sin_addr = address;
  int sock = sceNetSocket("ps4-np-dual-probe", AF_INET, SOCK_STREAM, 0);
  if (sock < 0) return -3;
  if (sceNetConnect(sock, (struct sockaddr *)&peer, sizeof(peer)) < 0) {
    sceNetSocketClose(sock);
    return -4;
  }
  size_t sent = 0;
  while (sent < report_len) {
    int rc = sceNetSend(sock, report + sent, report_len - sent, 0);
    if (rc <= 0) {
      sceNetSocketClose(sock);
      return -5;
    }
    sent += (size_t)rc;
  }
  sceNetSocketClose(sock);
  return 0;
}

static int write_usb_report(void) {
  if (!usb0_is_mounted()) return -1;
  int fd = open(USB_REPORT_PATH, O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (fd < 0) return -2;
  int rc = write_all(fd, report, report_len);
  int close_rc = close(fd);
  if (rc != 0 || close_rc != 0) return -3;
  return 0;
}

static void notify_probe_status(const char *msg) {
  if (sceKernelSendNotificationRequest == NULL) return;
  SceNotificationRequest n;
  zero_bytes(&n, sizeof(n));
  n.target_id = -1;
  n.use_icon_image_uri = 1;
  const char icon[] = "cxml://psnotification/tex_icon_system";
  for (size_t i = 0; i < sizeof(icon); ++i) n.uri[i] = icon[i];
  for (size_t i = 0; i < 1023 && msg[i] != 0; ++i) n.message[i] = msg[i];
  (void)sceKernelSendNotificationRequest(0, &n, sizeof(n), 0);
}

static void deliver_report(void) {
  int tcp_rc = send_tcp_report();
  if (tcp_rc == 0) return;
  add_text("output.tcp.error="); add_signed(tcp_rc); add_char('\n');
  int usb_rc = write_usb_report();
  if (usb_rc != 0) {
    add_text("output.usb.error="); add_signed(usb_rc); add_char('\n');
    notify_probe_status("Sonda Dual: fallo de red y USB");
  }
}

static void probe_keys(void) {
  int regmgr_handle = 0;
  set_errno_zero();

  int load_rc = loadModule("libSceRegMgr.sprx", &regmgr_handle);
  int load_err = saved_errno();
  add_text("regmgr.load_module.rc="); add_signed(load_rc); add_char('\n');
  add_errno_field("regmgr.load_module", load_err);
  add_text("regmgr.handle_initial="); add_signed(regmgr_handle); add_char('\n');

  if (regmgr_handle <= 0 && sceKernelLoadStartModule != NULL) {
    set_errno_zero();
    regmgr_handle = sceKernelLoadStartModule("libSceRegMgr.sprx", 0, 0, 0, NULL, NULL);
    int start_err = saved_errno();
    add_text("regmgr.start_module.handle="); add_signed(regmgr_handle); add_char('\n');
    add_errno_field("regmgr.start_module", start_err);
  }

  if (regmgr_handle <= 0) {
    add_text("regmgr.status=MODULE_LOAD_FAILED\n");
    return;
  }
  add_text("regmgr.status=MODULE_LOADED\n");

  sceRegMgrGetStr_t p_sceRegMgrGetStr = NULL;
  sceRegMgrGetInt_t p_sceRegMgrGetInt = NULL;

  set_errno_zero();
  int dlsym_str_rc = getFunctionAddressByName(regmgr_handle, "sceRegMgrGetStr", &p_sceRegMgrGetStr);
  int dlsym_str_err = saved_errno();
  add_text("regmgr.dlsym_str.rc="); add_signed(dlsym_str_rc); add_char('\n');
  add_errno_field("regmgr.dlsym_str", dlsym_str_err);

  set_errno_zero();
  int dlsym_int_rc = getFunctionAddressByName(regmgr_handle, "sceRegMgrGetInt", &p_sceRegMgrGetInt);
  int dlsym_int_err = saved_errno();
  add_text("regmgr.dlsym_int.rc="); add_signed(dlsym_int_rc); add_char('\n');
  add_errno_field("regmgr.dlsym_int", dlsym_int_err);

  if (p_sceRegMgrGetStr == NULL || p_sceRegMgrGetInt == NULL) {
    add_text("regmgr.status=DLSYM_FAILED\n");
    return;
  }
  add_text("regmgr.status=RESOLVED\n");

  /* 1. Clave 0x19800000: NP Environment de Sistema (GetStr, buffer 64 bytes, campo registro 17 bytes) */
  {
    char val_buf[64];
    zero_bytes(val_buf, sizeof(val_buf));
    add_text("reg.np_env_sys.key=0x19800000 (427819008)\n");
    set_errno_zero();
    int32_t rc = p_sceRegMgrGetStr((int32_t)KEY_NP_ENV_SYS, val_buf, sizeof(val_buf) - 1);
    int err = saved_errno();
    add_text("reg.np_env_sys.rc="); add_signed(rc); add_char('\n');
    add_text("reg.np_env_sys.errno="); add_signed(err); add_char('\n');
    add_text("reg.np_env_sys.val_str=");
    for (size_t c = 0; c < sizeof(val_buf) && val_buf[c] != 0; ++c) {
      char ch = val_buf[c];
      add_char((ch >= 32 && ch <= 126) ? ch : '?');
    }
    add_char('\n');
    /* Mostrar exactamente los 17 bytes del campo del registro */
    add_hex_span("reg.np_env_sys.hex17", (const uint8_t *)val_buf, 17);
  }

  /* 2. Clave 0x0780B007: NP Environment de Usuario 1 (GetStr, contraste de ámbito) */
  {
    char val_buf[64];
    zero_bytes(val_buf, sizeof(val_buf));
    add_text("reg.np_env_user1.key=0x0780b007 (125874183)\n");
    set_errno_zero();
    int32_t rc = p_sceRegMgrGetStr((int32_t)KEY_NP_ENV_USER1, val_buf, sizeof(val_buf) - 1);
    int err = saved_errno();
    add_text("reg.np_env_user1.rc="); add_signed(rc); add_char('\n');
    add_text("reg.np_env_user1.errno="); add_signed(err); add_char('\n');
    add_text("reg.np_env_user1.val_str=");
    for (size_t c = 0; c < sizeof(val_buf) && val_buf[c] != 0; ++c) {
      char ch = val_buf[c];
      add_char((ch >= 32 && ch <= 126) ? ch : '?');
    }
    add_char('\n');
    add_hex_span("reg.np_env_user1.hex17", (const uint8_t *)val_buf, 17);
  }

  /* 3. Clave 0x02800500: Descargas automáticas de actualización (GetInt, Int32) */
  {
    int32_t int_val = 0;
    add_text("reg.auto_download.key=0x02800500 (41944320)\n");
    set_errno_zero();
    int32_t rc = p_sceRegMgrGetInt((int32_t)KEY_UPDATE_AUTODL, &int_val);
    int err = saved_errno();
    add_text("reg.auto_download.rc="); add_signed(rc); add_char('\n');
    add_text("reg.auto_download.errno="); add_signed(err); add_char('\n');
    add_text("reg.auto_download.val_int="); add_signed((int64_t)int_val); add_char('\n');
    add_text("reg.auto_download.val_hex=0x"); add_hex_u32((uint32_t)int_val); add_char('\n');
  }
}

int _main(void *thread_data) {
  (void)thread_data;
  report_len = 0;
  report[0] = 0;
  add_text("probe=ps4_np_readonly_dual_probe\n");
  add_text("target=PS4_FW_9.00\n");
  add_text("mode=READ_ONLY_REGISTRY_DUAL\n");
  add_text("output.tcp="); add_text(PC_REPORT_IP); add_char(':');
  add_unsigned(PROBE_REPORT_PORT); add_char('\n');

  initKernel();
  int init_rc = (getuid != NULL && getgid != NULL && __error != NULL &&
                 sysctlbyname != NULL && sceKernelGetSystemSwVersion != NULL &&
                 sceKernelLoadStartModule != NULL &&
                 sceKernelSendNotificationRequest != NULL) ? 0 : -1;
  add_text("kernel_api.init_rc="); add_signed(init_rc); add_char('\n');
  if (init_rc != 0) {
    add_text("precondition=UNKNOWN; no further probes performed\n");
    add_text("probe.completed=precondition-unknown\n");
    deliver_report();
    return 0;
  }

  int uid = getuid();
  int gid = getgid();
  int sandbox = is_in_sandbox();
  add_text("identity.uid="); add_signed(uid); add_char('\n');
  add_text("identity.gid="); add_signed(gid); add_char('\n');
  add_text("identity.is_in_sandbox="); add_signed(sandbox); add_char('\n');
  if (uid != 0 || sandbox != 0) {
    add_text("precondition=FAIL\nmessage=carga GoldHEN antes de este payload\n");
    add_text("probe.completed=precondition-failed\n");
    deliver_report();
    return 0;
  }
  add_text("precondition=PASS\n");
  report_firmware();

  /* Ejecutar inspección de solo lectura */
  probe_keys();

  add_text("probe.completed=yes\n");
  deliver_report();
  notify_probe_status("Sonda Dual: lectura completada");
  return 0;
}
