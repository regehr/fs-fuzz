#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#define REPS (5*1000*1000)

#define SEEK_MAX 100
#define BUFLEN 100

#define DISK_FILES 5
#define OPEN_FILES 5

// multiple offsets into same file? 1 is better BUT there does not
// appear to be a way to synchronize multiple file streams into the
// same file (POSIX appears to support it but OS X doesn't -- this is
// probably a bug but it doesn't seem clear they'll be interested in
// fixing it)
static const int MULTIPLE = 0;

// WARNING: in addition to printing more information this flag can
// change the behavior since it makes side-effecting calls; no way to
// avoid this that I know of
static const int VERBOSE = 0;

#include "rand48.h"

struct _rand48_state rs;

static int choose(int arg) { return _lrand48(&rs) % arg; }

static FILE *files[OPEN_FILES];
static int last_was_write[OPEN_FILES];
static int last_was_read[OPEN_FILES];
static int update_mode[OPEN_FILES];

// for supporting ungetc() -- just a single character is guaranteed so
// that all we'll test
static int havec[OPEN_FILES];
static int whichc[OPEN_FILES];

static const char *custom_strerror(int errnum) {

  // FIXME
  return "x";

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

static void do_fclose(int which) {
  if (files[which] == 0)
    return;
  int ret = fclose(files[which]);
  printf("<<< fclose(%d) = %d, %s >>>\n", which, ret, custom_strerror(errno));
  files[which] = 0;
}

static void do_fopen(int which) {
  int n;
  if (MULTIPLE) {
    n = choose(DISK_FILES);
  } else {
    n = which;
  }

  if (files[which]) {
    printf("<<< close for open >>>\n");
    fclose(files[which]);
    errno = 0;
    files[which] = 0;
  }

  last_was_write[which] = 0;
  last_was_read[which] = 0;
  update_mode[which] = 0;
  havec[which] = 0;

  char fn[30];
  sprintf(fn, "data-%d", n);
  char *mode = 0;
  // FIXME -- should be choose(6) but append mode in POSIX is too screwed to test!
  switch (choose(4)) {
  case 0:
    if (choose(2))
      mode = "r";
    else
      mode = "rb";
    break;
  case 1:
    switch (choose(3)) {
    case 0:
      mode = "r+";
      break;
    case 1:
      mode = "rb+";
      break;
    case 2:
      mode = "r+b";
      break;
    }
    update_mode[which] = 1;
    break;
  case 2:
    if (choose(2)) {
      mode = "w";
    } else {
      mode = "wb";
    }
    break;
  case 3:
    switch (choose(3)) {
    case 0:
      mode = "w+";
      break;
    case 1:
      mode = "wb+";
      break;
    case 2:
      mode = "w+b";
      break;
    }
    update_mode[which] = 1;
    break;
  case 4:
    if (choose(2)) {
      mode = "a";
    } else {
      mode = "ab";
    }
    break;
  case 5:
    switch (choose(3)) {
    case 0:
      mode = "a+";
      break;
    case 1:
      mode = "ab+";
      break;
    case 2:
      mode = "a+b";
      break;
    }
    update_mode[which] = 1;
    break;
  }
  FILE *f = fopen(fn, mode);
  if (f != 0) {
    files[which] = f;
  }
  printf("<<< fopen(%d, \"%s\", \"%s\") = %s, %s >>>\n", which, fn, mode,
         f ? "FILE *" : "NULL", custom_strerror(errno));
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

static void do_fwrite(int which) {
  if (files[which] == 0)
    return;
  if (update_mode[which] && last_was_read[which]) {
    // FIXME two other positioning functions we can call here
    int res = fseek(files[which], 0, SEEK_CUR);
    printf("<<< fseek for write of %d = %d >>>\n", which, res);
    last_was_read[which] = 0;
    havec[which] = 0;
  }
  unsigned char buf[BUFLEN];
  int len = choose(BUFLEN);
  int i;
  for (i = 0; i < len; i++)
    buf[i] = '0' + choose(10);
  int ret = fwrite(buf, 1, len, files[which]);
  printf("<<< fwrite(%d, %d) = %d, %s >>>\n", which, len, ret,
         custom_strerror(errno));
  last_was_write[which] = 1;

  // necessary for testing against a buffering implementation
  if (MULTIPLE)
    fflush(files[which]);
  havec[which] = 0;
}

static void do_fread(int which) {
  if (files[which] == 0)
    return;
  if (update_mode[which] && last_was_write[which]) {
    // FIXME can also call file positioning functions here
    fflush(files[which]);
    // NOT checking the return value, fflush is too loosely specified
    last_was_write[which] = 0;
  }
  unsigned char buf[BUFLEN];
  int len = choose(BUFLEN);
  int ret = fread(buf, 1, len, files[which]);
  printf("<<< fread(%d, %d) = %d, %s, %s >>>\n", which, len, ret,
         custom_strerror(errno), (ret == -1) ? "" : strdata(buf, ret));
  last_was_read[which] = 1;
  havec[which] = 0;
}

size_t get_size(int which) {
  if (files[which] == 0)
    return -1;
  FILE *f = files[which];
  long pos = ftell(f);
  fseek(f, 0, SEEK_END);
  long length = ftell(f);
  fseek(f, pos, SEEK_SET);
  havec[which] = 0;
  return length;
}

static void do_size(int which) {
  printf("<<< size(%d) = %ld >>>\n", which, (long)get_size(which));
}

static void do_feof(int which) {
  if (files[which]) {
    printf("<<< feof(%d) = %d >>>\n", which, feof(files[which]));
  }
}

static void do_ferror(int which) {
  if (files[which]) {
    printf("<<< ferror(%d) = %d >>>\n", which, ferror(files[which]));
  }
}

static void do_clearerr(int which) {
  if (files[which]) {
    clearerr(files[which]);
    printf("<<< clearerr(%d) >>>\n", which);
  }
}

static void do_fgetc(int which) {
  if (files[which]) {
    int r = fgetc(files[which]);
    printf("<<< fgetc(%d) = %d >>>\n", which, r);
    if (r >= 0) {
      havec[which] = 1;
      whichc[which] = r;
    } else {
      havec[which] = 0;
    }
  }
  last_was_read[which] = 1;
}

static void do_ungetc(int which) {
  if (files[which]) {
    if (havec[which]) {
      int r = ungetc(whichc[which], files[which]);
      printf("<<< ungetc(%d, %d) = %d >>>\n", which, whichc[which], r);
      havec[which] = 0;
    } else {
      do_fgetc(which);
    }
  }
  last_was_write[which] = 1;
}

void do_fseek(int which) {
  if (files[which] == 0)
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
  int ret = fseek(files[which], off, whence);
  printf("<<< fseek(%d, %ld, %d) = %d, %s >>>\n", which, (long)off, whence, ret,
         custom_strerror(errno));
  havec[which] = 0;
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

void do_fflush(int which) {
  if (files[which]) {
    int res = fflush(files[which]);
    printf("<<< fflush(%d) = %d, %s >>>\n", which, res, custom_strerror(errno));
  }
}

void print_verbose(int which, const char *when) {
  if (files[which]) {
    long pos = ftell(files[which]);
    long size = get_size(which);
    printf("<<< at %s size of %d is %ld pos %ld eof %d error %d >>>\n",
	   when, which, size, pos, feof(files[which]), ferror(files[which]));
  }
}

#ifdef RANDOM_SEED
int main(void) {
#else
int main(int argc, char *argv[]) {
  int RANDOM_SEED;
  if (argc != 2) {
    printf("either #define RANDOM_SEED to be the seed or else provide seed on command line\n");
    exit(-1);
  }
  RANDOM_SEED = atoi(argv[1]);
#endif
  printf("random seed = %d\n", RANDOM_SEED);
  _srand48(&rs, RANDOM_SEED);
  int i;
  int useless = 0;
  for (i = 0; i < OPEN_FILES; i++) {
    files[i] = 0;
    if (VERBOSE)
      print_verbose(i, "init");
  }
  for (i = 0; i < REPS; i++) {
    unsigned op = choose(13);
    int which = choose(OPEN_FILES);
    if (VERBOSE)
      print_verbose(which, "before");
    errno = 0;
    switch (op) {
    case 0:
      switch (choose(2)) {
      case 0:
        do_fopen(which);
        break;
      case 1:
        do_unlink(which);
        break;
      }
      break;
    case 1:
      do_fclose(which);
      break;
    case 2:
      do_fwrite(which);
      break;
    case 3:
      do_fread(which);
      break;
    case 4:
      do_fseek(which);
      break;
    case 5:
      do_size(which);
      break;
    case 6:
      do_feof(which);
      break;
    case 7:
      do_ferror(which);
      break;
    case 8:
      do_clearerr(which);
      break;
    case 9:
      do_fgetc(which);
      break;
    case 10:
      do_ungetc(which);
      break;
    case 11:
      do_ungetc(which);
      break;
    case 12:
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
