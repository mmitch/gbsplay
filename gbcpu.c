/* $Id: gbcpu.c,v 1.15 2004/03/10 01:48:14 ranmachan Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003 (C) by Tobias Diedrich <ranma@gmx.at>
 * Licensed under GNU GPL.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <string.h>

#include "gbcpu.h"

#if DEBUG == 1
static const char regnames[12] = "BCDEHLFASPPC";
static const char *regnamech16[6] = {
	"BC", "DE", "HL", "FA", "SP", "PC"
};
static const char *conds[4] = {
	"NZ", "Z", "NC", "C"
};
#endif

struct opinfo;

typedef void regparm (*ex_fn)(uint8_t op, const struct opinfo *oi);

struct opinfo {
#if DEBUG == 1
	char *name;
#endif
	ex_fn fn;
};

gbcpu_regs_u gbcpu_regs;
int gbcpu_halted;
int gbcpu_stopped;
int gbcpu_if;

static regparm uint32_t none_get(uint32_t addr)
{
	return 0xff;
}

static regparm void none_put(uint32_t addr, uint8_t val)
{
}

static gbcpu_get_fn getlookup[256] = {
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get,
	&none_get
};

static gbcpu_put_fn putlookup[256] = {
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put,
	&none_put
};

static inline regparm uint32_t mem_get(uint32_t addr)
{
	gbcpu_get_fn fn = getlookup[addr >> 8];
	return fn(addr);
}

static inline regparm void mem_put(uint32_t addr, uint8_t val)
{
	gbcpu_put_fn fn = putlookup[addr >> 8];
	fn(addr, val);
}

static regparm void push(uint16_t val)
{
	uint16_t sp = REGS16_R(gbcpu_regs, SP) - 2;
	REGS16_W(gbcpu_regs, SP, sp);
	mem_put(sp, val & 0xff);
	mem_put(sp+1, val >> 8);
}

static regparm uint32_t pop(void)
{
	uint32_t res;
	uint32_t sp = REGS16_R(gbcpu_regs, SP);

	res  = mem_get(sp);
	res += mem_get(sp+1) << 8;
	REGS16_W(gbcpu_regs, SP, sp + 2);

	return res;
}

static regparm uint32_t get_imm8(void)
{
	uint32_t pc = REGS16_R(gbcpu_regs, PC);
	uint32_t res;
	REGS16_W(gbcpu_regs, PC, pc + 1);
	res = mem_get(pc);
	DPRINTF("%02x", res);
	return res;
}

static regparm uint32_t get_imm16(void)
{
	return get_imm8() + (get_imm8() << 8);
}

static inline void print_reg(int i)
{
	if (i == 6) DPRINTF("[HL]"); /* indirect memory access by [HL] */
	else DPRINTF("%c", regnames[i]);
}

static regparm uint32_t get_reg(int i)
{
	if (i == 6) /* indirect memory access by [HL] */
		return mem_get(REGS16_R(gbcpu_regs, HL));
	return REGS8_R(gbcpu_regs, i);
}

static regparm void put_reg(int i, uint8_t val)
{
	if (i == 6) /* indirect memory access by [HL] */
		mem_put(REGS16_R(gbcpu_regs, HL), val);
	else REGS8_W(gbcpu_regs, i, val);
}

static regparm void op_unknown(uint8_t op, const struct opinfo *oi)
{
	fprintf(stderr, "\n\nUnknown opcode %02x.\n", op);
	gbcpu_stopped = 1;
}

static regparm void op_set(uint8_t op)
{
	int reg = op & 7;
	int bit = (op >> 3) & 7;

	DPRINTF("\tSET %d, ", bit);
	print_reg(reg);
	put_reg(reg, get_reg(reg) | (1 << bit));
}

static regparm void op_res(uint8_t op)
{
	int reg = op & 7;
	int bit = (op >> 3) & 7;

	DPRINTF("\tRES %d, ", bit);
	print_reg(reg);
	put_reg(reg, get_reg(reg) & ~(1 << bit));
}

static regparm void op_bit(uint8_t op)
{
	int reg = op & 7;
	int bit = (op >> 3) & 7;

	DPRINTF("\tBIT %d, ", bit);
	print_reg(reg);
	gbcpu_regs.rn.f &= ~NF;
	gbcpu_regs.rn.f |= HF | ZF;
	gbcpu_regs.rn.f ^= ((get_reg(reg) << 8) >> (bit+1)) & ZF;
}

static regparm void op_rl(uint8_t op, const struct opinfo *oi)
{
	int reg = op & 7;
	uint16_t res;
	uint8_t val;

	DPRINTF(" %s ", oi->name);
	print_reg(reg);
	res  = val = get_reg(reg);
	res  = res << 1;
	res |= (((uint16_t)gbcpu_regs.rn.f & CF) >> 4);
	gbcpu_regs.rn.f = (val >> 7) << 4;
	if (res == 0) gbcpu_regs.rn.f |= ZF;
	put_reg(reg, res);
}

static regparm void op_rla(uint8_t op, const struct opinfo *oi)
{
	uint16_t res;

	DPRINTF(" %s", oi->name);
	res  = gbcpu_regs.rn.a;
	res  = res << 1;
	res |= (((uint16_t)gbcpu_regs.rn.f & CF) >> 4);
	gbcpu_regs.rn.f = (gbcpu_regs.rn.a >> 7) << 4;
	if (res == 0) gbcpu_regs.rn.f |= ZF;
	gbcpu_regs.rn.a = res;
}

static regparm void op_rlc(uint8_t op, const struct opinfo *oi)
{
	int reg = op & 7;
	uint16_t res;
	uint8_t val;

	DPRINTF(" %s ", oi->name);
	print_reg(reg);
	res  = val = get_reg(reg);
	res  = res << 1;
	res |= (val >> 7);
	gbcpu_regs.rn.f = (val >> 7) << 4;
	if (res == 0) gbcpu_regs.rn.f |= ZF;
	put_reg(reg, res);
}

static regparm void op_rlca(uint8_t op, const struct opinfo *oi)
{
	uint16_t res;

	DPRINTF(" %s", oi->name);
	res  = gbcpu_regs.rn.a;
	res  = res << 1;
	res |= (gbcpu_regs.rn.a >> 7);
	gbcpu_regs.rn.f = (gbcpu_regs.rn.a >> 7) << 4;
	if (res == 0) gbcpu_regs.rn.f |= ZF;
	gbcpu_regs.rn.a = res;
}

static regparm void op_sla(uint8_t op, const struct opinfo *oi)
{
	int reg = op & 7;
	uint16_t res;
	uint8_t val;

	DPRINTF(" %s ", oi->name);
	print_reg(reg);
	res  = val = get_reg(reg);
	res  = res << 1;
	gbcpu_regs.rn.f = ((val >> 7) << 4);
	if (res == 0) gbcpu_regs.rn.f |= ZF;
	put_reg(reg, res);
}

static regparm void op_rr(uint8_t op, const struct opinfo *oi)
{
	int reg = op & 7;
	uint16_t res;
	uint8_t val;

	DPRINTF(" %s ", oi->name);
	print_reg(reg);
	res  = val = get_reg(reg);
	res  = res >> 1;
	res |= (((uint16_t)gbcpu_regs.rn.f & CF) << 3);
	gbcpu_regs.rn.f = (val & 1) << 4;
	if (res == 0) gbcpu_regs.rn.f |= ZF;
	put_reg(reg, res);
}

