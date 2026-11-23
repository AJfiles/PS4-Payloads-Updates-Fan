/*
 * disable_updates_full.c — Bloqueo completo de actualizaciones en 3 capas.
 * Capa 1: Filesystem (directorios señuelo vacíos en /update)
 * Capa 2: Registro de sistema: SCE_REGMGR_ENT_KEY_SYSTEM_UPDATE_auto_download = 0 (OFF)
 * Capa 3: Registro de sistema: SCE_REGMGR_ENT_KEY_NP_env = "noupdt" (17 bytes con relleno cero)
 *
 * REGLAS DE SEGURIDAD Y PIC (Regla 12.3):
 * - Orden estricto: Capa 1 -> Capa 2 -> Capa 3 (NP siempre al final).
 * - Si Capa 1 falla, aborta inmediatamente sin tocar el registro.
 * - Reversión por capa: si Capa 2 falla al verificar, se revierte solo Capa 2.
 *   Si Capa 3 falla al verificar, se revierte solo Capa 3. No se revierten capas exitosas.
 * - Degradación elegante: si libSceRegMgr no carga o falla dlsym, Capa 1 queda aplicada (1/3).
 * - Matriz contigua para rutas (sin tablas de punteros absolutos en BIN raw PIC).
 * - Sin kexec, jailbreak ni syscall 11.
 * - Destino TCP incrustado en tiempo de compilación (-DREPORT_HOST).
 */
#include "payload_runtime.h"

extern int getFunctionAddressByName(int module_id, const char *name, void *destination);

#define ROUTE_COUNT 2
#define MODE_DIR 0040000
#define MODE_FILE 0100000
#define MODE_PERMS 07777
#define DIR_BUFFER_SIZE 1024

#define KEY_SYSTEM_UPDATE_AUTODL  0x02800500U
#define KEY_SYSTEM_NP_ENV         0x19800000U

/* Matriz contigua: evita tabla de punteros absolutos en el BIN raw PIC */
static const char update_paths[ROUTE_COUNT][36] = {
  "/update/PS4UPDATE.PUP.net.temp",
  "/update/PS4UPDATE.PUP"
};

enum route_state { ROUTE_ERROR = -1, ROUTE_MISSING = 0, ROUTE_EMPTY_DIR = 1,
                   ROUTE_FILE = 2, ROUTE_UNSAFE = 3 };

typedef int32_t (*sceRegMgrGetStr_t)(int32_t optionId, char *outString, size_t stringLength);
typedef int32_t (*sceRegMgrGetInt_t)(int32_t optionId, int32_t *outInt);
typedef int32_t (*sceRegMgrSetStr_t)(int32_t optionId, const char *inString, size_t stringLength);
typedef int32_t (*sceRegMgrSetInt_t)(int32_t optionId, int32_t inInt);

static void clear_errno(void) {
  if (__error && *__error) *__error() = 0;
}

static int mem_equal(const void *a, const void *b, size_t n) {
  const uint8_t *p1 = (const uint8_t *)a;
  const uint8_t *p2 = (const uint8_t *)b;
  for (size_t i = 0; i < n; ++i) {
    if (p1[i] != p2[i]) return 0;
  }
  return 1;
}

static void mem_zero(void *dst, size_t n) {
  uint8_t *p = (uint8_t *)dst;
  for (size_t i = 0; i < n; ++i) p[i] = 0;
}

static void report_hex17(const char *key, const uint8_t *bytes) {
  report_text(key);
  report_char('=');
  for (size_t i = 0; i < 17; ++i) {
    report_hex(bytes[i]);
  }
  report_char('\n');
}

static int directory_is_empty(const char *path) {
  int fd = payload_open(path, O_RDONLY, 0);
  if (fd < 0) return -1;
  char buffer[DIR_BUFFER_SIZE];
  int empty = 1;
  for (;;) {
    clear_errno();
    int bytes = payload_getdents(fd, buffer, sizeof(buffer));
    if (bytes < 0) { empty = -1; break; }
    if (bytes == 0) break;
    int offset = 0;
    while (offset < bytes) {
      if (bytes - offset < 8) { empty = -1; break; }
      const uint8_t *entry = (const uint8_t *)buffer + offset;
      unsigned int reclen = (unsigned int)entry[4] | ((unsigned int)entry[5] << 8);
      unsigned int namelen = entry[7];
      if (reclen < 8 || reclen > (unsigned int)(bytes - offset) ||
          namelen + 9 > reclen) { empty = -1; break; }
      const char *name = (const char *)(entry + 8);
      if (!(namelen == 1 && name[0] == '.') &&
          !(namelen == 2 && name[0] == '.' && name[1] == '.')) {
        empty = 0;
      }
      offset += (int)reclen;
    }
    if (empty <= 0) break;
  }
  int saved = payload_errno();
  clear_errno();
  int close_rc = payload_close(fd);
  int close_err = payload_errno();
  if (close_rc != 0) {
    report_text("directory.close.errno="); report_i64(close_err); report_char('\n');
    return -1;
  }
  if (empty < 0) {
    report_text("directory.read.errno="); report_i64(saved); report_char('\n');
  }
  return empty;
}

