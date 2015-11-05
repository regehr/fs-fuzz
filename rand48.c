#include "rand48.h"

static const unsigned short _rand48_mult[3] = { 0xe66d, 0xdeec, 0x0005 };
static const unsigned short _rand48_add = 0x000b;

static void _dorand48 (unsigned short *xseed)
{
  unsigned int accu;
  unsigned short temp[2];
  accu = (unsigned int) _rand48_mult[0] * (unsigned int) xseed[0] + (unsigned int) _rand48_add;
  temp[0] = (unsigned short) accu;
  accu >>= sizeof(unsigned short) * 8;
  accu += 
    (unsigned int) _rand48_mult[0] * (unsigned int) xseed[1] + 
    (unsigned int) _rand48_mult[1] * (unsigned int) xseed[0];
  temp[1] = (unsigned short) accu;
  accu >>= sizeof(unsigned short) * 8;
  accu += 
    (unsigned int) _rand48_mult[0] * xseed[2] + 
    (unsigned int) _rand48_mult[1] * xseed[1] + 
    (unsigned int) _rand48_mult[2] * xseed[0];
  xseed[0] = temp[0];
  xseed[1] = temp[1];
  xseed[2] = (unsigned short) accu;
}

int _lrand48(struct _rand48_state *s)
{
  unsigned short *_rand48_seed = &(s->seed[0]);
  _dorand48(_rand48_seed);
  return ((int) _rand48_seed[2] << 15) + ((int) _rand48_seed[1] >> 1);
}

void _srand48(struct _rand48_state *s, int seed)
{
  s->seed[0] = seed;
  s->seed[1] = 0;
  s->seed[2] = 0;
}

#ifdef TEST

#define N 6
#define REPS 100

#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

int main (void)
{
  struct _rand48_state s[N];
  int i;
  for (i=0; i<N; i++) {
    srand48 (&s[i], i);
  }
  for (i=0; i<REPS; i++) {
    int j;
    for (j=0; j<N; j++) {
      printf ("%08x ", lrand48(&s[j]));
    }
    printf ("\n");
  }
  return 0;
}

#endif