static regparm void op_rra(uint8_t op, const struct opinfo *oi)
{
	uint16_t res;

	DPRINTF(" %s", oi->name);
	res  = gbcpu_regs.rn.a;
	res  = res >> 1;
	res |= (((uint16_t)gbcpu_regs.rn.f & CF) << 3);
	gbcpu_regs.rn.f = (gbcpu_regs.rn.a & 1) << 4;
	if (res == 0) gbcpu_regs.rn.f |= ZF;
	gbcpu_regs.rn.a = res;
}

static regparm void op_rrc(uint8_t op, const struct opinfo *oi)
{
	int reg = op & 7;
	uint16_t res;
	uint8_t val;

	DPRINTF(" %s ", oi->name);
	print_reg(reg);
	res  = val = get_reg(reg);
	res  = res >> 1;
	res |= (val << 7);
	gbcpu_regs.rn.f = (val & 1) << 4;
	if (res == 0) gbcpu_regs.rn.f |= ZF;
	put_reg(reg, res);
}

static regparm void op_rrca(uint8_t op, const struct opinfo *oi)
{
	uint16_t res;

	DPRINTF(" %s", oi->name);
	res  = gbcpu_regs.rn.a;
	res  = res >> 1;
	res |= (gbcpu_regs.rn.a << 7);
	gbcpu_regs.rn.f = (gbcpu_regs.rn.a & 1) << 4;
	if (res == 0) gbcpu_regs.rn.f |= ZF;
	gbcpu_regs.rn.a = res;
}

static regparm void op_sra(uint8_t op, const struct opinfo *oi)
{
	int reg = op & 7;
	uint16_t res;
	uint8_t val;

	DPRINTF(" %s ", oi->name);
	print_reg(reg);
	res  = val = get_reg(reg);
	res  = res >> 1;
	res |= (val & 0x80);
	gbcpu_regs.rn.f = ((val & 1) << 4);
	if (res == 0) gbcpu_regs.rn.f |= ZF;
	put_reg(reg, res);
}

static regparm void op_srl(uint8_t op, const struct opinfo *oi)
{
	int reg = op & 7;
	uint16_t res;
	uint8_t val;

	DPRINTF(" %s ", oi->name);
	print_reg(reg);
	res  = val = get_reg(reg);
	res  = res >> 1;
	gbcpu_regs.rn.f = ((val & 1) << 4);
	if (res == 0) gbcpu_regs.rn.f |= ZF;
	put_reg(reg, res);
}

static regparm void op_swap(uint8_t op, const struct opinfo *oi)
{
	int reg = op & 7;
	uint16_t res;
	uint8_t val;

	DPRINTF(" %s ", oi->name);
	print_reg(reg);
	val = get_reg(reg);
	res = (val >> 4) |
	      (val << 4);
	gbcpu_regs.rn.f = 0;
	if (res == 0) gbcpu_regs.rn.f |= ZF;
	put_reg(reg, res);
}

static const struct opinfo cbops[8] = {
	OPINFO("\tRLC", &op_rlc),		/* opcode cb00-cb07 */
	OPINFO("\tRRC", &op_rrc),		/* opcode cb08-cb0f */
	OPINFO("\tRL", &op_rl),		/* opcode cb10-cb17 */
	OPINFO("\tRR", &op_rr),		/* opcode cb18-cb1f */
	OPINFO("\tSLA", &op_sla),		/* opcode cb20-cb27 */
	OPINFO("\tSRA", &op_sra),		/* opcode cb28-cb2f */
	OPINFO("\tSWAP", &op_swap),		/* opcode cb30-cb37 */
	OPINFO("\tSRL", &op_srl),		/* opcode cb38-cb3f */
};

static regparm void op_cbprefix(uint8_t op, const struct opinfo *oi)
{
	uint16_t pc = REGS16_R(gbcpu_regs, PC);

	REGS16_W(gbcpu_regs, PC, pc + 1);
	op = mem_get(pc);
	switch (op >> 6) {
		case 0: cbops[(op >> 3) & 7].fn(op, &cbops[(op >> 3) & 7]);
			return;
		case 1: op_bit(op); return;
		case 2: op_res(op); return;
		case 3: op_set(op); return;
	}
	fprintf(stderr, "\n\nUnknown CB subopcode %02x.\n", op);
	gbcpu_stopped = 1;
}

static regparm void op_ld(uint8_t op, const struct opinfo *oi)
{
	int src = op & 7;
	int dst = (op >> 3) & 7;

	DPRINTF(" %s  ", oi->name);
	print_reg(dst);
	DPRINTF(", ");
	print_reg(src);
	put_reg(dst, get_reg(src));
}

static regparm void op_ld_imm(uint8_t op, const struct opinfo *oi)
{
	int ofs = get_imm16();

	DPRINTF(" %s  A, [0x%04x]", oi->name, ofs);
	gbcpu_regs.rn.a = mem_get(ofs);
}

static regparm void op_ld_ind16_a(uint8_t op, const struct opinfo *oi)
{
	int ofs = get_imm16();

	DPRINTF(" %s  [0x%04x], A", oi->name, ofs);
	mem_put(ofs, gbcpu_regs.rn.a);
}

static regparm void op_ld_ind16_sp(uint8_t op, const struct opinfo *oi)
{
	int ofs = get_imm16();
	int sp = REGS16_R(gbcpu_regs, SP);

	DPRINTF(" %s  [0x%04x], SP", oi->name, ofs);
	mem_put(ofs, sp & 0xff);
	mem_put(ofs+1, sp >> 8);
}

static regparm void op_ld_hlsp(uint8_t op, const struct opinfo *oi)
{
	int8_t ofs = get_imm8();
	uint16_t old = REGS16_R(gbcpu_regs, SP);
	uint16_t new = old + ofs;

	if (ofs>0) DPRINTF(" %s  HL, SP+0x%02x", oi->name, ofs);
	else DPRINTF(" %s  HL, SP-0x%02x", oi->name, -ofs);
	REGS16_W(gbcpu_regs, HL, new);
	gbcpu_regs.rn.f = 0;
	if (old > new) gbcpu_regs.rn.f |= CF;
	if ((old & 0xfff) > (new & 0xfff)) gbcpu_regs.rn.f |= HF;
}

static regparm void op_ld_sphl(uint8_t op, const struct opinfo *oi)
{
	DPRINTF(" %s  SP, HL", oi->name);
	REGS16_W(gbcpu_regs, SP, REGS16_R(gbcpu_regs, HL));
}

static regparm void op_ld_reg16_imm(uint8_t op, const struct opinfo *oi)
{
	int val = get_imm16();
	int reg = (op >> 4) & 3;

	reg += reg > 2; /* skip over FA */
	DPRINTF(" %s  %s, 0x%04x", oi->name, regnamech16[reg], val);
	REGS16_W(gbcpu_regs, reg, val);
}

