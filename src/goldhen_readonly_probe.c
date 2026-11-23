/*
 * Sonda de solo lectura para GoldHEN / PS4 9.00.
 *
 * No contiene jailbreak(), kexec, syscall 11 ni offsets de kernel.
 * Las únicas operaciones sobre el sistema de archivos son stat/open O_RDONLY;
 * el único ioctl ICC es GET 0xC01C8F08. El fallback de archivo solo se habilita
 * si getfsstat confirma /mnt/usb0 como USB FAT/exFAT montado.
 */
#include <stdint.h>
#include "types.h"
#include "kernel.h"
#include "file.h"
#include "network.h"
#include "syscall.h"

#define STATFS_SIZE       0x1D8 /* FreeBSD 9, layout empleado por WebRTE PS4 */
#define MNT_NOWAIT        2
#define MOUNT_MAX         192
#define SF_FSTYPENAME     0x118
#define SF_MNTFROMNAME    0x128
#define SF_MNTONNAME      0x180
#define SF_NAME_LEN       88
#define SF_TYPE_LEN       16
#define REPORT_CAP        8192
#define ICC_GET_TABLE     0xC01C8F08UL
#define USB_REPORT_PATH   "/mnt/usb0/ps4_readonly_probe.txt"
#ifndef PROBE_REPORT_IP
#error "Compile con -DPROBE_REPORT_IP=\"IPv4_DEL_PC\""
#endif
#ifndef PROBE_REPORT_PORT
#define PROBE_REPORT_PORT 9090
#endif
#define PC_REPORT_IP      PROBE_REPORT_IP

/* Variables definidas por network.c del SDK. */
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
extern int getdents(int, char *, int);

/* Registro de montajes FreeBSD 9, de tamaño/offsets contrastados en WebRTE. */
typedef struct {
  uint8_t bytes[STATFS_SIZE];
} ps4_statfs_record;

/* Inicializador no nulo fuerza el buffer grande a estar dentro del BIN raw. */
static ps4_statfs_record mount_table[MOUNT_MAX] = {{{0xA5}}};
static char report[REPORT_CAP] = {0xA5};
static size_t report_len = 1;

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

static void add_errno_field(const char *name, int value) {
  add_text(name);
  add_text(".errno=");
  add_signed(value);
  add_char('\n');
}

static void add_hex_span(const char *name, const uint8_t *data, size_t n) {
  add_text(name);
  add_char('=');
  for (size_t i = 0; i < n; ++i) {
    if (i != 0) add_char(' ');
    add_hex_byte(data[i]);
  }
  add_char('\n');
}

static void zero_bytes(void *memory, size_t n) {
  volatile uint8_t *p = (volatile uint8_t *)memory;
  for (size_t i = 0; i < n; ++i) p[i] = 0;
}

static int bytes_equal(const uint8_t *a, const uint8_t *b, size_t n) {
  uint8_t difference = 0;
  for (size_t i = 0; i < n; ++i) difference |= (uint8_t)(a[i] ^ b[i]);
  return difference == 0;
}

static int sandbox_state(void) {
  /* Se usa directamente el wrapper is_in_sandbox() del SDK. */
  return is_in_sandbox();
}

static void report_firmware(void) {
  SceFwInfo fw;
  zero_bytes(&fw, sizeof(fw));
  set_errno_zero();
  int rc = sceKernelGetSystemSwVersion != NULL ? sceKernelGetSystemSwVersion(&fw) : -1;
  int err = saved_errno();
  add_text("fw.sce.rc="); add_signed(rc); add_char('\n');
  add_errno_field("fw.sce", err);
  if (rc == 0) {
    add_text("fw.sce.version=");
    for (size_t i = 0; i < sizeof(fw.version_string) && fw.version_string[i] != 0; ++i)
      add_char(fw.version_string[i]);
    add_text("\nfw.sce.numeric="); add_unsigned(fw.version);
    add_char('\n');
    add_hex_span("fw.sce.version_string_hex", (const uint8_t *)fw.version_string,
                 sizeof(fw.version_string));
  }

  char sdk[64];
  zero_bytes(sdk, sizeof(sdk));
  size_t sdk_len = sizeof(sdk) - 1;
  set_errno_zero();
  rc = sysctlbyname != NULL ? sysctlbyname("kern.sdk_version", sdk, &sdk_len, NULL, 0) : -1;
  err = saved_errno();
  add_text("fw.sysctl.rc="); add_signed(rc); add_char('\n');
  add_errno_field("fw.sysctl", err);
  add_text("fw.sysctl.length="); add_unsigned(sdk_len); add_char('\n');
  if (sdk_len > sizeof(sdk) - 1) sdk_len = sizeof(sdk) - 1;
  if (rc == 0) {
    add_hex_span("fw.sysctl.hex", (const uint8_t *)sdk, sdk_len);
    if (sdk_len == sizeof(uint32_t)) {
      uint32_t value = (uint32_t)(uint8_t)sdk[0] |
                       ((uint32_t)(uint8_t)sdk[1] << 8) |
                       ((uint32_t)(uint8_t)sdk[2] << 16) |
                       ((uint32_t)(uint8_t)sdk[3] << 24);
      add_text("fw.sysctl.u32le=0x");
      add_hex_byte((uint8_t)(value >> 24));
      add_hex_byte((uint8_t)(value >> 16));
      add_hex_byte((uint8_t)(value >> 8));
      add_hex_byte((uint8_t)value);
      add_char('\n');
    }
  }
}

