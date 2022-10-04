#ifndef OPT_65816_H_ /* Include guard */
#define OPT_65816_H_

/* initial no. of pointers to allocate (lines) */
#define NPTRS 1

/* POSIX suggests a line length of 4096 */
#define MAXLEN_LINE 4096

/* Remove comments */
#define COMMENT ";"

/* BSS constants */
#define BSS_START ".RAMSECTION \".bss\" BANK $7e SLOT 2"
#define BSS_END ".ENDS"

/* stores (accu/x/y/zero) to pseudo-registers */
#define STORE_TO_PSEUDO "st([axyz]).b tcc__([rf][0-9]*h?)$"
/* stores (x/y) to pseudo-registers */
#define STORE_XY_TO_PSEUDO "st([xy]).b tcc__([rf][0-9]*h?)$"
/* stores (accu only) to pseudo-registers */
#define STORE_A_TO_PSEUDO "sta.b tcc__([rf][0-9]*h?)$"

#endif