static regparm void op_ld_reg16_a(uint8_t op, const struct opinfo *oi)
{
	int reg = (op >> 4) & 3;
	uint16_t r;

	reg -= reg > 2;
	if (op & 8) {
		DPRINTF(" %s  A, [%s]", oi->name, regnamech16[reg]);
		gbcpu_regs.rn.a = mem_get(r = REGS16_R(gbcpu_regs, reg));
	} else {
		DPRINTF(" %s  [%s], A", oi->name, regnamech16[reg]);
		mem_put(r = REGS16_R(gbcpu_regs, reg), gbcpu_regs.rn.a);
	}

	if (reg == 2) {
		r += (((op & 0x10) == 0) << 1)-1;
		REGS16_W(gbcpu_regs, reg, r);
	}
}

static regparm void op_ld_reg8_imm(uint8_t op, const struct opinfo *oi)
{
	int val = get_imm8();
	int reg = (op >> 3) & 7;

	DPRINTF(" %s  ", oi->name);
	print_reg(reg);
	put_reg(reg, val);
	DPRINTF(", 0x%02x", val);
}

static regparm void op_ldh(uint8_t op, const struct opinfo *oi)
{
	int ofs = op & 2 ? 0 : get_imm8();

	if (op & 0x10) {
		DPRINTF(" %s  A, ", oi->name);
		if ((op & 2) == 0) {
			DPRINTF("[%02x]", ofs);
		} else {
			ofs = gbcpu_regs.rn.c;
			DPRINTF("[C]");
		}
		gbcpu_regs.rn.a = mem_get(0xff00 + ofs);
	} else {
		if ((op & 2) == 0) {
			DPRINTF(" %s  [%02x], A", oi->name, ofs);
		} else {
			ofs = gbcpu_regs.rn.c;
			DPRINTF(" %s  [C], A", oi->name);
		}
		mem_put(0xff00 + ofs, gbcpu_regs.rn.a);
	}
}

static regparm void op_inc(uint8_t op, const struct opinfo *oi)
{
	int reg = (op >> 3) & 7;
	uint8_t res;
	uint8_t old;

	DPRINTF(" %s ", oi->name);
	print_reg(reg);
	old = res = get_reg(reg);
	res++;
	put_reg(reg, res);
	gbcpu_regs.rn.f &= ~(NF | ZF | HF);
	if (res == 0) gbcpu_regs.rn.f |= ZF;
	if ((old & 15) > (res & 15)) gbcpu_regs.rn.f |= HF;
}

static regparm void op_inc16(uint8_t op, const struct opinfo *oi)
{
	int reg = (op >> 4) & 3;
	uint16_t res = REGS16_R(gbcpu_regs, reg);

	DPRINTF(" %s %s\t", oi->name, regnamech16[reg]);
	res++;
	REGS16_W(gbcpu_regs, reg, res);
}

static regparm void op_dec(uint8_t op, const struct opinfo *oi)
{
	int reg = (op >> 3) & 7;
	uint8_t res;
	uint8_t old;

	DPRINTF(" %s ", oi->name);
	print_reg(reg);
	old = res = get_reg(reg);
	res--;
	put_reg(reg, res);
	gbcpu_regs.rn.f |= NF;
	gbcpu_regs.rn.f &= ~(ZF | HF);
	if (res == 0) gbcpu_regs.rn.f |= ZF;
	if ((old & 15) > (res & 15)) gbcpu_regs.rn.f |= HF;
}

static regparm void op_dec16(uint8_t op, const struct opinfo *oi)
{
	int reg = (op >> 4) & 3;
	uint16_t res = REGS16_R(gbcpu_regs, reg);

	DPRINTF(" %s %s", oi->name, regnamech16[reg]);
	res--;
	REGS16_W(gbcpu_regs, reg, res);
}

static regparm void op_add_sp_imm(uint8_t op, const struct opinfo *oi)
{
	int8_t imm = get_imm8();
	uint16_t old = REGS16_R(gbcpu_regs, SP);
	uint16_t new = old;

	DPRINTF(" %s SP, %02x", oi->name, imm);
	new += imm;
	REGS16_W(gbcpu_regs, SP, new);
	gbcpu_regs.rn.f = 0;
	if (old > new) gbcpu_regs.rn.f |= CF;
	if ((old & 0xfff) > (new & 0xfff)) gbcpu_regs.rn.f |= HF;
}

static regparm void op_add(uint8_t op, const struct opinfo *oi)
{
	uint8_t old = gbcpu_regs.rn.a;
	uint8_t new;

	DPRINTF(" %s A, ", oi->name);
	print_reg(op & 7);
	gbcpu_regs.rn.a += get_reg(op & 7);
	new = gbcpu_regs.rn.a;
	gbcpu_regs.rn.f = 0;
	if (old > new) gbcpu_regs.rn.f |= CF;
	if ((old & 15) > (new & 15)) gbcpu_regs.rn.f |= HF;
	if (new == 0) gbcpu_regs.rn.f |= ZF;
}

static regparm void op_add_imm(uint8_t op, const struct opinfo *oi)
{
	uint8_t imm = get_imm8();
	uint8_t old = gbcpu_regs.rn.a;
	uint8_t new = old;

	DPRINTF(" %s A, $0x%02x", oi->name, imm);
	new += imm;
	gbcpu_regs.rn.a = new;
	gbcpu_regs.rn.f = 0;
	if (old > new) gbcpu_regs.rn.f |= CF;
	if ((old & 15) > (new & 15)) gbcpu_regs.rn.f |= HF;
	if (new == 0) gbcpu_regs.rn.f |= ZF;
}

static regparm void op_add_hl(uint8_t op, const struct opinfo *oi)
{
	int reg = (op >> 4) & 3;
	uint16_t old = REGS16_R(gbcpu_regs, HL);
	uint16_t new = old;

	reg += reg > 2;
	DPRINTF(" %s HL, %s", oi->name, regnamech16[reg]);

	new += REGS16_R(gbcpu_regs, reg);
	REGS16_W(gbcpu_regs, HL, new);

	gbcpu_regs.rn.f &= ~(NF | CF | HF);
	if (old > new) gbcpu_regs.rn.f |= CF;
	if ((old & 0xfff) > (new & 0xfff)) gbcpu_regs.rn.f |= HF;
}

static regparm void op_adc(uint8_t op, const struct opinfo *oi)
{
	uint8_t old = gbcpu_regs.rn.a;
	uint8_t new;

	DPRINTF(" %s A, ", oi->name);
	print_reg(op & 7);
	gbcpu_regs.rn.a += get_reg(op & 7);
	gbcpu_regs.rn.a += (gbcpu_regs.rn.f & CF) > 0;
	gbcpu_regs.rn.f &= ~NF;
	new = gbcpu_regs.rn.a;
	if (old > new) gbcpu_regs.rn.f |= CF; else gbcpu_regs.rn.f &= ~CF;
	if ((old & 15) > (new & 15)) gbcpu_regs.rn.f |= HF; else gbcpu_regs.rn.f &= ~HF;
	if (new == 0) gbcpu_regs.rn.f |= ZF; else gbcpu_regs.rn.f &= ~ZF;
}

