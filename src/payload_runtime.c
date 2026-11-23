/* Inicialización mínima, logging y conexión TCP común a los BIN generados. */
#include "payload_runtime.h"
#include "module.h"
#include "syscall.h"

typedef int (*fn_socket)(const char *, int, int, int);
typedef int (*fn_close)(int);
typedef int (*fn_connect)(int, struct sockaddr *, int);
typedef int (*fn_send)(int, const void *, size_t, int);
typedef int (*fn_pton)(int, const char *, void *);
typedef uint16_t (*fn_htons)(uint16_t);

char payload_report[REPORT_CAP] = {0xA5};
size_t payload_report_len = 1;
int *(*__error)(void);
int (*payload_getuid)(void);
int (*payload_getgid)(void);
int (*payload_sysctlbyname)(char *, char *, size_t *, char *, size_t);
int (*payload_get_sw_version)(SceFwInfo *);
int (*payload_sce_load_module)(const char *, size_t, const void *, unsigned int, int, int);
int libKernelHandle;

static fn_socket net_socket;
static fn_close net_close;
static fn_connect net_connect;
static fn_send net_send;
static fn_pton net_pton;
static fn_htons net_htons;
static int (*send_notification)(int, SceNotificationRequest *, size_t, int);

extern int getFunctionAddressByName(int module_id, char *name, void *destination);

void report_start(const char *name) {
  payload_report_len = 0;
  payload_report[0] = 0;
  report_text("payload=");
  report_text(name);
  report_text("\n");
  report_text("report_endpoint=" REPORT_HOST ":");
  report_u64(REPORT_PORT);
  report_text("\n");
}

void report_char(char c) {
  if (payload_report_len + 1 >= REPORT_CAP) return;
  payload_report[payload_report_len++] = c;
  payload_report[payload_report_len] = 0;
}

void report_text(const char *text) {
  if (text == 0) return;
  while (*text) report_char(*text++);
}

void report_u64(uint64_t value) {
  char digits[24];
  unsigned int n = 0;
  do {
    digits[n++] = (char)('0' + (value % 10));
    value /= 10;
  } while (value && n < sizeof(digits));
  while (n) report_char(digits[--n]);
}

void report_i64(int64_t value) {
  if (value < 0) {
    report_char('-');
    report_u64((uint64_t)(-(value + 1)) + 1);
  } else {
    report_u64((uint64_t)value);
  }
}

void report_hex(uint8_t value) {
  const char *hex = "0123456789abcdef";
  report_char(hex[value >> 4]);
  report_char(hex[value & 15]);
}

static void report_u32_hex(uint32_t value) {
  report_hex((uint8_t)(value >> 24));
  report_hex((uint8_t)(value >> 16));
  report_hex((uint8_t)(value >> 8));
  report_hex((uint8_t)value);
}

void report_bytes(const char *key, const uint8_t *bytes, size_t len) {
  report_text(key);
  report_char('=');
  for (size_t i = 0; i < len; ++i) {
    if (i) report_char(' ');
    report_hex(bytes[i]);
  }
  report_char('\n');
}

int payload_errno(void) {
  return __error ? *__error() : 0;
}

static int resolve(int module_id, char *name, void *destination) {
  return getFunctionAddressByName(module_id, name, destination);
}

static int load_kernel(void) {
  /* BSS de un BIN raw no se presupone inicializado por el loader. */
  __error = 0;
  payload_getuid = 0;
  payload_getgid = 0;
  payload_sysctlbyname = 0;
  payload_get_sw_version = 0;
  payload_sce_load_module = 0;
  send_notification = 0;
  libKernelHandle = 0;
  int result = payload_sys_load_module("libkernel.sprx", 0, &libKernelHandle, 0);
  if (result) result = payload_sys_load_module("libkernel_web.sprx", 0, &libKernelHandle, 0);
  if (result) result = payload_sys_load_module("libkernel_sys.sprx", 0, &libKernelHandle, 0);
  if (result) return result;

  resolve(libKernelHandle, "__error", &__error);
  resolve(libKernelHandle, "getuid", &payload_getuid);
  resolve(libKernelHandle, "getgid", &payload_getgid);
  resolve(libKernelHandle, "sceKernelLoadStartModule", &payload_sce_load_module);
  resolve(libKernelHandle, "sceKernelGetSystemSwVersion", &payload_get_sw_version);
  resolve(libKernelHandle, "sysctlbyname", &payload_sysctlbyname);
  resolve(libKernelHandle, "sceKernelSendNotificationRequest", &send_notification);
  return (!__error || !payload_getuid || !payload_getgid ||
          !payload_sce_load_module) ? -1 : 0;
}

