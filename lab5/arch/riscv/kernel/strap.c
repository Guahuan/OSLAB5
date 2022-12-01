#include "put.h"
#include "riscv.h"
#include "sched.h"
#include "vm.h"
#include "slub.h"

#define offset (0xffffffe000000000 - 0x80000000)

#define CAUSE_FETCH_PAGE_FAULT 12
#define CAUSE_LOAD_PAGE_FAULT 13
#define CAUSE_STORE_PAGE_FAULT 15

void handler_s(uint64_t cause, uint64_t *stack) {
  if (cause >> 63) {                 // interrupt
    if (((cause << 1) >> 1) == 5) {  // supervisor timer interrupt
      asm volatile("ecall");
      do_timer();
    }
  } else {
    uint64_t stval = read_csr(stval);
  }
  return;
}
