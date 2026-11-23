/* Bloqueo de updates por señuelos vacíos en las dos rutas conocidas. */
#include "payload_runtime.h"

#define ROUTE_COUNT 2
#define MODE_DIR 0040000
#define MODE_FILE 0100000
#define MODE_PERMS 07777
#define DIR_BUFFER_SIZE 1024

/* Matriz contigua: evita una tabla de punteros absolutos en el BIN raw PIC. */
static const char update_paths[ROUTE_COUNT][36] = {
  "/update/PS4UPDATE.PUP.net.temp",
  "/update/PS4UPDATE.PUP"
};

enum route_state { ROUTE_ERROR = -1, ROUTE_MISSING = 0, ROUTE_EMPTY_DIR = 1,
                   ROUTE_FILE = 2, ROUTE_UNSAFE = 3 };

static void clear_errno(void) {
  if (__error) *__error() = 0;
}

/* Retorna 1 vacío, 0 con contenido y -1 si no se pudo comprobar. */
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
    report_text("route."); report_u64(index); report_text(".before.size=NA\n");
    return err == 2 ? ROUTE_MISSING : ROUTE_ERROR;
  }

  report_text("route."); report_u64(index); report_text(".before.type=");
  if (S_ISDIR(st->st_mode)) report_text("directory");
  else if (S_ISREG(st->st_mode)) report_text("file");
  else if (S_ISLNK(st->st_mode)) report_text("symlink");
  else report_text("other");
  report_char('\n');
  report_text("route."); report_u64(index); report_text(".before.mode=");
  report_u64(st->st_mode); report_char('\n');
  report_text("route."); report_u64(index); report_text(".before.permissions=");
  report_u64(st->st_mode & MODE_PERMS); report_char('\n');
  report_text("route."); report_u64(index); report_text(".before.size=");
  report_i64(st->st_size); report_char('\n');
  report_text("route."); report_u64(index); report_text(".before.uid=");
  report_u64(st->st_uid); report_char('\n');

  if (S_ISDIR(st->st_mode)) {
    int empty = directory_is_empty(update_paths[index]);
    report_text("route."); report_u64(index); report_text(".before.empty=");
    report_text(empty == 1 ? "yes\n" : empty == 0 ? "no\n" : "UNKNOWN\n");
    report_text("route."); report_u64(index); report_text(".directory_safety=");
    report_text(empty == 1 ? "EXACT_PATH_EMPTY_DIRECTORY\n" :
                empty == 0 ? "DIRECTORY_HAS_CONTENT\n" : "DIRECTORY_EMPTYNESS_UNKNOWN\n");
    return empty == 1 ? ROUTE_EMPTY_DIR : empty == 0 ? ROUTE_UNSAFE : ROUTE_ERROR;
  }
  if (S_ISREG(st->st_mode)) return ROUTE_FILE;
  return ROUTE_UNSAFE;
}

static int mkdir_0777(const char *path) {
  int prior_mask = payload_umask(0);
  if (prior_mask < 0) {
    report_text("mkdir.status=ABORT_UMASK_FAILED\n");
    return -1;
  }
  clear_errno();
  int rc = payload_mkdir(path, 0777);
  int err = payload_errno();
  payload_umask(prior_mask);
  report_text("mkdir.rc="); report_i64(rc); report_text(".errno=");
  report_i64(err); report_char('\n');
  return rc;
}

static int restore_directory_mode(const char *path, unsigned int mode) {
  int prior_mask = payload_umask(0);
  if (prior_mask < 0) return -1;
  clear_errno();
  int rc = payload_mkdir(path, mode);
  payload_umask(prior_mask);
  return rc;
}

static void log_after(unsigned int index) {
  struct stat st = {0};
  clear_errno();
  int rc = payload_lstat(update_paths[index], &st);
  int err = payload_errno();
  report_text("route."); report_u64(index); report_text(".after.rc=");
  report_i64(rc); report_text(".errno="); report_i64(err); report_char('\n');
  if (rc != 0) {
    report_text("route."); report_u64(index); report_text(".after.type=");
    report_text(err == 2 ? "missing\n" : "unknown\n");
    report_text("route."); report_u64(index); report_text(".after.size=NA\n");
  }
  if (rc == 0) {
    report_text("route."); report_u64(index); report_text(".after.type=");
    if (S_ISDIR(st.st_mode)) report_text("directory");
    else if (S_ISREG(st.st_mode)) report_text("file");
    else if (S_ISLNK(st.st_mode)) report_text("symlink");
    else report_text("other");
    report_char('\n');
    report_text("route."); report_u64(index); report_text(".after.mode=");
    report_u64(st.st_mode); report_char('\n');
    report_text("route."); report_u64(index); report_text(".after.permissions=");
    report_u64(st.st_mode & MODE_PERMS); report_char('\n');
    report_text("route."); report_u64(index); report_text(".after.uid=");
    report_u64(st.st_uid); report_char('\n');
    report_text("route."); report_u64(index); report_text(".after.size=");
    report_i64(st.st_size); report_char('\n');
    if (S_ISDIR(st.st_mode)) {
      int empty = directory_is_empty(update_paths[index]);
      report_text("route."); report_u64(index); report_text(".after.empty=");
      report_text(empty == 1 ? "yes\n" : empty == 0 ? "no\n" : "UNKNOWN\n");
    }
  }
}