static enum route_state inspect_route(unsigned int index, struct stat *st) {
  report_text("route."); report_u64(index); report_text(".path=");
  report_text(update_paths[index]); report_char('\n');
  clear_errno();
  int rc = payload_lstat(update_paths[index], st);
  int err = payload_errno();
  report_text("route."); report_u64(index); report_text(".before.lstat.rc=");
  report_i64(rc); report_char('\n');
  report_text("route."); report_u64(index); report_text(".before.errno=");
  report_i64(err); report_char('\n');
  if (rc != 0) {
    report_text("route."); report_u64(index); report_text(".before.type=");
    report_text(err == 2 ? "missing\n" : "unknown\n");
    return err == 2 ? ROUTE_MISSING : ROUTE_ERROR;
  }

  report_text("route."); report_u64(index); report_text(".before.type=");
  if (S_ISDIR(st->st_mode)) report_text("directory\n");
  else if (S_ISREG(st->st_mode)) report_text("file\n");
  else if (S_ISLNK(st->st_mode)) report_text("symlink\n");
  else report_text("other\n");

  if (S_ISDIR(st->st_mode)) {
    int empty = directory_is_empty(update_paths[index]);
    report_text("route."); report_u64(index); report_text(".before.empty=");
    report_text(empty == 1 ? "yes\n" : empty == 0 ? "no\n" : "UNKNOWN\n");
    return empty == 1 ? ROUTE_EMPTY_DIR : empty == 0 ? ROUTE_UNSAFE : ROUTE_ERROR;
  }
  if (S_ISREG(st->st_mode)) return ROUTE_FILE;
  return ROUTE_UNSAFE;
}

static int mkdir_0777(const char *path) {
  int prior_mask = payload_umask(0);
  if (prior_mask < 0) return -1;
  clear_errno();
  int rc = payload_mkdir(path, 0777);
  int err = payload_errno();
  payload_umask(prior_mask);
  report_text("mkdir.rc="); report_i64(rc); report_text(".errno=");
  report_i64(err); report_char('\n');
  return rc;
}

static void log_after_route(unsigned int index) {
  struct stat st = {0};
  clear_errno();
  int rc = payload_lstat(update_paths[index], &st);
  int err = payload_errno();
  report_text("route."); report_u64(index); report_text(".after.rc=");
  report_i64(rc); report_text(".errno="); report_i64(err); report_char('\n');
  if (rc == 0 && S_ISDIR(st.st_mode)) {
    int empty = directory_is_empty(update_paths[index]);
    report_text("route."); report_u64(index); report_text(".after.empty=");
    report_text(empty == 1 ? "yes\n" : "no\n");
  }
}

