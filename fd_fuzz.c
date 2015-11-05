#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <stdio.h>
#include <errno.h>

// TODO:
// test dup dup2
// test permissions

#define REPS 5000

#define SEEK_MAX 100
#define BUFLEN 100

#define DISK_FILES 10
#define OPEN_FILES 10

static const int MULTIPLE = 0; // multiple offsets into same file? 1 is better
static const int NEGATIVE_PRW = 0; // negative offsets to pread/pwrite? 1 is better
static const int CALL_LINK = 0; // 1 is better

static const int VERBOSE = 0;

#include "rand48.h"

struct _rand48_state rs;

static int choose(int arg) { return _lrand48(&rs) % arg; }

#define OFFSET 0

static int files[OPEN_FILES + OFFSET];
static int modes[OPEN_FILES + OFFSET];

static const char *custom_strerror(int errnum) {
  switch (errnum) {
  case 0:
    return "Success";
  case EINVAL:
    return "Invalid argument";
  case EBADF:
    return "Bad file descriptor";
  case ENOENT:
    return "No such file or directory";
  case EEXIST:
    return "File exists";
  default:
    printf("oops unknown errno %d\n", errnum);
    assert(0);
  }
  return "xxx";
}

static void do_close(int which) {
  if (files[which] == -1) {
    printf("<<< no close >>>\n");
  } else {
    int ret = close(files[which]);
    printf("<<< close(%d) = %d, %s >>>\n", which, ret, custom_strerror(errno));
    files[which] = -1;
  }
}

static char *mode_str(int mode) {
  static char buf[512];
  buf[0] = 0;
  if (mode & O_APPEND) {
    strcat(buf, "O_APPEND|");
    mode &= ~O_APPEND;
  }
  if (mode & O_TRUNC) {
    strcat(buf, "O_TRUNC|");
    mode &= ~O_TRUNC;
  }
  switch (mode) {
  case 0:
    strcat(buf, "O_RDONLY");
    break;
  case 1:
    strcat(buf, "O_WRONLY");
    break;
  case 2:
    strcat(buf, "O_RDWR");
    break;
  default:
    assert(0);
  }
  return buf;
}

static void do_open(int which) {
  int n;
  if (MULTIPLE) {
    n = choose(DISK_FILES);
  } else {
    n = which;
  }

  if (files[which] != -1) {
    close(files[which]);
    errno = 0;
    files[which] = -1;
  }

  char fn[30];
  sprintf(fn, "data-%d", n);
  int mode;
  switch (choose(3)) {
  case 0:
    mode = O_RDONLY;
    break;
  case 1:
    mode = O_WRONLY;
    break;
  case 2:
    mode = O_RDWR;
    break;
  default:
    assert(0);
  }
  if (choose(2))
    mode |= O_APPEND;
  // O_APPEND + O_RDONLY is explicitly undefined in POSIX
  if (choose(2) && ((mode & O_RDWR) || (mode & O_WRONLY)))
    mode |= O_TRUNC;
  int fd;
  if (choose(2)) {
    fd = open(fn, mode);
  } else {
#ifdef TIS_INTERPRETER
    fd = open3(fn, mode | O_CREAT, (mode_t)(S_IRUSR | S_IWUSR));
#else
    fd = open(fn, mode | O_CREAT, S_IRUSR | S_IWUSR);
#endif
  }
  if (fd >= 0) {
    files[which] = fd;
    modes[which] = mode;
  } else {
    modes[which] = 0;
  }
  printf("<<< open(%d, \"%s\", %s) = fd, %s >>>\n", which, fn, mode_str(mode),
         custom_strerror(errno));
}

char *strdata(unsigned char *buf, int len) {
  static char _strdata[1 + BUFLEN];
  int i;
  for (i = 0; i < len; i++) {
    assert(buf[i] == 0 || (buf[i] >= '0' && buf[i] <= '9'));
    if (buf[i] == 0)
      _strdata[i] = '.';
    else
      _strdata[i] = buf[i];
  }
  _strdata[i] = 0;
  return _strdata;
}

static void do_write(int which) {
  if (files[which] == -1)
    return;
  unsigned char buf[BUFLEN];
  int len = choose(BUFLEN);
  int i;
  for (i = 0; i < len; i++)
    buf[i] = '0' + choose(10);
  /*
   * pwrite() + O_APPEND is documented to be buggy in Linux; we'll
   * take this as a sign that the behavior of this combination is
   * irrelevant and we'll just avoid testing it
   */
  if ((0 == (modes[which] & O_APPEND)) && choose(2)) {
    int off = choose(3 * BUFLEN) - (NEGATIVE_PRW ? BUFLEN : 0);
    int ret = pwrite(files[which], buf, len, off);
    printf("<<< pwrite(%d, %d, %d) = %d >>>\n", which, len, off, ret);
  } else {
    int ret = write(files[which], buf, len);
    printf("<<< write(%d, %d) = %d, %s >>>\n", which, len, ret,
           custom_strerror(errno));
  }
}

