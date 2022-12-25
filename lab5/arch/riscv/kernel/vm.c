#include "vm.h"

#include "buddy.h"
#include "put.h"
#include "slub.h"

extern unsigned long long text_start;
extern unsigned long long rodata_start;
extern unsigned long long data_start;
extern unsigned long long _end;
extern buddy mybuddy;

#define ValidBit(i) ((i)&1)
#define PPN(i) (((i) >> 10) & 0xfffffffffff)
#define offset (0xffffffe000000000 - 0x80000000)

void create_mapping(uint64_t *pgtbl, uint64_t va, uint64_t pa, uint64_t sz, int perm)
{
    /* your code */
    for (uint64_t va_i = va, pa_i = pa; va_i < va + sz; va_i += (uint64_t)0x1000, pa_i += (uint64_t)0x1000) {
        /* 寻找页表地址 */
        // 获得三级vpn
        uint64_t vpn2 = (uint64_t)((va_i >> 30) & (0x1ff));
        uint64_t vpn1 = (uint64_t)((va_i >> 21) & (0x1ff));
        uint64_t vpn0 = (uint64_t)((va_i >> 12) & (0x1ff));
        // 三级页表基地址
        uint64_t *pgtbl2 = pgtbl, *pgtbl1, *pgtbl0;
        // 存放pte地址
        uint64_t *pte;

        /* 处理第二级页表 */
        // 获得pte
        pte = &pgtbl2[vpn2];
        // 如果 V bit 合法
        if (*pte & 0x1) {
            uint64_t ppn = (uint64_t)((*pte >> 10) & 0x7ffffff);
            pgtbl1 = (uint64_t*)(ppn << 12);
        }
        // 如果 V bit 不合法，则说明此页表不存在，需要从物理内存里分配空间
        else {
            // 生成新物理地址
            uint64_t new_page_address = (uint64_t)alloc_pages(1);
            if(new_page_address > (uint64_t)(0xf000000000000000))
                new_page_address = (uint64_t)((uint64_t)(new_page_address) - (uint64_t)(offset));
            // 更新当前pte内容
            // 保留原来pte的前10位，reserved
            uint64_t reserved = (uint64_t)(*pte & 0xffc0000000000000);
            // 使用新物理地址右移12位生成ppn，然后再左移十位放到有效位置
            uint64_t ppn = (uint64_t)((uint64_t)(new_page_address >> 12) << 10);
            // 生成最后十位，除最后1位（V，有效位）为1外，其余为0
            uint64_t protection = (uint64_t)0x1;
            // 更新pte
            *pte = reserved | ppn | protection;
            // 更新页表基地址
            pgtbl1 = (uint64_t*)new_page_address;
        }

        /* 处理第一级页表 */
        // 获得pte
        pte = &pgtbl1[vpn1];
        // 如果 V bit 合法
        if (*pte & 0x1) {
            uint64_t ppn = (uint64_t)((*pte >> 10) & 0x7ffffff);
            pgtbl0 = (uint64_t*)(ppn << 12);
        }
        // 如果 V bit 不合法，则说明此页表不存在，需要从物理内存里分配空间
        else {
            // 生成新物理地址
            uint64_t new_page_address = (uint64_t)alloc_pages(1);
            if(new_page_address > (uint64_t)(0xf000000000000000))
                new_page_address = (uint64_t)((uint64_t)(new_page_address)- (uint64_t)(offset));
            // 更新当前pte内容
            // 保留原来pte的前10位，reserved
            uint64_t reserved = (uint64_t)(*pte & 0xffc0000000000000);
            // 使用新物理地址右移12位生成ppn，然后再左移十位放到有效位置
            uint64_t ppn = (uint64_t)((uint64_t)(new_page_address >> 12) << 10);
            // 生成最后十位，除最后1位（V，有效位）为1外，其余为0
            uint64_t protection = (uint64_t)0x1;
            // 更新pte
            *pte = reserved | ppn | protection;
            // 更新页表基地址
            pgtbl0 = (uint64_t*)new_page_address;
        }

        /* 处理第零级页表 */
        pte = &pgtbl0[vpn0];
        // 保留原来pte的前10位，reserved
        uint64_t reserved = (uint64_t)((*pte) & 0xffc0000000000000);
        // 从pa_i获得ppn，并放到有效位置
        uint64_t ppn = (uint64_t)((uint64_t)((pa_i >> 12)) << 10);
        // 生成最后十位，最后1位为1，其余位为perm
        uint64_t protection = (uint64_t)(perm << 1) | (uint64_t)0x1;
        // 更新pte
        *pte = reserved | ppn | protection;
    }
}

void paging_init()
{
    init_buddy_system();
    uint64_t kernel_pages = ((uint64_t)&_end - (uint64_t)&text_start) / (uint64_t)0x1000;
    alloc_pages(kernel_pages);

    /* your code */
    uint64_t *pgtbl = &_end;

    create_mapping(pgtbl, (uint64_t)0xffffffe000000000, (uint64_t)0x80000000, (uint64_t)1<<24,7);
    create_mapping(pgtbl, (uint64_t)0x80000000, (uint64_t)0x80000000,( uint64_t)1<<24,7);
    create_mapping(pgtbl, (uint64_t)0x10000000, (uint64_t)0x10000000, (uint64_t)1<<20,7);
}