int payload_check_runtime(void) {
  /* El nombre del módulo y sus exports se resuelven en runtime; no hay offsets. */
  int kernel_rc = load_kernel();
  report_text("kernel_api.rc="); report_i64(kernel_rc); report_char('\n');
  if (kernel_rc != 0) {
    report_text("precondition=UNKNOWN\nreason=libkernel API no disponible\n");
    (void)payload_notify("Carga GoldHEN antes de este payload");
    return 0;
  }

  int uid = payload_getuid();
  int gid = payload_getgid();
  int sandbox = payload_in_sandbox();
  report_text("identity.uid="); report_i64(uid); report_char('\n');
  report_text("identity.gid="); report_i64(gid); report_char('\n');
  report_text("identity.is_in_sandbox="); report_i64(sandbox); report_char('\n');
  if (uid != 0 || sandbox != 0) {
    report_text("precondition=FAIL\nmessage=carga GoldHEN primero\n");
    (void)payload_notify("Carga GoldHEN antes de este payload");
    return 0;
  }
  report_text("precondition=PASS\n");
  payload_log_firmware();
  return 1;
}

void payload_log_firmware(void) {
  if (payload_get_sw_version) {
    SceFwInfo info = {0};
    int rc = payload_get_sw_version(&info);
    report_text("fw.sce.rc="); report_i64(rc); report_char('\n');
    report_text("fw.sce.version=");
    for (size_t i = 0; i < sizeof(info.version_string) && info.version_string[i]; ++i)
      report_char(info.version_string[i]);
    report_char('\n');
    report_text("fw.sce.numeric="); report_u64(info.version); report_char('\n');
    report_text("fw.sce.numeric_hex=0x"); report_u32_hex(info.version); report_char('\n');
  }
  if (payload_sysctlbyname) {
    char key[] = "kern.sdk_version";
    uint32_t sdk = 0;
    size_t size = sizeof(sdk);
    int rc = payload_sysctlbyname(key, (char *)&sdk, &size, 0, 0);
    report_text("fw.sysctl.rc="); report_i64(rc); report_char('\n');
    report_text("fw.sysctl.size="); report_u64(size); report_char('\n');
    if (rc == 0 && size == sizeof(sdk)) {
      report_text("fw.sysctl.raw=");
      report_hex((uint8_t)sdk); report_char(' ');
      report_hex((uint8_t)(sdk >> 8)); report_char(' ');
      report_hex((uint8_t)(sdk >> 16)); report_char(' ');
      report_hex((uint8_t)(sdk >> 24)); report_char('\n');
      report_text("fw.sysctl.u32le=0x"); report_u32_hex(sdk); report_char('\n');
    }
  }
}

void payload_load_network_api(void) {
  net_socket = 0;
  net_close = 0;
  net_connect = 0;
  net_send = 0;
  net_pton = 0;
  net_htons = 0;
  if (!payload_sce_load_module) {
    report_text("network.status=ABORT_NO_KERNEL_MODULE_API\n");
    return;
  }
  int rc = payload_sce_load_module("libSceNet.sprx", 0, 0, 0, 0, 0);
  if (rc < 0) {
    report_text("network.module.rc="); report_i64(rc); report_char('\n');
    return;
  }
  resolve(rc, "sceNetSocket", &net_socket);
  resolve(rc, "sceNetSocketClose", &net_close);
  resolve(rc, "sceNetConnect", &net_connect);
  resolve(rc, "sceNetSend", &net_send);
  resolve(rc, "sceNetInetPton", &net_pton);
  resolve(rc, "sceNetHtons", &net_htons);
}