static regparm void op_adc_imm(uint8_t op, const struct opinfo *oi)
{
	uint8_t imm = get_imm8();
	uint8_t old = gbcpu_regs.rn.a;
	uint8_t new = old;

	DPRINTF(" %s A, $0x%02x", oi->name, imm);
	new += imm;
	new += (gbcpu_regs.rn.f & CF) > 0;
	gbcpu_regs.rn.f &= ~NF;
	gbcpu_regs.rn.a = new;
	if (old > new) gbcpu_regs.rn.f |= CF; else gbcpu_regs.rn.f &= ~CF;
	if ((old & 15) > (new & 15)) gbcpu_regs.rn.f |= HF; else gbcpu_regs.rn.f &= ~HF;
	if (new == 0) gbcpu_regs.rn.f |= ZF; else gbcpu_regs.rn.f &= ~ZF;
}

static regparm void op_cp(uint8_t op, const struct opinfo *oi)
{
	uint8_t old = gbcpu_regs.rn.a;
	uint8_t new = old;

	DPRINTF(" %s A, ", oi->name);
	print_reg(op & 7);
	new -= get_reg(op & 7);
	gbcpu_regs.rn.f = NF;
	if (old < new) gbcpu_regs.rn.f |= CF;
	if ((old & 15) < (new & 15)) gbcpu_regs.rn.f |= HF;
	if (new == 0) gbcpu_regs.rn.f |= ZF;
}

static regparm void op_cp_imm(uint8_t op, const struct opinfo *oi)
{
	uint8_t imm = get_imm8();
	uint8_t old = gbcpu_regs.rn.a;
	uint8_t new = old;

	DPRINTF(" %s A, $0x%02x", oi->name, imm);
	new -= imm;
	gbcpu_regs.rn.f = NF;
	if (old < new) gbcpu_regs.rn.f |= CF;
	if ((old & 15) < (new & 15)) gbcpu_regs.rn.f |= HF;
	if (new == 0) gbcpu_regs.rn.f |= ZF;
}

static regparm void op_sub(uint8_t op, const struct opinfo *oi)
{
	uint8_t old = gbcpu_regs.rn.a;
	uint8_t new;

	DPRINTF(" %s A, ", oi->name);
	print_reg(op & 7);
	gbcpu_regs.rn.a -= get_reg(op & 7);
	new = gbcpu_regs.rn.a;
	gbcpu_regs.rn.f = NF;
	if (old < new) gbcpu_regs.rn.f |= CF;
	if ((old & 15) < (new & 15)) gbcpu_regs.rn.f |= HF;
	if (new == 0) gbcpu_regs.rn.f |= ZF;
}

static regparm void op_sub_imm(uint8_t op, const struct opinfo *oi)
{
	uint8_t imm = get_imm8();
	uint8_t old = gbcpu_regs.rn.a;
	uint8_t new = old;

	DPRINTF(" %s A, $0x%02x", oi->name, imm);
	new -= imm;
	gbcpu_regs.rn.a = new;
	gbcpu_regs.rn.f = NF;
	if (old < new) gbcpu_regs.rn.f |= CF;
	if ((old & 15) < (new & 15)) gbcpu_regs.rn.f |= HF;
	if (new == 0) gbcpu_regs.rn.f |= ZF;
}

static regparm void op_sbc(uint8_t op, const struct opinfo *oi)
{
	uint8_t old = gbcpu_regs.rn.a;
	uint8_t new;

	DPRINTF(" %s A, ", oi->name);
	print_reg(op & 7);
	gbcpu_regs.rn.a -= get_reg(op & 7);
	gbcpu_regs.rn.a -= (gbcpu_regs.rn.f & CF) > 0;
	new = gbcpu_regs.rn.a;
	gbcpu_regs.rn.f = NF;
	if (old < new) gbcpu_regs.rn.f |= CF;
	if ((old & 15) < (new & 15)) gbcpu_regs.rn.f |= HF;
	if (new == 0) gbcpu_regs.rn.f |= ZF;
}

static regparm void op_sbc_imm(uint8_t op, const struct opinfo *oi)
{
	uint8_t imm = get_imm8();
	uint8_t old = gbcpu_regs.rn.a;
	uint8_t new = old;

	DPRINTF(" %s A, $0x%02x", oi->name, imm);
	new -= imm;
	new -= (gbcpu_regs.rn.f & CF) > 0;
	gbcpu_regs.rn.a = new;
	gbcpu_regs.rn.f = NF;
	if (old < new) gbcpu_regs.rn.f |= CF;
	if ((old & 15) < (new & 15)) gbcpu_regs.rn.f |= HF;
	if (new == 0) gbcpu_regs.rn.f |= ZF;
}

static regparm void op_and(uint8_t op, const struct opinfo *oi)
{
	DPRINTF(" %s A, ", oi->name);
	print_reg(op & 7);
	gbcpu_regs.rn.a &= get_reg(op & 7);
	gbcpu_regs.rn.f = HF;
	if (gbcpu_regs.rn.a == 0) gbcpu_regs.rn.f |= ZF;
}

static regparm void op_and_imm(uint8_t op, const struct opinfo *oi)
{
	uint8_t imm = get_imm8();

	DPRINTF(" %s A, $0x%02x", oi->name, imm);
	gbcpu_regs.rn.a &= imm;
	gbcpu_regs.rn.f = HF;
	if (gbcpu_regs.rn.a == 0) gbcpu_regs.rn.f |= ZF;
}

static regparm void op_or(uint8_t op, const struct opinfo *oi)
{
	DPRINTF(" %s A, ", oi->name);
	print_reg(op & 7);
	gbcpu_regs.rn.a |= get_reg(op & 7);
	gbcpu_regs.rn.f = 0;
	if (gbcpu_regs.rn.a == 0) gbcpu_regs.rn.f |= ZF;
}

static regparm void op_or_imm(uint8_t op, const struct opinfo *oi)
{
	uint8_t imm = get_imm8();

	DPRINTF(" %s A, $0x%02x", oi->name, imm);
	gbcpu_regs.rn.a |= imm;
	gbcpu_regs.rn.f = 0;
	if (gbcpu_regs.rn.a == 0) gbcpu_regs.rn.f |= ZF;
}

static regparm void op_xor(uint8_t op, const struct opinfo *oi)
{
	DPRINTF(" %s A, ", oi->name);
	print_reg(op & 7);
	gbcpu_regs.rn.a ^= get_reg(op & 7);
	gbcpu_regs.rn.f = 0;
	if (gbcpu_regs.rn.a == 0) gbcpu_regs.rn.f |= ZF;
}

static regparm void op_xor_imm(uint8_t op, const struct opinfo *oi)
{
	uint8_t imm = get_imm8();

	DPRINTF(" %s A, $0x%02x", oi->name, imm);
	gbcpu_regs.rn.a ^= imm;
	gbcpu_regs.rn.f = 0;
	if (gbcpu_regs.rn.a == 0) gbcpu_regs.rn.f |= ZF;
}

static regparm void op_push(uint8_t op, const struct opinfo *oi)
{
	int reg = op >> 4 & 3;

	push(REGS16_R(gbcpu_regs, reg));
	DPRINTF(" %s %s\t", oi->name, regnamech16[reg]);
}

static regparm void op_pop(uint8_t op, const struct opinfo *oi)
{
	int reg = op >> 4 & 3;

	REGS16_W(gbcpu_regs, reg, pop());
	DPRINTF(" %s %s\t", oi->name, regnamech16[reg]);
}

static regparm void op_cpl(uint8_t op, const struct opinfo *oi)
{
	DPRINTF(" %s", oi->name);
	gbcpu_regs.rn.a = ~gbcpu_regs.rn.a;
	gbcpu_regs.rn.f |= NF | HF;
}

