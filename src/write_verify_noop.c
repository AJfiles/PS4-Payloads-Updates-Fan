/*
 * write_verify_noop.c — Test de escritura y verificación sin cambio de estado.
 * Valida SetStr y SetInt en el registro de sistema de la PS4 (Fase 2b/2c)
 * reescribiendo los valores exactos ya activos:
 *   - 0x02800500 SCE_REGMGR_ENT_KEY_SYSTEM_UPDATE_auto_download = 1 (Int32)
 *   - 0x19800000 SCE_REGMGR_ENT_KEY_NP_env = "np" (String, 17 bytes con relleno cero)
 *
 * REGLAS DE SEGURIDAD Y PIC (Regla 12.3):
 * - NO toca el filesystem (sin open, unlink, mkdir, rmdir).
 * - Sin tablas de punteros absolutos; variables escalares y arrays contiguos.
 * - Sin kexec, jailbreak ni syscall 11.
 * - Destino TCP del reporte incrustado en tiempo de compilación (-DREPORT_HOST).
 */
#include "payload_runtime.h"

extern int getFunctionAddressByName(int module_id, const char *name, void *destination);

#define KEY_SYSTEM_UPDATE_AUTODL  0x02800500U
#define KEY_SYSTEM_NP_ENV         0x19800000U

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

