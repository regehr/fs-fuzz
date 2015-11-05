struct _rand48_state {
  unsigned short seed[3];
};

int _lrand48(struct _rand48_state *);
void _srand48(struct _rand48_state *, int);
