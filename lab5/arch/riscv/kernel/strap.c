#include "put.h"
#include "riscv.h"
#include "sched.h"
#include "vm.h"
#include "slub.h"

#define offset (0xffffffe000000000 - 0x80000000)

#define CAUSE_FETCH_PAGE_FAULT 12
#define CAUSE_LOAD_PAGE_FAULT 13
#define CAUSE_STORE_PAGE_FAULT 15

extern struct task_struct *current;

void handler_s(uint64_t cause, uint64_t *stack) {
  if (cause >> 63) {                 // interrupt
    if (((cause << 1) >> 1) == 5) {  // supervisor timer interrupt
      asm volatile("ecall");
      do_timer();
    }
  }
  else if(cause == CAUSE_FETCH_PAGE_FAULT || cause == CAUSE_LOAD_PAGE_FAULT || cause == CAUSE_STORE_PAGE_FAULT)
  {
    uint64_t stval = read_csr(stval);
    if(cause == CAUSE_FETCH_PAGE_FAULT) printf("[FETCH_PAGE_FAULT] Address: %lx\n", stval);
    else if(cause == CAUSE_LOAD_PAGE_FAULT) printf("[LOAD_PAGE_FAULT] Address: %lx\n", stval);
    else if(cause == CAUSE_STORE_PAGE_FAULT)  printf("[STORE_PAGE_FAULT] Address: %lx\n", stval);
    //遍历vm_list
    struct vm_area_struct *vas = current->mm.vm;
    int flag = 0;

    do{
      if(stval >= vas->vm_start && stval < vas->vm_end)
      {
        flag = 1;
        break;
      }
      vas = (struct vm_area_struct *)vas->vm_list.next;
    }while(vas != current->mm.vm);

    if(!flag)
      puts("Invalid vm area in page fault\n");
    else
    {
      //分配空间
      uint64_t addr = (uint64_t)kmalloc(vas->vm_end - vas->vm_start);
      uint64_t satp = read_csr(satp);
      __asm__ volatile(
      "csrw satp, x0\n"
      ::: "memory"
      );
      create_mapping(current->mm.satp, vas->vm_start, addr - offset, vas->vm_end - vas->vm_start, vas->vm_flags);
      __asm__ volatile(
      "mv t0, %[satp_]\n"
      "csrw satp, t0\n"
      "sfence.vma x0, x0\n"
      :: [satp_] "r" (satp): "memory"
      );
      puts("[CHECK] page fault handle OK\n");
    }
  }
  return;
}