static void run_disable(void) {
  struct stat stats[ROUTE_COUNT] = {{0}};
  enum route_state states[ROUTE_COUNT];
  int failures = 0;
  for (unsigned int i = 0; i < ROUTE_COUNT; ++i) {
    states[i] = inspect_route(i, &stats[i]);
    if (states[i] == ROUTE_ERROR || states[i] == ROUTE_UNSAFE) failures = 1;
  }
  if (failures) {
    report_text("updates.status=ABORT_PREFLIGHT_UNSAFE; no paths changed\n");
    for (unsigned int i = 0; i < ROUTE_COUNT; ++i) log_after(i);
    payload_notify_updates(0, 0);
    return;
  }

  /* Crear primero rutas ausentes; si falla, no se han borrado PUPs. */
  for (unsigned int i = 0; i < ROUTE_COUNT; ++i) {
    if (states[i] != ROUTE_MISSING) continue;
    report_text("route."); report_u64(i); report_text(".action=mkdir_sentinel\n");
    if (mkdir_0777(update_paths[i]) != 0) failures = 1;
    log_after(i);
  }
  if (failures) {
    report_text("updates.status=PARTIAL_CREATE_FAILURE; regular files untouched\n");
    for (unsigned int i = 0; i < ROUTE_COUNT; ++i) log_after(i);
    payload_notify_updates(0, 0);
    return;
  }

  /* Los ficheros reales se borran solo tras completar el preflight de ambas rutas. */
  for (unsigned int i = 0; i < ROUTE_COUNT; ++i) {
    if (states[i] == ROUTE_EMPTY_DIR) {
      report_text("route."); report_u64(i);
      report_text(".action=empty_directory_preserved\n");
      log_after(i);
      continue;
    }
    if (states[i] != ROUTE_FILE) continue;
    report_text("DESTRUCTIVE_WARNING: se eliminará fichero de ");
    report_i64(stats[i].st_size);
    report_text(" bytes en "); report_text(update_paths[i]);
    report_text("; su PUP/contenido se perderá.\n");
    clear_errno();
    int unlink_rc = payload_unlink(update_paths[i]);
    int unlink_err = payload_errno();
    report_text("route."); report_u64(i); report_text(".unlink.rc=");
    report_i64(unlink_rc); report_text(".errno="); report_i64(unlink_err); report_char('\n');
    if (unlink_rc == 0) {
      report_text("route."); report_u64(i); report_text(".action=mkdir_after_unlink\n");
      if (mkdir_0777(update_paths[i]) != 0) failures = 1;
    } else {
      failures = 1;
    }
    log_after(i);
  }
  report_text(failures ? "updates.status=COMPLETED_WITH_ERRORS\n" : "updates.status=DISABLED\n");
  payload_notify_updates(0, !failures);
}

static void run_enable(void) {
  struct stat stats[ROUTE_COUNT] = {{0}};
  enum route_state states[ROUTE_COUNT];
  for (unsigned int i = 0; i < ROUTE_COUNT; ++i) {
    states[i] = inspect_route(i, &stats[i]);
    if (states[i] == ROUTE_ERROR || states[i] == ROUTE_UNSAFE || states[i] == ROUTE_FILE) {
      report_text("updates.status=ABORT_PREFLIGHT_UNSAFE; no paths changed\n");
      for (unsigned int j = 0; j < ROUTE_COUNT; ++j) log_after(j);
      payload_notify_updates(1, 0);
      return;
    }
  }

  unsigned int removed[ROUTE_COUNT] = {0, 0};
  for (unsigned int i = 0; i < ROUTE_COUNT; ++i) {
    if (states[i] == ROUTE_MISSING) {
      report_text("route."); report_u64(i); report_text(".action=already_missing_noop\n");
      continue;
    }
    /* rmdir es el único borrado: solo afecta a estas rutas y directorios vacíos. */
    clear_errno();
    int rc = payload_rmdir(update_paths[i]);
    int err = payload_errno();
    report_text("route."); report_u64(i); report_text(".rmdir.rc=");
    report_i64(rc); report_text(".errno="); report_i64(err); report_char('\n');
    if (rc != 0) {
      /* Recupera los señuelos ya quitados si el segundo rmdir falla. */
      for (unsigned int j = 0; j < i; ++j) {
        if (removed[j]) {
          report_text("route."); report_u64(j); report_text(".rollback=recreate_sentinel\n");
          (void)restore_directory_mode(update_paths[j], stats[j].st_mode & MODE_PERMS);
        }
      }
      report_text("updates.status=ERROR_RMDIR; prior empty sentinels restored where possible\n");
      for (unsigned int j = 0; j < ROUTE_COUNT; ++j) log_after(j);
      payload_notify_updates(1, 0);
      return;
    }
    removed[i] = 1;
  }
  for (unsigned int i = 0; i < ROUTE_COUNT; ++i) log_after(i);
  report_text("updates.status=ENABLED_FILES_NOT_TOUCHED\n");
  payload_notify_updates(1, 1);
}

int _main(void *thread_data) {
  (void)thread_data;
#ifdef UPDATE_DISABLE
  report_start("disable_updates");
#else
  report_start("enable_updates");
#endif
  if (payload_check_runtime()) {
#ifdef UPDATE_DISABLE
    run_disable();
#else
    run_enable();
#endif
  }
  report_text("payload.completed=yes\n");
  (void)payload_send_report();
  return 0;
}