int _main(void *thread_data) {
  (void)thread_data;
  report_start("write_verify_noop");

  if (!payload_check_runtime()) {
    report_text("precondition=FAIL\n");
    (void)payload_send_report();
    return 0;
  }

  int regmgr_handle = 0;
  clear_errno();
  int load_rc = payload_sys_load_module("libSceRegMgr.sprx", 0, &regmgr_handle, 0);
  int load_err = payload_errno();
  report_text("regmgr.load_module.rc="); report_i64(load_rc); report_char('\n');
  report_text("regmgr.load_module.errno="); report_i64(load_err); report_char('\n');
  report_text("regmgr.handle_initial="); report_i64(regmgr_handle); report_char('\n');

  if (regmgr_handle <= 0 && payload_sce_load_module != 0) {
    clear_errno();
    regmgr_handle = payload_sce_load_module("libSceRegMgr.sprx", 0, 0, 0, 0, 0);
    int start_err = payload_errno();
    report_text("regmgr.start_module.handle="); report_i64(regmgr_handle); report_char('\n');
    report_text("regmgr.start_module.errno="); report_i64(start_err); report_char('\n');
  }

  if (regmgr_handle <= 0) {
    report_text("regmgr.status=MODULE_LOAD_FAILED\n");
    (void)payload_notify("Write Verify No-Op: Fallo carga libSceRegMgr");
    report_text("payload.completed=yes\n");
    (void)payload_send_report();
    return 0;
  }
  report_text("regmgr.status=MODULE_LOADED\n");

  sceRegMgrGetStr_t p_GetStr = 0;
  sceRegMgrGetInt_t p_GetInt = 0;
  sceRegMgrSetStr_t p_SetStr = 0;
  sceRegMgrSetInt_t p_SetInt = 0;

  clear_errno();
  int d_getstr = getFunctionAddressByName(regmgr_handle, "sceRegMgrGetStr", &p_GetStr);
  report_text("regmgr.dlsym.GetStr.rc="); report_i64(d_getstr); report_char('\n');

  clear_errno();
  int d_getint = getFunctionAddressByName(regmgr_handle, "sceRegMgrGetInt", &p_GetInt);
  report_text("regmgr.dlsym.GetInt.rc="); report_i64(d_getint); report_char('\n');

  clear_errno();
  int d_setstr = getFunctionAddressByName(regmgr_handle, "sceRegMgrSetStr", &p_SetStr);
  report_text("regmgr.dlsym.SetStr.rc="); report_i64(d_setstr); report_char('\n');

  clear_errno();
  int d_setint = getFunctionAddressByName(regmgr_handle, "sceRegMgrSetInt", &p_SetInt);
  report_text("regmgr.dlsym.SetInt.rc="); report_i64(d_setint); report_char('\n');

  if (!p_GetStr || !p_GetInt || !p_SetStr || !p_SetInt) {
    report_text("regmgr.status=SYMBOLS_RESOLVE_FAILED\n");
    (void)payload_notify("Write Verify No-Op: Simbolos no encontrados");
    report_text("payload.completed=yes\n");
    (void)payload_send_report();
    return 0;
  }

  /* -------------------------------------------------------------
   * Capa 2: Test de escritura y re-lectura para auto_download (1 -> 1)
   * ------------------------------------------------------------- */
  int32_t autodl_before = -1;
  clear_errno();
  int ad_get_rc = p_GetInt(KEY_SYSTEM_UPDATE_AUTODL, &autodl_before);
  int ad_get_err = payload_errno();
  report_text("autodl.before.rc="); report_i64(ad_get_rc); report_char('\n');
  report_text("autodl.before.errno="); report_i64(ad_get_err); report_char('\n');
  report_text("autodl.before.val="); report_i64(autodl_before); report_char('\n');

  clear_errno();
  int ad_set_rc = p_SetInt(KEY_SYSTEM_UPDATE_AUTODL, 1);
  int ad_set_err = payload_errno();
  report_text("autodl.set.val=1\n");
  report_text("autodl.set.rc="); report_i64(ad_set_rc); report_char('\n');
  report_text("autodl.set.errno="); report_i64(ad_set_err); report_char('\n');

  int32_t autodl_after = -1;
  clear_errno();
  int ad_ver_rc = p_GetInt(KEY_SYSTEM_UPDATE_AUTODL, &autodl_after);
  int ad_ver_err = payload_errno();
  report_text("autodl.after.rc="); report_i64(ad_ver_rc); report_char('\n');
  report_text("autodl.after.errno="); report_i64(ad_ver_err); report_char('\n');
  report_text("autodl.after.val="); report_i64(autodl_after); report_char('\n');

  int autodl_ok = (ad_set_rc == 0 && ad_ver_rc == 0 && autodl_after == 1);
  report_text("autodl.verify=");
  report_text(autodl_ok ? "PASS\n" : "FAIL\n");

  /* -------------------------------------------------------------
   * Capa 3: Test de escritura y re-lectura para NP_env ("np" -> "np")
   * Buffer de 17 bytes relleno de ceros con "np" al inicio.
   * ------------------------------------------------------------- */
  uint8_t np_expected[17];
  mem_zero(np_expected, 17);
  np_expected[0] = 'n';
  np_expected[1] = 'p';

  uint8_t np_before[17];
  mem_zero(np_before, 17);
  clear_errno();
  int np_get_rc = p_GetStr(KEY_SYSTEM_NP_ENV, (char *)np_before, 17);
  int np_get_err = payload_errno();
  report_text("npenv.before.rc="); report_i64(np_get_rc); report_char('\n');
  report_text("npenv.before.errno="); report_i64(np_get_err); report_char('\n');
  report_hex17("npenv.before.hex17", np_before);

  clear_errno();
  int np_fallback_used = 0;
  int np_set_rc = p_SetStr(KEY_SYSTEM_NP_ENV, (const char *)np_expected, 17);
  int np_set_err = payload_errno();
  report_text("npenv.set.attempt_len=17\n");
  report_text("npenv.set.attempt_rc="); report_i64(np_set_rc); report_char('\n');
  report_text("npenv.set.attempt_errno="); report_i64(np_set_err); report_char('\n');

  if (np_set_rc != 0) {
    /* Fallback a strlen+1 = 3 si len=17 es rechazado por el kernel/driver */
    clear_errno();
    np_set_rc = p_SetStr(KEY_SYSTEM_NP_ENV, (const char *)np_expected, 3);
    np_set_err = payload_errno();
    np_fallback_used = 1;
    report_text("npenv.set.fallback_len=3\n");
    report_text("npenv.set.fallback_rc="); report_i64(np_set_rc); report_char('\n');
    report_text("npenv.set.fallback_errno="); report_i64(np_set_err); report_char('\n');
  }
  report_text("npenv.set.fallback_used=");
  report_text(np_fallback_used ? "yes\n" : "no\n");

  uint8_t np_after[17];
  mem_zero(np_after, 17);
  clear_errno();
  int np_ver_rc = p_GetStr(KEY_SYSTEM_NP_ENV, (char *)np_after, 17);
  int np_ver_err = payload_errno();
  report_text("npenv.after.rc="); report_i64(np_ver_rc); report_char('\n');
  report_text("npenv.after.errno="); report_i64(np_ver_err); report_char('\n');
  report_hex17("npenv.after.hex17", np_after);

  int npenv_bytes_match = mem_equal(np_after, np_expected, 17);
  report_text("npenv.bytes_match=");
  report_text(npenv_bytes_match ? "yes\n" : "no\n");

  int npenv_ok = (np_set_rc == 0 && np_ver_rc == 0 && npenv_bytes_match);
  report_text("npenv.verify=");
  report_text(npenv_ok ? "PASS\n" : "FAIL\n");

  /* -------------------------------------------------------------
   * Resultado global y notificación
   * ------------------------------------------------------------- */
  if (autodl_ok && npenv_ok) {
    report_text("write_verify.status=PASS_ALL\n");
    (void)payload_notify("Write Verify No-Op: PASS (SetStr + SetInt OK)");
  } else {
    report_text("write_verify.status=FAIL\n");
    (void)payload_notify("Write Verify No-Op: FAIL (Revise log)");
  }

  report_text("payload.completed=yes\n");
  (void)payload_send_report();
  return 0;
}