static regparm void op_ccf(uint8_t op, const struct opinfo *oi)
{
	DPRINTF(" %s", oi->name);
	gbcpu_regs.rn.f ^= CF;
	gbcpu_regs.rn.f &= ~(NF | HF);
}

static regparm void op_scf(uint8_t op, const struct opinfo *oi)
{
	DPRINTF(" %s", oi->name);
	gbcpu_regs.rn.f |= CF;
	gbcpu_regs.rn.f &= ~(NF | HF);
}

static regparm void op_call(uint8_t op, const struct opinfo *oi)
{
	uint16_t ofs = get_imm16();

	DPRINTF(" %s 0x%04x", oi->name, ofs);
	push(REGS16_R(gbcpu_regs, PC));
	REGS16_W(gbcpu_regs, PC, ofs);
}

static regparm void op_call_cond(uint8_t op, const struct opinfo *oi)
{
	uint16_t ofs = get_imm16();
	int cond = (op >> 3) & 3;

	DPRINTF(" %s %s 0x%04x", oi->name, conds[cond], ofs);
	switch (cond) {
		case 0: if ((gbcpu_regs.rn.f & ZF) != 0) return; break;
		case 1: if ((gbcpu_regs.rn.f & ZF) == 0) return; break;
		case 2: if ((gbcpu_regs.rn.f & CF) != 0) return; break;
		case 3: if ((gbcpu_regs.rn.f & CF) == 0) return; break;
	}
	push(REGS16_R(gbcpu_regs, PC));
	REGS16_W(gbcpu_regs, PC, ofs);
}

static regparm void op_ret(uint8_t op, const struct opinfo *oi)
{
	REGS16_W(gbcpu_regs, PC, pop());
	DPRINTF(" %s", oi->name);
}

static regparm void op_reti(uint8_t op, const struct opinfo *oi)
{
	REGS16_W(gbcpu_regs, PC, pop());
	DPRINTF(" %s", oi->name);
}

static regparm void op_ret_cond(uint8_t op, const struct opinfo *oi)
{
	int cond = (op >> 3) & 3;

	DPRINTF(" %s %s", oi->name, conds[cond]);
	switch (cond) {
		case 0: if ((gbcpu_regs.rn.f & ZF) != 0) return; break;
		case 1: if ((gbcpu_regs.rn.f & ZF) == 0) return; break;
		case 2: if ((gbcpu_regs.rn.f & CF) != 0) return; break;
		case 3: if ((gbcpu_regs.rn.f & CF) == 0) return; break;
	}
	REGS16_W(gbcpu_regs, PC, pop());
}

static regparm void op_halt(uint8_t op, const struct opinfo *oi)
{
	gbcpu_halted = 1;
	DPRINTF(" %s", oi->name);
}

static regparm void op_stop(uint8_t op, const struct opinfo *oi)
{
	DPRINTF(" %s", oi->name);
}

static regparm void op_di(uint8_t op, const struct opinfo *oi)
{
	gbcpu_if = 0;
	DPRINTF(" %s", oi->name);
}

static regparm void op_ei(uint8_t op, const struct opinfo *oi)
{
	gbcpu_if = 1;
	DPRINTF(" %s", oi->name);
}

static regparm void op_jr(uint8_t op, const struct opinfo *oi)
{
	int16_t ofs = (int8_t) get_imm8();

	if (ofs < 0) DPRINTF(" %s $-0x%02x", oi->name, -ofs);
	else DPRINTF(" %s $+0x%02x", oi->name, ofs);
	REGS16_W(gbcpu_regs, PC, REGS16_R(gbcpu_regs, PC) + ofs);
}

static regparm void op_jr_cond(uint8_t op, const struct opinfo *oi)
{
	int16_t ofs = (int8_t) get_imm8();
	int cond = (op >> 3) & 3;

	if (ofs < 0) DPRINTF(" %s %s $-0x%02x", oi->name, conds[cond], -ofs);
	else DPRINTF(" %s %s $+0x%02x", oi->name, conds[cond], ofs);
	switch (cond) {
		case 0: if ((gbcpu_regs.rn.f & ZF) != 0) return; break;
		case 1: if ((gbcpu_regs.rn.f & ZF) == 0) return; break;
		case 2: if ((gbcpu_regs.rn.f & CF) != 0) return; break;
		case 3: if ((gbcpu_regs.rn.f & CF) == 0) return; break;
	}
	REGS16_W(gbcpu_regs, PC, REGS16_R(gbcpu_regs, PC) + ofs);
}

static regparm void op_jp(uint8_t op, const struct opinfo *oi)
{
	uint16_t ofs = get_imm16();

	DPRINTF(" %s 0x%04x", oi->name, ofs);
	REGS16_W(gbcpu_regs, PC, ofs);
}

static regparm void op_jp_hl(uint8_t op, const struct opinfo *oi)
{
	DPRINTF(" %s HL", oi->name);
	REGS16_W(gbcpu_regs, PC, REGS16_R(gbcpu_regs, HL));
}

static regparm void op_jp_cond(uint8_t op, const struct opinfo *oi)
{
	uint16_t ofs = get_imm16();
	int cond = (op >> 3) & 3;

	DPRINTF(" %s %s 0x%04x", oi->name, conds[cond], ofs);
	switch (cond) {
		case 0: if ((gbcpu_regs.rn.f & ZF) != 0) return; break;
		case 1: if ((gbcpu_regs.rn.f & ZF) == 0) return; break;
		case 2: if ((gbcpu_regs.rn.f & CF) != 0) return; break;
		case 3: if ((gbcpu_regs.rn.f & CF) == 0) return; break;
	}
	REGS16_W(gbcpu_regs, PC, ofs);
}

static regparm void op_rst(uint8_t op, const struct opinfo *oi)
{
	int16_t ofs = op & 0x38;

	DPRINTF(" %s 0x%02x", oi->name, ofs);
	push(REGS16_R(gbcpu_regs, PC));
	REGS16_W(gbcpu_regs, PC, ofs);
}

static regparm void op_nop(uint8_t op, const struct opinfo *oi)
{
	DPRINTF(" %s", oi->name);
}

