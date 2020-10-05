#include <stdio.h>

#define return_err(s) \
    do {int __err = (s); if (__err < 0) return __err;} while (0)
