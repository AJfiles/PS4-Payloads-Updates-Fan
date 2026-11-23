/* Control térmico ICC: GET completo, cambia byte 5, SET y verifica/rollback. */
#include "payload_runtime.h"

#ifndef FAN_TARGET
#error "Compile con -DFAN_TARGET=50..80 (o 79 para el preset)"
#endif
#if FAN_TARGET != 50 && FAN_TARGET != 55 && FAN_TARGET != 60 && \
    FAN_TARGET != 65 && FAN_TARGET != 70 && FAN_TARGET != 75 && \
    FAN_TARGET != 80 && FAN_TARGET != 79
#error "Umbral permitido: 50, 55, 60, 65, 70, 75, 80; 79 solo para el preset"
#endif

#define FAN_PATH "/dev/icc_fan"
#define ICC_SET_TABLE 0xC01C8F07UL
#define ICC_GET_TABLE 0xC01C8F08UL
#define TABLE_SIZE 28
#define IO_BUFFER_SIZE 64

static void clear_errno(void) {
  if (__error) *__error() = 0;
}

static int table_equal_except_threshold(const uint8_t *a, const uint8_t *b) {
  for (size_t i = 0; i < TABLE_SIZE; ++i) {
    if (i != 5 && a[i] != b[i]) return 0;
  }
  return b[5] == FAN_TARGET;
}

static void log_diff(const uint8_t *before, const uint8_t *after) {
  int any = 0;
  for (size_t i = 0; i < TABLE_SIZE; ++i) {
    if (before[i] == after[i]) continue;
    if (!any) report_text("fan.diff=");
    else report_text(",");
    any = 1;
    report_text("0x"); report_hex((uint8_t)i); report_char(':');
    report_hex(before[i]); report_text("->"); report_hex(after[i]);
  }
  if (!any) report_text("fan.diff=none");
  report_char('\n');
}

static int get_table(int fd, uint8_t buffer[IO_BUFFER_SIZE], const char *tag) {
  for (size_t i = 0; i < IO_BUFFER_SIZE; ++i) buffer[i] = 0;
  clear_errno();
  int rc = payload_ioctl(fd, ICC_GET_TABLE, buffer);
  int err = payload_errno();
  report_text(tag); report_text(".rc="); report_i64(rc); report_char('\n');
  report_text(tag); report_text(".errno="); report_i64(err); report_char('\n');
  return rc;
}

static int restore_before(int fd, const uint8_t before[TABLE_SIZE]) {
  uint8_t restore[IO_BUFFER_SIZE] = {0};
  uint8_t check[IO_BUFFER_SIZE] = {0};
  for (size_t i = 0; i < TABLE_SIZE; ++i) restore[i] = before[i];
  clear_errno();
  int set_rc = payload_ioctl(fd, ICC_SET_TABLE, restore);
  int set_err = payload_errno();
  report_text("fan.rollback.set.rc="); report_i64(set_rc); report_char('\n');
  report_text("fan.rollback.set.errno="); report_i64(set_err); report_char('\n');
  int get_rc = get_table(fd, check, "fan.rollback.get");
  report_bytes("fan.rollback.table", check, TABLE_SIZE);
  int exact = get_rc == 0;
  for (size_t i = 0; i < TABLE_SIZE && exact; ++i) exact = check[i] == before[i];
  report_text("fan.rollback.exact="); report_text(exact ? "yes\n" : "NO\n");
  return exact;
}