static const struct opinfo ops[256] = {
	OPINFO("\tNOP", &op_nop),		/* opcode 00 */
	OPINFO("\tLD", &op_ld_reg16_imm),		/* opcode 01 */
	OPINFO("\tLD", &op_ld_reg16_a),		/* opcode 02 */
	OPINFO("\tINC", &op_inc16),		/* opcode 03 */
	OPINFO("\tINC", &op_inc),		/* opcode 04 */
	OPINFO("\tDEC", &op_dec),		/* opcode 05 */
	OPINFO("\tLD", &op_ld_reg8_imm),		/* opcode 06 */
	OPINFO("\tRLCA", &op_rlca),		/* opcode 07 */
	OPINFO("\tLD", &op_ld_ind16_sp),		/* opcode 08 */
	OPINFO("\tADD", &op_add_hl),		/* opcode 09 */
	OPINFO("\tLD", &op_ld_reg16_a),		/* opcode 0a */
	OPINFO("\tDEC", &op_dec16),		/* opcode 0b */
	OPINFO("\tINC", &op_inc),		/* opcode 0c */
	OPINFO("\tDEC", &op_dec),		/* opcode 0d */
	OPINFO("\tLD", &op_ld_reg8_imm),		/* opcode 0e */
	OPINFO("\tRRCA", &op_rrca),		/* opcode 0f */
	OPINFO("\tSTOP", &op_stop),		/* opcode 10 */
	OPINFO("\tLD", &op_ld_reg16_imm),		/* opcode 11 */
	OPINFO("\tLD", &op_ld_reg16_a),		/* opcode 12 */
	OPINFO("\tINC", &op_inc16),		/* opcode 13 */
	OPINFO("\tINC", &op_inc),		/* opcode 14 */
	OPINFO("\tDEC", &op_dec),		/* opcode 15 */
	OPINFO("\tLD", &op_ld_reg8_imm),		/* opcode 16 */
	OPINFO("\tRLA", &op_rla),		/* opcode 17 */
	OPINFO("\tJR", &op_jr),		/* opcode 18 */
	OPINFO("\tADD", &op_add_hl),		/* opcode 19 */
	OPINFO("\tLD", &op_ld_reg16_a),		/* opcode 1a */
	OPINFO("\tDEC", &op_dec16),		/* opcode 1b */
	OPINFO("\tINC", &op_inc),		/* opcode 1c */
	OPINFO("\tDEC", &op_dec),		/* opcode 1d */
	OPINFO("\tLD", &op_ld_reg8_imm),		/* opcode 1e */
	OPINFO("\tRRA", &op_rra),		/* opcode 1f */
	OPINFO("\tJR", &op_jr_cond),		/* opcode 20 */
	OPINFO("\tLD", &op_ld_reg16_imm),		/* opcode 21 */
	OPINFO("\tLDI", &op_ld_reg16_a),		/* opcode 22 */
	OPINFO("\tINC", &op_inc16),		/* opcode 23 */
	OPINFO("\tINC", &op_inc),		/* opcode 24 */
	OPINFO("\tDEC", &op_dec),		/* opcode 25 */
	OPINFO("\tLD", &op_ld_reg8_imm),		/* opcode 26 */
	OPINFO("\tUNKN", &op_unknown),		/* opcode 27 */
	OPINFO("\tJR", &op_jr_cond),		/* opcode 28 */
	OPINFO("\tADD", &op_add_hl),		/* opcode 29 */
	OPINFO("\tLDI", &op_ld_reg16_a),		/* opcode 2a */
	OPINFO("\tDEC", &op_dec16),		/* opcode 2b */
	OPINFO("\tINC", &op_inc),		/* opcode 2c */
	OPINFO("\tDEC", &op_dec),		/* opcode 2d */
	OPINFO("\tLD", &op_ld_reg8_imm),		/* opcode 2e */
	OPINFO("\tCPL", &op_cpl),		/* opcode 2f */
	OPINFO("\tJR", &op_jr_cond),		/* opcode 30 */
	OPINFO("\tLD", &op_ld_reg16_imm),		/* opcode 31 */
	OPINFO("\tLDD", &op_ld_reg16_a),		/* opcode 32 */
	OPINFO("\tINC", &op_inc16),		/* opcode 33 */
	OPINFO("\tINC", &op_inc),		/* opcode 34 */
	OPINFO("\tDEC", &op_dec),		/* opcode 35 */
	OPINFO("\tLD", &op_ld_reg8_imm),		/* opcode 36 */
	OPINFO("\tSCF", &op_scf),		/* opcode 37 */
	OPINFO("\tJR", &op_jr_cond),		/* opcode 38 */
	OPINFO("\tADD", &op_add_hl),		/* opcode 39 */
	OPINFO("\tLDD", &op_ld_reg16_a),		/* opcode 3a */
	OPINFO("\tDEC", &op_dec16),		/* opcode 3b */
	OPINFO("\tINC", &op_inc),		/* opcode 3c */
	OPINFO("\tDEC", &op_dec),		/* opcode 3d */
	OPINFO("\tLD", &op_ld_reg8_imm),		/* opcode 3e */
	OPINFO("\tCCF", &op_ccf),		/* opcode 3f */
	OPINFO("\tLD", &op_ld),		/* opcode 40 */
	OPINFO("\tLD", &op_ld),		/* opcode 41 */
	OPINFO("\tLD", &op_ld),		/* opcode 42 */
	OPINFO("\tLD", &op_ld),		/* opcode 43 */
	OPINFO("\tLD", &op_ld),		/* opcode 44 */
	OPINFO("\tLD", &op_ld),		/* opcode 45 */
	OPINFO("\tLD", &op_ld),		/* opcode 46 */
	OPINFO("\tLD", &op_ld),		/* opcode 47 */
	OPINFO("\tLD", &op_ld),		/* opcode 48 */
	OPINFO("\tLD", &op_ld),		/* opcode 49 */
	OPINFO("\tLD", &op_ld),		/* opcode 4a */
	OPINFO("\tLD", &op_ld),		/* opcode 4b */
	OPINFO("\tLD", &op_ld),		/* opcode 4c */
	OPINFO("\tLD", &op_ld),		/* opcode 4d */
	OPINFO("\tLD", &op_ld),		/* opcode 4e */
	OPINFO("\tLD", &op_ld),		/* opcode 4f */
	OPINFO("\tLD", &op_ld),		/* opcode 50 */
	OPINFO("\tLD", &op_ld),		/* opcode 51 */
	OPINFO("\tLD", &op_ld),		/* opcode 52 */
	OPINFO("\tLD", &op_ld),		/* opcode 53 */
	OPINFO("\tLD", &op_ld),		/* opcode 54 */
	OPINFO("\tLD", &op_ld),		/* opcode 55 */
	OPINFO("\tLD", &op_ld),		/* opcode 56 */
	OPINFO("\tLD", &op_ld),		/* opcode 57 */
	OPINFO("\tLD", &op_ld),		/* opcode 58 */
	OPINFO("\tLD", &op_ld),		/* opcode 59 */
	OPINFO("\tLD", &op_ld),		/* opcode 5a */
	OPINFO("\tLD", &op_ld),		/* opcode 5b */
	OPINFO("\tLD", &op_ld),		/* opcode 5c */
	OPINFO("\tLD", &op_ld),		/* opcode 5d */
	OPINFO("\tLD", &op_ld),		/* opcode 5e */
	OPINFO("\tLD", &op_ld),		/* opcode 5f */
	OPINFO("\tLD", &op_ld),		/* opcode 60 */
	OPINFO("\tLD", &op_ld),		/* opcode 61 */
	OPINFO("\tLD", &op_ld),		/* opcode 62 */
	OPINFO("\tLD", &op_ld),		/* opcode 63 */
	OPINFO("\tLD", &op_ld),		/* opcode 64 */
	OPINFO("\tLD", &op_ld),		/* opcode 65 */
	OPINFO("\tLD", &op_ld),		/* opcode 66 */
	OPINFO("\tLD", &op_ld),		/* opcode 67 */
	OPINFO("\tLD", &op_ld),		/* opcode 68 */
	OPINFO("\tLD", &op_ld),		/* opcode 69 */
	OPINFO("\tLD", &op_ld),		/* opcode 6a */
	OPINFO("\tLD", &op_ld),		/* opcode 6b */
	OPINFO("\tLD", &op_ld),		/* opcode 6c */
	OPINFO("\tLD", &op_ld),		/* opcode 6d */
	OPINFO("\tLD", &op_ld),		/* opcode 6e */
	OPINFO("\tLD", &op_ld),		/* opcode 6f */
	OPINFO("\tLD", &op_ld),		/* opcode 70 */
	OPINFO("\tLD", &op_ld),		/* opcode 71 */
	OPINFO("\tLD", &op_ld),		/* opcode 72 */
	OPINFO("\tLD", &op_ld),		/* opcode 73 */
	OPINFO("\tLD", &op_ld),		/* opcode 74 */
	OPINFO("\tLD", &op_ld),		/* opcode 75 */
	OPINFO("\tHALT", &op_halt),		/* opcode 76 */
	OPINFO("\tLD", &op_ld),		/* opcode 77 */
	OPINFO("\tLD", &op_ld),		/* opcode 78 */
	OPINFO("\tLD", &op_ld),		/* opcode 79 */
	OPINFO("\tLD", &op_ld),		/* opcode 7a */
	OPINFO("\tLD", &op_ld),		/* opcode 7b */
	OPINFO("\tLD", &op_ld),		/* opcode 7c */
	OPINFO("\tLD", &op_ld),		/* opcode 7d */
	OPINFO("\tLD", &op_ld),		/* opcode 7e */
	OPINFO("\tLD", &op_ld),		/* opcode 7f */
	OPINFO("\tADD", &op_add),		/* opcode 80 */
	OPINFO("\tADD", &op_add),		/* opcode 81 */
	OPINFO("\tADD", &op_add),		/* opcode 82 */
	OPINFO("\tADD", &op_add),		/* opcode 83 */
	OPINFO("\tADD", &op_add),		/* opcode 84 */
	OPINFO("\tADD", &op_add),		/* opcode 85 */
	OPINFO("\tADD", &op_add),		/* opcode 86 */
	OPINFO("\tADD", &op_add),		/* opcode 87 */
	OPINFO("\tADC", &op_adc),		/* opcode 88 */
	OPINFO("\tADC", &op_adc),		/* opcode 89 */
	OPINFO("\tADC", &op_adc),		/* opcode 8a */
	OPINFO("\tADC", &op_adc),		/* opcode 8b */
	OPINFO("\tADC", &op_adc),		/* opcode 8c */
	OPINFO("\tADC", &op_adc),		/* opcode 8d */
	OPINFO("\tADC", &op_adc),		/* opcode 8e */
	OPINFO("\tADC", &op_adc),		/* opcode 8f */
	OPINFO("\tSUB", &op_sub),		/* opcode 90 */
	OPINFO("\tSUB", &op_sub),		/* opcode 91 */
	OPINFO("\tSUB", &op_sub),		/* opcode 92 */
	OPINFO("\tSUB", &op_sub),		/* opcode 93 */
	OPINFO("\tSUB", &op_sub),		/* opcode 94 */
	OPINFO("\tSUB", &op_sub),		/* opcode 95 */
	OPINFO("\tSUB", &op_sub),		/* opcode 96 */
	OPINFO("\tSUB", &op_sub),		/* opcode 97 */
	OPINFO("\tSBC", &op_sbc),		/* opcode 98 */
	OPINFO("\tSBC", &op_sbc),		/* opcode 99 */
	OPINFO("\tSBC", &op_sbc),		/* opcode 9a */
	OPINFO("\tSBC", &op_sbc),		/* opcode 9b */
	OPINFO("\tSBC", &op_sbc),		/* opcode 9c */
	OPINFO("\tSBC", &op_sbc),		/* opcode 9d */
	OPINFO("\tSBC", &op_sbc),		/* opcode 9e */
	OPINFO("\tSBC", &op_sbc),		/* opcode 9f */
	OPINFO("\tAND", &op_and),		/* opcode a0 */
	OPINFO("\tAND", &op_and),		/* opcode a1 */
	OPINFO("\tAND", &op_and),		/* opcode a2 */
	OPINFO("\tAND", &op_and),		/* opcode a3 */
	OPINFO("\tAND", &op_and),		/* opcode a4 */
	OPINFO("\tAND", &op_and),		/* opcode a5 */
	OPINFO("\tAND", &op_and),		/* opcode a6 */
	OPINFO("\tAND", &op_and),		/* opcode a7 */
	OPINFO("\tXOR", &op_xor),		/* opcode a8 */
	OPINFO("\tXOR", &op_xor),		/* opcode a9 */
	OPINFO("\tXOR", &op_xor),		/* opcode aa */
	OPINFO("\tXOR", &op_xor),		/* opcode ab */
	OPINFO("\tXOR", &op_xor),		/* opcode ac */
	OPINFO("\tXOR", &op_xor),		/* opcode ad */
	OPINFO("\tXOR", &op_xor),		/* opcode ae */
	OPINFO("\tXOR", &op_xor),		/* opcode af */
	OPINFO("\tOR", &op_or),		/* opcode b0 */
	OPINFO("\tOR", &op_or),		/* opcode b1 */
	OPINFO("\tOR", &op_or),		/* opcode b2 */
	OPINFO("\tOR", &op_or),		/* opcode b3 */
	OPINFO("\tOR", &op_or),		/* opcode b4 */
	OPINFO("\tOR", &op_or),		/* opcode b5 */
	OPINFO("\tOR", &op_or),		/* opcode b6 */
	OPINFO("\tOR", &op_or),		/* opcode b7 */
	OPINFO("\tCP", &op_cp),		/* opcode b8 */
	OPINFO("\tCP", &op_cp),		/* opcode b9 */
	OPINFO("\tCP", &op_cp),		/* opcode ba */
	OPINFO("\tCP", &op_cp),		/* opcode bb */
	OPINFO("\tCP", &op_cp),		/* opcode bc */
	OPINFO("\tCP", &op_cp),		/* opcode bd */
	OPINFO("\tCP", &op_cp),		/* opcode be */
	OPINFO("\tUNKN", &op_unknown),		/* opcode bf */
	OPINFO("\tRET", &op_ret_cond),		/* opcode c0 */
	OPINFO("\tPOP", &op_pop),		/* opcode c1 */
	OPINFO("\tJP", &op_jp_cond),		/* opcode c2 */
	OPINFO("\tJP", &op_jp),		/* opcode c3 */
	OPINFO("\tCALL", &op_call_cond),		/* opcode c4 */
	OPINFO("\tPUSH", &op_push),		/* opcode c5 */
	OPINFO("\tADD", &op_add_imm),		/* opcode c6 */
	OPINFO("\tRST", &op_rst),		/* opcode c7 */
	OPINFO("\tRET", &op_ret_cond),		/* opcode c8 */
	OPINFO("\tRET", &op_ret),		/* opcode c9 */
	OPINFO("\tJP", &op_jp_cond),		/* opcode ca */
	OPINFO("\tCBPREFIX", &op_cbprefix),		/* opcode cb */
	OPINFO("\tCALL", &op_call_cond),		/* opcode cc */
	OPINFO("\tCALL", &op_call),		/* opcode cd */
	OPINFO("\tADC", &op_adc_imm),		/* opcode ce */
	OPINFO("\tRST", &op_rst),		/* opcode cf */
	OPINFO("\tRET", &op_ret_cond),		/* opcode d0 */
	OPINFO("\tPOP", &op_pop),		/* opcode d1 */
	OPINFO("\tJP", &op_jp_cond),		/* opcode d2 */
	OPINFO("\tUNKN", &op_unknown),		/* opcode d3 */
	OPINFO("\tCALL", &op_call_cond),		/* opcode d4 */
	OPINFO("\tPUSH", &op_push),		/* opcode d5 */
	OPINFO("\tSUB", &op_sub_imm),		/* opcode d6 */
	OPINFO("\tRST", &op_rst),		/* opcode d7 */
	OPINFO("\tRET", &op_ret_cond),		/* opcode d8 */
	OPINFO("\tRETI", &op_reti),		/* opcode d9 */
	OPINFO("\tJP", &op_jp_cond),		/* opcode da */
	OPINFO("\tUNKN", &op_unknown),		/* opcode db */
	OPINFO("\tCALL", &op_call_cond),		/* opcode dc */
	OPINFO("\tUNKN", &op_unknown),		/* opcode dd */
	OPINFO("\tSBC", &op_sbc_imm),		/* opcode de */
	OPINFO("\tRST", &op_rst),		/* opcode df */
	OPINFO("\tLDH", &op_ldh),		/* opcode e0 */
	OPINFO("\tPOP", &op_pop),		/* opcode e1 */
	OPINFO("\tLDH", &op_ldh),		/* opcode e2 */
	OPINFO("\tUNKN", &op_unknown),		/* opcode e3 */
	OPINFO("\tUNKN", &op_unknown),		/* opcode e4 */
	OPINFO("\tPUSH", &op_push),		/* opcode e5 */
	OPINFO("\tAND", &op_and_imm),		/* opcode e6 */
	OPINFO("\tRST", &op_rst),		/* opcode e7 */
	OPINFO("\tADD", &op_add_sp_imm),		/* opcode e8 */
	OPINFO("\tJP", &op_jp_hl),		/* opcode e9 */
	OPINFO("\tLD", &op_ld_ind16_a),		/* opcode ea */
	OPINFO("\tUNKN", &op_unknown),		/* opcode eb */
	OPINFO("\tUNKN", &op_unknown),		/* opcode ec */
	OPINFO("\tUNKN", &op_unknown),		/* opcode ed */
	OPINFO("\tXOR", &op_xor_imm),		/* opcode ee */
	OPINFO("\tRST", &op_rst),		/* opcode ef */
	OPINFO("\tLDH", &op_ldh),		/* opcode f0 */
	OPINFO("\tPOP", &op_pop),		/* opcode f1 */
	OPINFO("\tLDH", &op_ldh),		/* opcode f2 */
	OPINFO("\tDI", &op_di),		/* opcode f3 */
	OPINFO("\tUNKN", &op_unknown),		/* opcode f4 */
	OPINFO("\tPUSH", &op_push),		/* opcode f5 */
	OPINFO("\tOR", &op_or_imm),		/* opcode f6 */
	OPINFO("\tRST", &op_rst),		/* opcode f7 */
	OPINFO("\tLD", &op_ld_hlsp),		/* opcode f8 */
	OPINFO("\tLD", &op_ld_sphl),		/* opcode f9 */
	OPINFO("\tLD", &op_ld_imm),		/* opcode fa */
	OPINFO("\tEI", &op_ei),		/* opcode fb */
	OPINFO("\tUNKN", &op_unknown),		/* opcode fc */
	OPINFO("\tUNKN", &op_unknown),		/* opcode fd */
	OPINFO("\tCP", &op_cp_imm),		/* opcode fe */
	OPINFO("\tRST", &op_rst),		/* opcode ff */
};

