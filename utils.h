#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdarg.h>

#undef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#undef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))

#undef CLAMP
#define CLAMP(val, min, max) MAX(MIN(val, max), min)

#ifndef HAVE_STRTONUM
long long strtonum(char *, long long, long long, char **);
#endif /* HAVE_STRTONUM */

int xasprintf(char **, char *, ...) __attribute__((__format__ (printf, 2, 3))) __attribute__((__nonnull__ (2)));
void xexec(char *);
void xspawn(char *);
int xvasprintf(char **, char *, va_list) __attribute__((__nonnull__ (2)));

#endif /* __UTILS_H__ */