static void run_fan_change(void) {
  struct stat st = {0};
  clear_errno();
  int stat_rc = payload_lstat(FAN_PATH, &st);
  int stat_err = payload_errno();
  report_text("icc.stat.rc="); report_i64(stat_rc); report_char('\n');
  report_text("icc.stat.errno="); report_i64(stat_err); report_char('\n');
  if (stat_rc != 0 || !S_ISCHR(st.st_mode)) {
    report_text("fan.status=ABORT_NODE_MISSING_OR_NOT_CHAR_DEVICE\n");
    (void)payload_notify("Este GoldHEN no expone el control del ventilador");
    return;
  }
  report_text("icc.mode="); report_u64(st.st_mode); report_char('\n');
  report_text("icc.size="); report_i64(st.st_size); report_char('\n');
  clear_errno();
  int fd = payload_open(FAN_PATH, O_RDWR, 0);
  int open_err = payload_errno();
  report_text("icc.open.fd="); report_i64(fd); report_char('\n');
  report_text("icc.open.errno="); report_i64(open_err); report_char('\n');
  if (fd < 0) {
    report_text("fan.status=ABORT_NODE_NOT_ACCESSIBLE\n");
    (void)payload_notify("Este GoldHEN no expone el control del ventilador");
    return;
  }

  uint8_t before_buffer[IO_BUFFER_SIZE] = {0};
  uint8_t request[IO_BUFFER_SIZE] = {0};
  uint8_t after_buffer[IO_BUFFER_SIZE] = {0};
  if (get_table(fd, before_buffer, "fan.get.before") != 0) {
    report_text("fan.status=ABORT_GET_FAILED_NO_WRITE\n");
    (void)payload_notify("No se pudo leer el ventilador. Sin cambios");
    goto done;
  }
  uint8_t before[TABLE_SIZE];
  for (size_t i = 0; i < TABLE_SIZE; ++i) {
    before[i] = before_buffer[i];
    request[i] = before_buffer[i];
  }
  report_bytes("fan.table.before", before, TABLE_SIZE);
  report_text("fan.requested_threshold="); report_u64(FAN_TARGET); report_char('\n');
  if (FAN_TARGET == 80) report_text("fan.warning=80_exceeds_stock_79\n");

  int set_rc = 0;
  int set_err = 0;
  int did_set = before[5] != FAN_TARGET;
  if (did_set) {
    request[5] = FAN_TARGET;
    clear_errno();
    set_rc = payload_ioctl(fd, ICC_SET_TABLE, request);
    set_err = payload_errno();
    report_text("fan.set.rc="); report_i64(set_rc); report_char('\n');
    report_text("fan.set.errno="); report_i64(set_err); report_char('\n');
  } else {
    report_text("fan.set=SKIPPED_ALREADY_AT_TARGET\n");
  }

  int after_rc = get_table(fd, after_buffer, "fan.get.after");
  if (after_rc != 0) {
    report_text("fan.table.after=UNAVAILABLE\n");
    if (did_set) {
      report_text("fan.alert=readback_failed_attempting_restore\n");
      if (restore_before(fd, before))
        (void)payload_notify("No se pudo leer el ventilador. Sin cambios");
      else
        (void)payload_notify("Error en ventilador. Estado requiere revision");
    } else {
      (void)payload_notify("No se pudo leer el ventilador. Sin cambios");
    }
    report_text("fan.status=ERROR_READBACK_FAILED\n");
    goto done;
  }
  report_bytes("fan.table.after", after_buffer, TABLE_SIZE);
  log_diff(before, after_buffer);
  int exactly_before = 1;
  for (size_t i = 0; i < TABLE_SIZE; ++i)
    if (before[i] != after_buffer[i]) exactly_before = 0;
  if (did_set && exactly_before) {
    report_text("fan.alert=SET_DID_NOT_CHANGE_TABLE\n");
    report_text("fan.status=ERROR_SET_NOT_APPLIED_NO_ROLLBACK_NEEDED\n");
    (void)payload_notify("No se pudo leer el ventilador. Sin cambios");
  } else if (table_equal_except_threshold(before, after_buffer)) {
    report_text(set_rc == 0 ? "fan.status=VERIFIED\n" : "fan.status=VERIFIED_SET_REPORTED_ERROR\n");
    payload_notify_fan(FAN_TARGET, before[5], FAN_TARGET == 79);
  } else {
    report_text("fan.alert=VERIFY_FAILED_OTHER_FIELD_OR_THRESHOLD_CHANGED\n");
    int restored = did_set ? restore_before(fd, before) : 0;
    report_text("fan.status=ERROR_VERIFY_FAILED\n");
    (void)payload_notify(restored
      ? "No se pudo leer el ventilador. Sin cambios"
      : "Error en ventilador. Estado requiere revision");
  }

done:
  clear_errno();
  int close_rc = payload_close(fd);
  int close_err = payload_errno();
  report_text("icc.close.rc="); report_i64(close_rc); report_char('\n');
  report_text("icc.close.errno="); report_i64(close_err); report_char('\n');
}

int _main(void *thread_data) {
  (void)thread_data;
#if FAN_TARGET == 79
  report_start("fan_default_apply_79");
  report_text("fan.mode=apply_79_not_factory_restore\n");
#else
  report_start("fan_threshold");
  report_text("fan.mode=threshold\n");
#endif
  if (payload_check_runtime()) run_fan_change();
  report_text("payload.completed=yes\n");
  (void)payload_send_report();
  return 0;
}