static void do_read(int which) {
  if (files[which] == -1)
    return;
  unsigned char buf[BUFLEN];
  int len = choose(BUFLEN);
  if (choose(2)) {
    int off = choose(3 * BUFLEN) - (NEGATIVE_PRW ? BUFLEN : 0);
    int ret = pread(files[which], buf, len, off);
    printf("<<< pread(%d, %d, %d) = %d, %s >>>\n", which, len, off, ret,
           (ret == -1) ? "" : strdata(buf, ret));
  } else {
    int ret = read(files[which], buf, len);
    printf("<<< read(%d, %d) = %d, %s, %s >>>\n", which, len, ret,
           custom_strerror(errno), (ret == -1) ? "" : strdata(buf, ret));
  }
}

static void do_stat(int which) {
  if (files[which] == -1)
    return;
  struct stat s;
  int ret = fstat(files[which], &s);
  printf("<<< stat(%d) = %d, %s, %lu >>>\n", which, ret, custom_strerror(errno),
         (ret == 0) ? (unsigned long)s.st_size : -1);
}

static void do_trunc(int which) {
  if (files[which] == -1)
    return;
  off_t off = choose(SEEK_MAX);
  int ret = ftruncate(files[which], off);
  // NB not checking errno since Linux and TIS differ, apparently legitimately
  printf("<<< ftruncate(%d, %ld) = %d >>>\n", which, (long)off, ret);
}

void do_lseek(int which) {
  if (files[which] == -1)
    return;
  int whence;
  switch (choose(4)) {
  case 0:
    whence = SEEK_SET;
    break;
  case 1:
    whence = SEEK_CUR;
    break;
  case 2:
    whence = SEEK_END;
    break;
  case 3:
    whence = _lrand48(&rs);
    break;
  default:
    assert(0);
  }
  off_t off = choose(SEEK_MAX) - (SEEK_MAX / 2);
  int ret = lseek(files[which], off, whence);
  printf("<<< lseek(%d, %ld, %d) = %d, %s >>>\n", which, (long)off, whence, ret,
         custom_strerror(errno));
}

void do_link(int which1) {
  int n1, n2;
  int which2 = OFFSET + choose(OPEN_FILES);
  if (MULTIPLE) {
    n1 = choose(DISK_FILES);
    n2 = choose(DISK_FILES);
  } else {
    n1 = which1;
    n2 = which2;
  }

  char fn1[30];
  sprintf(fn1, "data-%d", n1);
  char fn2[30];
  sprintf(fn2, "data-%d", n2);
  int res = link(fn1, fn2);
  printf("<<< link(%d, \"%s\", %d, \"%s\") = %d, %s >>>\n", which1, fn1, which2,
         fn2, res, custom_strerror(errno));
}

void do_unlink(int which) {
  int n;
  if (MULTIPLE) {
    n = choose(DISK_FILES);
  } else {
    n = which;
  }

  char fn[30];
  sprintf(fn, "data-%d", n);
  int res = unlink(fn);
  printf("<<< unlink(%d, \"%s\") = %d, %s >>>\n", which, fn, res,
         custom_strerror(errno));
}

void print_verbose(int which, const char *when) {
  struct stat s;
  int ret = fstat(files[which], &s);
  printf("<<< for %d %s: pos = %lld, size = %lld >>>\n", which, when,
         (long long)(lseek(files[which], 0, SEEK_CUR)),
         (long long)((ret == 0) ? s.st_size : -1));
}

#include "random_seed.h"

int main(void) {
  printf("random seed = %d\n", RANDOM_SEED);
  _srand48(&rs, RANDOM_SEED);
  int i;
  int useless = 0;
  for (i = 0; i < OPEN_FILES; i++) {
    int x = i + OFFSET;
    files[x] = -1;
    if (VERBOSE)
      print_verbose(x, "init");
  }
  for (i = 0; i < REPS; i++) {
    unsigned op = choose(8);
    int which = OFFSET + choose(OPEN_FILES);
    if (VERBOSE)
      print_verbose(which, "before");
    errno = 0;
    switch (op) {
    case 0:
      switch (choose(2 + CALL_LINK)) {
      case 0:
        do_open(which);
        break;
      case 1:
        do_unlink(which);
        break;
      case 2:
        do_link(which);
      }
      break;
    case 1:
      do_close(which);
      break;
    case 2:
      do_write(which);
      break;
    case 3:
      do_read(which);
      break;
    case 4:
      do_lseek(which);
      break;
    case 5:
      do_stat(which);
      break;
    case 6:
      do_trunc(which);
      break;
    case 7:
      useless++;
      break;
    default:
      assert(0);
    }
    if (VERBOSE)
      print_verbose(which, "after");
  }
  assert(useless > 0);
  printf("<<< Done >>>\n");
  return 0;
}