/* Cuenta entradas reales excluyendo . y ..; no abre con flags de escritura. */
static void report_directory_entry_count(const char *path, const char *label) {
  char key[64];
  size_t key_len = 0;
  while (label[key_len] != 0 && key_len + 16 < sizeof(key)) {
    key[key_len] = label[key_len];
    ++key_len;
  }
  key[key_len] = 0;

  set_errno_zero();
  int fd = open(path, O_RDONLY, 0);
  int open_err = saved_errno();
  add_text(key); add_text(".entries.open.rc="); add_signed(fd); add_char('\n');
  add_text(key); add_text(".entries.open.errno="); add_signed(open_err); add_char('\n');
  if (fd < 0) {
    add_text(key); add_text(".entries.count=UNKNOWN\n");
    return;
  }

  char buffer[1024];
  uint64_t count = 0;
  int read_error = 0;
  int complete = 0;
  for (unsigned int batch = 0; batch < 4096; ++batch) {
    set_errno_zero();
    int bytes = getdents(fd, buffer, sizeof(buffer));
    int err = saved_errno();
    if (bytes < 0) { read_error = err; break; }
    if (bytes == 0) { complete = 1; break; }

    int offset = 0;
    while (offset < bytes) {
      if (bytes - offset < 8) { read_error = 5; break; }
      const uint8_t *entry = (const uint8_t *)buffer + offset;
      unsigned int reclen = (unsigned int)entry[4] | ((unsigned int)entry[5] << 8);
      unsigned int namelen = entry[7];
      if (reclen < 8 || reclen > (unsigned int)(bytes - offset) ||
          namelen + 9 > reclen) { read_error = 5; break; }
      const char *name = (const char *)(entry + 8);
      if (!(namelen == 1 && name[0] == '.') &&
          !(namelen == 2 && name[0] == '.' && name[1] == '.')) {
        ++count;
      }
      offset += (int)reclen;
    }
    if (read_error != 0) break;
  }

  set_errno_zero();
  int close_rc = close(fd);
  int close_err = saved_errno();
  if (complete && read_error == 0 && close_rc == 0) {
    add_text(key); add_text(".entries.count="); add_unsigned(count); add_char('\n');
    add_text(key); add_text(".entries.status=OK\n");
  } else {
    add_text(key); add_text(".entries.count=UNKNOWN\n");
    add_text(key); add_text(".entries.status=ERROR\n");
    add_text(key); add_text(".entries.read_errno="); add_signed(read_error); add_char('\n');
    add_text(key); add_text(".entries.close_rc="); add_signed(close_rc); add_char('\n');
    add_text(key); add_text(".entries.close_errno="); add_signed(close_err); add_char('\n');
  }
}

static void report_path(const char *path, const char *label) {
  struct stat st;
  zero_bytes(&st, sizeof(st));
  set_errno_zero();
  int rc = stat(path, &st); /* syscall stat del SDK, solo lectura */
  int err = saved_errno();
  add_text(label); add_text(".stat.rc="); add_signed(rc); add_char('\n');
  add_errno_field(label, err);
  if (rc != 0) return;
  add_text(label); add_text(".mode_raw="); add_unsigned((uint64_t)st.st_mode); add_char('\n');
  add_text(label); add_text(".permissions_octal=");
  uint16_t mode = (uint16_t)(st.st_mode & 07777);
  add_char((char)('0' + ((mode >> 9) & 7)));
  add_char((char)('0' + ((mode >> 6) & 7)));
  add_char((char)('0' + ((mode >> 3) & 7)));
  add_char((char)('0' + (mode & 7)));
  add_char('\n');
  add_text(label); add_text(".size="); add_signed(st.st_size); add_char('\n');
  add_text(label); add_text(".uid="); add_unsigned(st.st_uid); add_char('\n');
  add_text(label); add_text(".type=");
  if (S_ISREG(st.st_mode)) add_text("file");
  else if (S_ISDIR(st.st_mode)) add_text("directory");
  else if (S_ISCHR(st.st_mode)) add_text("character-device");
  else if (S_ISLNK(st.st_mode)) add_text("symlink-target");
  else add_text("other");
  add_char('\n');
  if (S_ISDIR(st.st_mode)) report_directory_entry_count(path, label);
  else {
    add_text(label); add_text(".entries.status=NOT_DIRECTORY\n");
  }
}