int payload_send_report(void) {
  payload_load_network_api();
  if (!net_socket || !net_close || !net_connect || !net_send || !net_pton || !net_htons) return -1;
  struct in_addr address = {0};
  if (net_pton(AF_INET, REPORT_HOST, &address) != 1) return -2;
  struct sockaddr_in peer = {0};
  peer.sin_len = sizeof(peer);
  peer.sin_family = AF_INET;
  peer.sin_port = net_htons(REPORT_PORT);
  peer.sin_addr = address;
  int fd = net_socket("ps4-universal-payload", AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return -3;
  if (net_connect(fd, (struct sockaddr *)&peer, sizeof(peer)) < 0) {
    net_close(fd);
    return -4;
  }
  size_t sent = 0;
  while (sent < payload_report_len) {
    int n = net_send(fd, payload_report + sent, payload_report_len - sent, 0);
    if (n <= 0) {
      net_close(fd);
      return -5;
    }
    sent += (size_t)n;
  }
  net_close(fd);
  return 0;
}

/* Estructura del SDK fijado: sizeof(SceNotificationRequest) = 0xC30;
 * message dispone de 1024 bytes. Se envía UTF-8 como hace printf_notification.
 */
_Static_assert(sizeof(SceNotificationRequest) == 0xC30,
               "Tamaño de notificación inesperado en el SDK");

int payload_notify(const char *message) {
  if (!send_notification || !message) return -1;
  SceNotificationRequest request = {0};
  request.target_id = -1;
  request.use_icon_image_uri = 1;
  static const char icon[] = "cxml://psnotification/tex_icon_system";
  for (size_t i = 0; i < sizeof(icon); ++i) request.uri[i] = icon[i];
  size_t i = 0;
  while (i + 1 < sizeof(request.message) && message[i]) {
    request.message[i] = message[i];
    ++i;
  }
  request.message[i] = 0;
  return send_notification(0, &request, sizeof(request), 0);
}

static size_t append_text(char *out, size_t pos, size_t cap, const char *text) {
  while (*text && pos + 1 < cap) out[pos++] = *text++;
  out[pos] = 0;
  return pos;
}

static size_t append_number(char *out, size_t pos, size_t cap, unsigned int value) {
  char digits[10];
  unsigned int count = 0;
  do {
    digits[count++] = (char)('0' + value % 10);
    value /= 10;
  } while (value && count < sizeof(digits));
  while (count && pos + 1 < cap) out[pos++] = digits[--count];
  out[pos] = 0;
  return pos;
}

void payload_notify_fan(unsigned int target, unsigned int before, int is_default) {
  char text[128] = {0};
  size_t pos = 0;
  if (is_default) {
    (void)payload_notify("Ventilador: umbral original restaurado (79 \xC2\xB0" "C)");
    return;
  }
  if (target == 80) {
    pos = append_text(text, pos, sizeof(text), "Ventilador: 80 \xC2\xB0" "C (antes ");
    pos = append_number(text, pos, sizeof(text), before);
    (void)append_text(text, pos, sizeof(text), " \xC2\xB0" "C). Superior al original");
    (void)payload_notify(text);
    return;
  }
  pos = append_text(text, pos, sizeof(text), "Ventilador ajustado: ");
  pos = append_number(text, pos, sizeof(text), target);
  pos = append_text(text, pos, sizeof(text), " \xC2\xB0" "C (antes ");
  pos = append_number(text, pos, sizeof(text), before);
  (void)append_text(text, pos, sizeof(text), " \xC2\xB0" "C)");
  (void)payload_notify(text);
}

void payload_notify_updates(int enabling, int success) {
  if (!success) {
    (void)payload_notify(enabling
      ? "No se pudieron habilitar actualizaciones. Revise el log"
      : "No se pudieron bloquear actualizaciones. Revise el log");
  } else {
    (void)payload_notify(enabling
      ? "Actualizaciones habilitadas. Pueden descargarse"
      : "Actualizaciones bloqueadas. Se mantienen al apagar");
  }
}
