/* $Id: gbcpu.h,v 1.5 2003/11/28 20:52:05 ranma Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003 (C) by Tobias Diedrich <ranma@gmx.at>
 * Licensed under GNU GPL.
 */

#ifndef _GBCPU_H_
#define _GBCPU_H_

#include <stdio.h>
#include <sys/param.h>

#ifndef BYTE_ORDER

#define BIG_ENDIAN 1
#define LITTLE_ENDIAN 2

#ifdef _BIG_ENDIAN
#define BYTE_ORDER BIG_ENDIAN
#endif

#ifdef _LITTLE_ENDIAN
#define BYTE_ORDER LITTLE_ENDIAN
#endif

#endif /* BYTE_ORDER */

#if !BYTE_ORDER || !BIG_ENDIAN || !LITTLE_ENDIAN
#error endian defines missing
#endif

#define ZF	0x80
#define NF	0x40
#define HF	0x20
#define CF	0x10

#define BC	0
#define DE	1
#define HL	2
#define FA	3
#define SP	4
#define PC	5

#define DEBUG 0

#if DEBUG == 1

#define DPRINTF(...) printf(__VA_ARGS__)
#define DEB(x) x
#define OPINFO(name, fn) {name, fn}

#else

static inline void foo(void)
{
}


#define DPRINTF(...) foo()
#define DEB(x) foo()
#define OPINFO(name, fn) {fn}

#endif

#if BYTE_ORDER == LITTLE_ENDIAN

#define REGS16_R(r, i) (*((unsigned short*)&r.ri[i*2]))
#define REGS16_W(r, i, x) (*((unsigned short*)&r.ri[i*2])) = x
#define REGS8_R(r, i) (r.ri[i^1])
#define REGS8_W(r, i, x) (r.ri[i^1]) = x

struct gbcpu_regs {
	union {
		unsigned char ri[12];
		struct {
			unsigned char c;
			unsigned char b;
			unsigned char e;
			unsigned char d;
			unsigned char l;
			unsigned char h;
			unsigned char a;
			unsigned char f;
			unsigned short sp;
			unsigned short pc;
		} rn;
	};
};

#else

#define REGS16_R(r, i) (*((unsigned short*)&r.ri[i*2]))
#define REGS16_W(r, i, x) (*((unsigned short*)&r.ri[i*2])) = x
#define REGS8_R(r, i) (r.ri[i])
#define REGS8_W(r, i, x) (r.ri[i]) = x

struct gbcpu_regs {
	union {
		unsigned char ri[12];
		struct {
			unsigned char b;
			unsigned char c;
			unsigned char d;
			unsigned char e;
			unsigned char h;
			unsigned char l;
			unsigned char f;
			unsigned char a;
			unsigned short sp;
			unsigned short pc;
		} rn;
	};
};

#endif

typedef void (*gbcpu_put_fn)(unsigned int addr, unsigned char val);
typedef unsigned char (*gbcpu_get_fn)(unsigned int addr);

extern struct gbcpu_regs gbcpu_regs;
extern int gbcpu_halted;
extern int gbcpu_if;

void gbcpu_addmem(int start, int end, gbcpu_put_fn putfn, gbcpu_get_fn getfn);
void gbcpu_init(void);
int gbcpu_step(void);
void gbcpu_intr(int vec);

#endif