#if DEBUG == 1
static gbcpu_regs_u oldregs;

static regparm void dump_regs(void)
{
	int i;

	DPRINTF("; ");
	for (i=0; i<8; i++) {
		DPRINTF("%c=%02x ", regnames[i], REGS8_R(gbcpu_regs, i));
	}
	for (i=5; i<6; i++) {
		DPRINTF("%s=%04x ", regnamech16[i], REGS16_R(gbcpu_regs, i));
	}
	DPRINTF("\n");
	oldregs = gbcpu_regs;
}

static regparm void show_reg_diffs(void)
{
	int i;


	DPRINTF("\t\t; ");
	for (i=0; i<3; i++) {
		if (REGS16_R(gbcpu_regs, i) != REGS16_R(oldregs, i)) {
			DPRINTF("%s=%04x ", regnamech16[i], REGS16_R(gbcpu_regs, i));
			REGS16_W(oldregs, i, REGS16_R(gbcpu_regs, i));
		}
	}
	for (i=6; i<8; i++) {
		if (REGS8_R(gbcpu_regs, i) != REGS8_R(oldregs, i)) {
			if (i == 6) { /* Flags */
				if (gbcpu_regs.rn.f & ZF) DPRINTF("Z");
				else DPRINTF("z");
				if (gbcpu_regs.rn.f & NF) DPRINTF("N");
				else DPRINTF("n");
				if (gbcpu_regs.rn.f & HF) DPRINTF("H");
				else DPRINTF("h");
				if (gbcpu_regs.rn.f & CF) DPRINTF("C");
				else DPRINTF("c");
				DPRINTF(" ");
			} else {
				DPRINTF("%c=%02x ", regnames[i], REGS8_R(gbcpu_regs,i));
			}
			REGS8_W(oldregs, i, REGS8_R(gbcpu_regs, i));
		}
	}
	for (i=4; i<5; i++) {
		if (REGS16_R(gbcpu_regs, i) != REGS16_R(oldregs, i)) {
			DPRINTF("%s=%04x ", regnamech16[i], REGS16_R(gbcpu_regs, i));
			REGS16_W(oldregs, i, REGS16_R(gbcpu_regs, i));
		}
	}
	DPRINTF("\n");
}
#endif