static int read_fan_state(void) {
  report_path("/dev/icc_fan", "icc.stat");
  set_errno_zero();
  int fd = open("/dev/icc_fan", O_RDONLY, 0);
  int err = saved_errno();
  add_text("icc.open.rc="); add_signed(fd); add_char('\n');
  add_errno_field("icc.open", err);
  if (fd < 0) return -1;

  uint8_t first[64];
  uint8_t second[64];
  zero_bytes(first, sizeof(first));
  zero_bytes(second, sizeof(second));
  set_errno_zero();
  int rc1 = ioctl(fd, ICC_GET_TABLE, first);
  int err1 = saved_errno();
  set_errno_zero();
  int rc2 = ioctl(fd, ICC_GET_TABLE, second);
  int err2 = saved_errno();
  add_text("icc.get1.rc="); add_signed(rc1); add_char('\n');
  add_errno_field("icc.get1", err1);
  add_hex_span("icc.get1.hex28", first, 28);
  add_text("icc.get2.rc="); add_signed(rc2); add_char('\n');
  add_errno_field("icc.get2", err2);
  add_hex_span("icc.get2.hex28", second, 28);
  add_text("icc.get.stable=");
  add_text((rc1 == 0 && rc2 == 0 && bytes_equal(first, second, 28)) ? "yes\n" : "no\n");

  set_errno_zero();
  int close_rc = close(fd);
  int close_errno = saved_errno();
  add_text("icc.close.rc="); add_signed(close_rc); add_char('\n');
  add_errno_field("icc.close", close_errno);
  return 0;
}

static int fixed_field_equals(const uint8_t *record, size_t offset,
                              size_t field_len, const char *wanted) {
  size_t i = 0;
  while (i < field_len && wanted[i] != 0) {
    if (record[offset + i] != (uint8_t)wanted[i]) return 0;
    ++i;
  }
  if (wanted[i] != 0) return 0;
  return i == field_len || record[offset + i] == 0;
}

static int fixed_field_prefix(const uint8_t *record, size_t offset,
                              size_t field_len, const char *prefix) {
  for (size_t i = 0; prefix[i] != 0; ++i) {
    if (i >= field_len || record[offset + i] != (uint8_t)prefix[i]) return 0;
  }
  return 1;
}

/* Solo considera USB si getfsstat muestra el punto de montaje y dispositivo. */
static int usb0_is_mounted(void) {
  int count = getfsstat(NULL, 0, MNT_NOWAIT);
  if (count <= 0) return 0;
  if (count > MOUNT_MAX) count = MOUNT_MAX;
  long bytes = (long)count * STATFS_SIZE;
  zero_bytes(mount_table, sizeof(mount_table));
  int got = getfsstat(mount_table, bytes, MNT_NOWAIT);
  if (got <= 0) return 0;
  if (got > count) got = count;
  for (int i = 0; i < got; ++i) {
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
  int sock = sceNetSocket("ps4-readonly-probe", AF_INET, SOCK_STREAM, 0);
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
  /* O_EXCL evita truncar/sobrescribir cualquier reporte previo. */
  int fd = open(USB_REPORT_PATH, O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (fd < 0) return -2;
  int rc = write_all(fd, report, report_len);
  int write_errno = saved_errno();
  int close_rc = close(fd);
  if (rc != 0 || close_rc != 0) {
    (void)write_errno;
    return -3;
  }
  return 0;
}

static void notify_output_failure(void) {
  if (sceKernelSendNotificationRequest == NULL) return;
  SceNotificationRequest n;
  zero_bytes(&n, sizeof(n));
  n.target_id = -1;
  n.use_icon_image_uri = 1;
  const char icon[] = "cxml://psnotification/tex_icon_system";
  const char text[] = "Sonda: fallo de red; USB no disponible.";
  for (size_t i = 0; i < sizeof(icon); ++i) n.uri[i] = icon[i];
  for (size_t i = 0; i < sizeof(text); ++i) n.message[i] = text[i];
  (void)sceKernelSendNotificationRequest(0, &n, sizeof(n), 0);
}

static void deliver_report(void) {
  int tcp_rc = send_tcp_report();
  if (tcp_rc == 0) return;
  add_text("output.tcp.error="); add_signed(tcp_rc); add_char('\n');
  int usb_rc = write_usb_report();
  if (usb_rc != 0) {
    add_text("output.usb.error="); add_signed(usb_rc); add_char('\n');
    notify_output_failure();
  }
}

int _main(void *thread_data) {
  (void)thread_data;
  report_len = 0;
  report[0] = 0;
  add_text("probe=goldhen_readonly_probe\n");
  add_text("target=PS4_FW_9.00\n");
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
  int sandbox = sandbox_state();
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
  add_text("goldhen.version=UNKNOWN (sin API publica documentada)\n");
  report_firmware();
  (void)read_fan_state();
  report_path("/update/PS4UPDATE.PUP", "update.pup");
  report_path("/update/PS4UPDATE.PUP.net.temp", "update.net_temp");
  add_text("probe.completed=yes\n");
  deliver_report();
  return 0;
}
