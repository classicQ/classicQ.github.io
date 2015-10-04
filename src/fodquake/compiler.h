#ifndef COMPILER_H
#define COMPILER_H

#ifdef __GNUC__
#define PRINTFWARNING(x, y) __attribute__((format(printf, x, y)))
#endif

#endif