int _main(void *thread_data) {
  (void)thread_data;
  report_start("disable_updates_full");

  if (!payload_check_runtime()) {
    report_text("precondition=FAIL\n");
    (void)payload_send_report();
    return 0;
  }

  /* =============================================================
   * CAPA 1: FILESYSTEM (Señuelos en /update)
   * ============================================================= */
  struct stat stats[ROUTE_COUNT] = {{0}};
  enum route_state states[ROUTE_COUNT];
  for (unsigned int i = 0; i < ROUTE_COUNT; ++i) {
    states[i] = inspect_route(i, &stats[i]);
    if (states[i] == ROUTE_ERROR || states[i] == ROUTE_UNSAFE) {
      report_text("layer1.status=ABORT_UNSAFE\n");
      report_text("updates.status=ABORT_LAYER1_FAILED (0/3)\n");
      (void)payload_notify("Updates: 0/3 - fallo en filesystem");
      report_text("payload.completed=yes\n");
      (void)payload_send_report();
      return 0;
    }
  }

  int layer1_failures = 0;
  /* 1. Crear directorios para rutas faltantes */
  for (unsigned int i = 0; i < ROUTE_COUNT; ++i) {
    if (states[i] == ROUTE_MISSING) {
      report_text("route."); report_u64(i); report_text(".action=mkdir_missing\n");
      if (mkdir_0777(update_paths[i]) != 0) layer1_failures = 1;
    }
  }

  /* 2. Para ficheros reales existentes: unlink y luego mkdir */
  if (!layer1_failures) {
    for (unsigned int i = 0; i < ROUTE_COUNT; ++i) {
      if (states[i] == ROUTE_EMPTY_DIR) {
        report_text("route."); report_u64(i); report_text(".action=empty_dir_preserved\n");
        continue;
      }
      if (states[i] != ROUTE_FILE) continue;
      report_text("route."); report_u64(i); report_text(".action=unlink_and_mkdir\n");
      clear_errno();
      int unlink_rc = payload_unlink(update_paths[i]);
      int unlink_err = payload_errno();
      report_text("route."); report_u64(i); report_text(".unlink.rc=");
      report_i64(unlink_rc); report_text(".errno="); report_i64(unlink_err); report_char('\n');
      if (unlink_rc == 0) {
        if (mkdir_0777(update_paths[i]) != 0) layer1_failures = 1;
      } else {
        layer1_failures = 1;
      }
    }
  }

  for (unsigned int i = 0; i < ROUTE_COUNT; ++i) log_after_route(i);

  if (layer1_failures) {
    report_text("layer1.status=FAIL\n");
    report_text("updates.status=ABORT_LAYER1_FAILED (0/3)\n");
    (void)payload_notify("Updates: 0/3 - fallo en filesystem");
    report_text("payload.completed=yes\n");
    (void)payload_send_report();
    return 0;
  }
  report_text("layer1.status=PASS\n");

  /* =============================================================
   * CARGA DE libSceRegMgr Y DLSYM
   * ============================================================= */
  int regmgr_handle = 0;
  clear_errno();
  int load_rc = payload_sys_load_module("libSceRegMgr.sprx", 0, &regmgr_handle, 0);
  if (regmgr_handle <= 0 && payload_sce_load_module != 0) {
    clear_errno();
    regmgr_handle = payload_sce_load_module("libSceRegMgr.sprx", 0, 0, 0, 0, 0);
  }

  sceRegMgrGetStr_t p_GetStr = 0;
  sceRegMgrGetInt_t p_GetInt = 0;
  sceRegMgrSetStr_t p_SetStr = 0;
  sceRegMgrSetInt_t p_SetInt = 0;

  if (regmgr_handle > 0) {
    getFunctionAddressByName(regmgr_handle, "sceRegMgrGetStr", &p_GetStr);
    getFunctionAddressByName(regmgr_handle, "sceRegMgrGetInt", &p_GetInt);
    getFunctionAddressByName(regmgr_handle, "sceRegMgrSetStr", &p_SetStr);
    getFunctionAddressByName(regmgr_handle, "sceRegMgrSetInt", &p_SetInt);
  }

  /* Degradación elegante si no está disponible el registro */
  if (regmgr_handle <= 0 || !p_GetStr || !p_GetInt || !p_SetStr || !p_SetInt) {
    report_text("regmgr.status=NOT_AVAILABLE\n");
    report_text("updates.status=PARTIAL_1_OF_3_REGISTRY_UNAVAILABLE\n");
    (void)payload_notify("Updates: 1/3 - registro no disponible");
    report_text("payload.completed=yes\n");
    (void)payload_send_report();
    return 0;
  }
  report_text("regmgr.status=AVAILABLE\n");

  /* =============================================================
   * CAPA 2: REGISTRO auto_download = 0 (OFF)
   * ============================================================= */
  int32_t autodl_before = -1;
  clear_errno();
  int ad_get_rc = p_GetInt(KEY_SYSTEM_UPDATE_AUTODL, &autodl_before);
  report_text("layer2.autodl.before.val="); report_i64(autodl_before); report_char('\n');

  clear_errno();
  int ad_set_rc = p_SetInt(KEY_SYSTEM_UPDATE_AUTODL, 0);
  int ad_set_err = payload_errno();
  report_text("layer2.autodl.set.rc="); report_i64(ad_set_rc); report_char('\n');
  report_text("layer2.autodl.set.errno="); report_i64(ad_set_err); report_char('\n');

  int32_t autodl_after = -1;
  clear_errno();
  int ad_ver_rc = p_GetInt(KEY_SYSTEM_UPDATE_AUTODL, &autodl_after);
  report_text("layer2.autodl.after.val="); report_i64(autodl_after); report_char('\n');

  int autodl_ok = (ad_set_rc == 0 && ad_ver_rc == 0 && autodl_after == 0);
  if (!autodl_ok) {
    report_text("layer2.status=FAIL\n");
    /* Reversión de Capa 2: restaurar valor previo si se leyó correctamente */
    if (ad_get_rc == 0 && autodl_before >= 0) {
      clear_errno();
      int rb_rc = p_SetInt(KEY_SYSTEM_UPDATE_AUTODL, autodl_before);
      report_text("layer2.rollback.rc="); report_i64(rb_rc); report_char('\n');
    }
    report_text("updates.status=PARTIAL_1_OF_3_AUTODL_FAILED\n");
    (void)payload_notify("Updates: 1/3 - auto_download fallo");
    report_text("payload.completed=yes\n");
    (void)payload_send_report();
    return 0;
  }
  report_text("layer2.status=PASS\n");

  /* =============================================================
   * CAPA 3: REGISTRO NP_env = "noupdt" (al final)
   * ============================================================= */
  uint8_t noupdt_buf[17];
  mem_zero(noupdt_buf, 17);
  noupdt_buf[0] = 'n';
  noupdt_buf[1] = 'o';
  noupdt_buf[2] = 'u';
  noupdt_buf[3] = 'p';
  noupdt_buf[4] = 'd';
  noupdt_buf[5] = 't';

  uint8_t np_before[17];
  mem_zero(np_before, 17);
  clear_errno();
  int np_get_rc = p_GetStr(KEY_SYSTEM_NP_ENV, (char *)np_before, 17);
  report_hex17("layer3.npenv.before.hex17", np_before);

  clear_errno();
  int np_fallback_used = 0;
  int np_set_rc = p_SetStr(KEY_SYSTEM_NP_ENV, (const char *)noupdt_buf, 17);
  int np_set_err = payload_errno();
  if (np_set_rc != 0) {
    /* Fallback a strlen+1 = 7 si len=17 es rechazado */
    clear_errno();
    np_set_rc = p_SetStr(KEY_SYSTEM_NP_ENV, (const char *)noupdt_buf, 7);
    np_set_err = payload_errno();
    np_fallback_used = 1;
  }
  report_text("layer3.npenv.set.rc="); report_i64(np_set_rc); report_char('\n');
  report_text("layer3.npenv.set.errno="); report_i64(np_set_err); report_char('\n');
  report_text("layer3.npenv.fallback_used="); report_text(np_fallback_used ? "yes\n" : "no\n");

  uint8_t np_after[17];
  mem_zero(np_after, 17);
  clear_errno();
  int np_ver_rc = p_GetStr(KEY_SYSTEM_NP_ENV, (char *)np_after, 17);
  report_hex17("layer3.npenv.after.hex17", np_after);

  int npenv_ok = (np_set_rc == 0 && np_ver_rc == 0 && mem_equal(np_after, noupdt_buf, 17));
  if (!npenv_ok) {
    report_text("layer3.status=FAIL\n");
    /* Reversión de Capa 3: restaurar valor previo de NP_env */
    if (np_get_rc == 0) {
      clear_errno();
      int rb_rc = p_SetStr(KEY_SYSTEM_NP_ENV, (const char *)np_before, 17);
      if (rb_rc != 0) (void)p_SetStr(KEY_SYSTEM_NP_ENV, (const char *)np_before, 3);
      report_text("layer3.rollback.performed=yes\n");
    }
    report_text("updates.status=PARTIAL_2_OF_3_NPENV_FAILED\n");
    (void)payload_notify("Updates: 2/3 - NP_env fallo");
    report_text("payload.completed=yes\n");
    (void)payload_send_report();
    return 0;
  }
  report_text("layer3.status=PASS\n");

  /* =============================================================
   * RESUMEN FINAL: 3/3 CAPAS APLICADAS EXITOSAMENTE
   * ============================================================= */
  report_text("updates.status=ALL_LAYERS_APPLIED (3/3)\n");
  (void)payload_notify("Updates: 3/3 aplicado");
  report_text("payload.completed=yes\n");
  (void)payload_send_report();
  return 0;
}