regparm void gbcpu_addmem(uint32_t start, uint32_t end, gbcpu_put_fn putfn, gbcpu_get_fn getfn)
{
	uint32_t i;

	for (i=start; i<=end; i++) {
		putlookup[i] = putfn;
		getlookup[i] = getfn;
	}
}

regparm void gbcpu_init(void)
{
	memset(&gbcpu_regs, 0, sizeof(gbcpu_regs));
	gbcpu_halted = 0;
	gbcpu_stopped = 0;
	gbcpu_if = 0;
	DEB(dump_regs());
}

regparm void gbcpu_intr(int vec)
{
	gbcpu_halted = 0;
	push(REGS16_R(gbcpu_regs, PC));
	REGS16_W(gbcpu_regs, PC, vec);
}

regparm int gbcpu_step(void)
{
	uint8_t op;

	if (!gbcpu_halted) {
		op = mem_get(gbcpu_regs.rn.pc++);
		DPRINTF("%04x: %02x", gbcpu_regs.rn.pc - 1, op);
		ops[op].fn(op, &ops[op]);

		DEB(show_reg_diffs());
		return 1;
	}
	if (gbcpu_halted == 1 && gbcpu_if == 0) {
		fprintf(stderr, "CPU locked up (halt with interrupts disabled).\n");
		gbcpu_stopped = 1;
	}
	if (gbcpu_stopped) return -1;
	return 16;
}
