/* $Id: gbcpu.h,v 1.7 2003/12/13 23:47:01 ranma Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003 (C) by Tobias Diedrich <ranma@gmx.at>
 * Licensed under GNU GPL.
 */

#ifndef _GBCPU_H_
#define _GBCPU_H_

#include <sys/param.h>
#include <inttypes.h>

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

#define REGS16_R(r, i) (*((uint16_t*)&r.ri[i*2]))
#define REGS16_W(r, i, x) (*((uint16_t*)&r.ri[i*2])) = x
#define REGS8_R(r, i) (r.ri[i^1])
#define REGS8_W(r, i, x) (r.ri[i^1]) = x

typedef union {
		uint8_t ri[12];
		struct {
			uint8_t c;
			uint8_t b;
			uint8_t e;
			uint8_t d;
			uint8_t l;
			uint8_t h;
			uint8_t a;
			uint8_t f;
			uint16_t sp;
			uint16_t pc;
		} rn;
} gbcpu_regs_u;

#else

#define REGS16_R(r, i) (*((uint16_t*)&r.ri[i*2]))
#define REGS16_W(r, i, x) (*((uint16_t*)&r.ri[i*2])) = x
#define REGS8_R(r, i) (r.ri[i])
#define REGS8_W(r, i, x) (r.ri[i]) = x

typedef union {
		uint8_t ri[12];
		struct {
			uint8_t b;
			uint8_t c;
			uint8_t d;
			uint8_t e;
			uint8_t h;
			uint8_t l;
			uint8_t f;
			uint8_t a;
			uint16_t sp;
			uint16_t pc;
		} rn;
} gbcpu_regs_u;

#endif

typedef void (*gbcpu_put_fn)(uint32_t addr, uint8_t val);
typedef uint8_t (*gbcpu_get_fn)(uint32_t addr);

extern gbcpu_regs_u gbcpu_regs;
extern int gbcpu_halted;
extern int gbcpu_if;

void gbcpu_addmem(uint32_t start, uint32_t end, gbcpu_put_fn putfn, gbcpu_get_fn getfn);
void gbcpu_init(void);
int gbcpu_step(void);
void gbcpu_intr(int vec);

#endif
