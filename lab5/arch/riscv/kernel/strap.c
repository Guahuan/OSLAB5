#include "put.h"
#include "riscv.h"
#include "sched.h"
#include "vm.h"
#include "slub.h"

#define offset (0xffffffe000000000 - 0x80000000)

#define CAUSE_FETCH_PAGE_FAULT 12
#define CAUSE_LOAD_PAGE_FAULT 13
#define CAUSE_STORE_PAGE_FAULT 15
#define Kalloc_Size 4096



void handler_s(uint64_t cause, uint64_t *stack) {
  if (cause >> 63) {                 // interrupt
    if (((cause << 1) >> 1) == 5) {  // supervisor timer interrupt
      asm volatile("ecall");
      do_timer();
    }
  } else {//error
    extern struct task_struct *current;
    uint64_t stval = read_csr(stval);
    if(cause == CAUSE_FETCH_PAGE_FAULT) printf("[FETCH_PAGE_FAULTTT] Address: %lx\n", stval);
    else if(cause == CAUSE_LOAD_PAGE_FAULT) printf("[LOAD_PAGE_FAULT] Address: %lx\n", stval);
    else if(cause == CAUSE_STORE_PAGE_FAULT)  printf("[STORE_PAGE_FAULT] Address: %lx\n", stval);

    struct vm_area_struct *vas = current->mm.vm;
    struct vm_area_struct *cur = NULL;
    struct list_head *hd;
    
    int flag=0;
    unsigned long vm_flags=0;

    // 遍历vm_list
    list_for_each(hd, &(vas->vm_list))
    {
      cur = list_entry(hd, struct vm_area_struct, vm_list);
      if(((uint64_t)(cur->vm_start)<=stval)&&((uint64_t)(cur->vm_end)>=stval))
      {
        flag=1;
        vm_flags = cur->vm_flags;
      }
    }

    
    if(!flag)
      puts("Invalid vm area in page fault!\n");
    else
    {
      //分配空间
      uint64_t addr = (uint64_t)kmalloc(Kalloc_Size);
      if(addr > (uint64_t)(0xf000000000000000)) addr = (uint64_t)((uint64_t)(addr) - (uint64_t)(offset));
      uint64_t pgtbl = (uint64_t*)(((current->mm.satp)&0xfffffffffff)<<12);
      create_mapping(pgtbl,stval,(uint64_t)addr,0x1000,vm_flags);
    }
  }
  return;
}

