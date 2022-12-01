#include "vm.h"

#include "buddy.h"
#include "put.h"
#include "slub.h"

extern unsigned long long text_start;
extern unsigned long long rodata_start;
extern unsigned long long data_start;
extern unsigned long long _end;

#define ValidBit(i) ((i)&1)
#define PPN(i) (((i) >> 10) & 0xfffffffffff)
#define offset (0xffffffe000000000 - 0x80000000)

void create_mapping(uint64_t *pgtbl, uint64_t va, uint64_t pa, uint64_t sz,
                    int perm) {}

void paging_init() {}