#include <stdlib.h>

#include "common.h"
#include "gbcpu.h"
#include "mapper.h"

#define MAPPER_ROMBANK_SIZE 0x4000
#define MAPPER_ROMBANK_MASK (MAPPER_ROMBANK_SIZE - 1)
#define MAPPER_EXTRAM_SIZE 0x2000
#define MAPPER_EXTRAM_MASK (MAPPER_EXTRAM_SIZE - 1)

struct mapper;

struct bank {
	const uint8_t *data;
	size_t size;
};

struct mapper {
	const uint8_t *rom;
	size_t rom_size;

	struct bank rom_lower;
	struct bank rom_upper;
	uint8_t extram[MAPPER_EXTRAM_SIZE];
};

static void mapper_map(struct bank *b, const uint8_t *rom, size_t size, long bank)
{
	size_t ofs = bank * MAPPER_ROMBANK_SIZE;
	size_t len = size - ofs;
	if (len <= 0) {
		WARN_ONCE("Bank %ld out of range (0-%ld)!\n", bank, size / MAPPER_ROMBANK_SIZE);
		b->data = NULL;
		b->size = 0;
	}
	b->data = rom + ofs;
	b->size = size - ofs;
}


static uint32_t extram_get(void *priv, uint32_t addr)
{
	struct mapper *m = priv;
	return m->extram[addr & MAPPER_EXTRAM_MASK];
}

static void extram_put(void *priv, uint32_t addr, uint8_t val)
{
	struct mapper *m = priv;
	m->extram[addr & MAPPER_EXTRAM_MASK] = val;
}

static uint32_t bank_get(void *priv, uint32_t addr)
{
	struct bank *b = priv;
	uint32_t maddr = addr & MAPPER_ROMBANK_MASK;
	if (maddr > b->size) {
		return 0xff;
	}
	return b->data[maddr];
}

static void bank_put(void *priv, uint32_t addr, uint8_t val)
{
	if (addr >= 0x2000 && addr <= 0x3fff) {
		struct bank *b = priv;
		struct mapper *m = CONTAINER_OF(b, struct mapper, rom_lower);
		val &= 0x1f;
		mapper_map(&m->rom_upper, m->rom, m->rom_size, val + (val == 0));
	} else {
		WARN_ONCE("rom write of %02x to %04x ignored\n", val, addr);
	}
}

struct mapper *mapper_gbs(struct gbcpu *gbcpu, const uint8_t *rom, size_t size) {
	struct mapper *m = calloc(sizeof(*m), 1);
	m->rom = rom;
	m->rom_size = size;
	mapper_map(&m->rom_lower, rom, size, 0);
	mapper_map(&m->rom_upper, rom, size, 1);
	gbcpu_add_mem(gbcpu, 0x00, 0x3f, bank_put, bank_get, &m->rom_lower);
	gbcpu_add_mem(gbcpu, 0x40, 0x7f, bank_put, bank_get, &m->rom_upper);
	gbcpu_add_mem(gbcpu, 0xa0, 0xbf, extram_put, extram_get, m);
	return m;
};

struct mapper *mapper_gbr(struct gbcpu *gbcpu, const uint8_t *rom, size_t size, uint8_t bank_lower, uint8_t bank_upper) {
	struct mapper *m = calloc(sizeof(*m), 1);
	m->rom = rom;
	m->rom_size = size;
	mapper_map(&m->rom_lower, rom, size, bank_lower);
	mapper_map(&m->rom_upper, rom, size, bank_upper);
	gbcpu_add_mem(gbcpu, 0x00, 0x3f, bank_put, bank_get, &m->rom_lower);
	gbcpu_add_mem(gbcpu, 0x40, 0x7f, bank_put, bank_get, &m->rom_upper);
	gbcpu_add_mem(gbcpu, 0xa0, 0xbf, extram_put, extram_get, m);
	return m;
};

struct mapper *mapper_gb(struct gbcpu *gbcpu, const uint8_t *rom, size_t size, uint8_t type) {
	struct mapper *m = calloc(sizeof(*m), 1);
	m->rom = rom;
	m->rom_size = size;
	mapper_map(&m->rom_lower, rom, size, 0);
	mapper_map(&m->rom_upper, rom, size, 1);
	gbcpu_add_mem(gbcpu, 0x00, 0x3f, bank_put, bank_get, &m->rom_lower);
	gbcpu_add_mem(gbcpu, 0x40, 0x7f, bank_put, bank_get, &m->rom_upper);
	gbcpu_add_mem(gbcpu, 0xa0, 0xbf, extram_put, extram_get, m);
	return m;
};

void mapper_free(struct mapper *m) {
	free(m);
};
