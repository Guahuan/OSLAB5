#include "sched.h"

#include "buddy.h"
#include "put.h"
#include "rand.h"
#include "slub.h"

int x = 1;
#define offset (0xffffffe000000000 - 0x80000000)

struct task_struct *current;
struct task_struct *task[NR_TASKS];
long PRIORITY_INIT_COUNTER[NR_TASKS] = {0, 1, 2, 3, 4};
long COUNTER_INIT_COUNTER[NR_TASKS] = {0, 1, 2, 3, 4};

int task_init_done = 0;
extern int page_id;
extern unsigned long long _end;
extern uint64_t rodata_start;
extern uint64_t data_start;

extern void init_epc(void);
extern void __switch_to(struct task_struct *current, struct task_struct *next);

void task_init(void) {}

void do_timer(void) {}

void schedule(void) {}

void switch_to(struct task_struct *next) {}

void dead_loop(void) {
  while (1) {
    if (current->pid) {
      mmap(0, 0x1000, 7, 0, 0, 0);
      int *a = (int *)(0x0000);
      *a = 1;
      printf("\033[32m[CHECK] page store OK\033[0m\n\n");

      mmap(0x1000, 0x9000, 7, 0, 0, 0);
      a = (int *)(0x3000);
      int b = (*a);
      printf("\033[32m[CHECK] page load OK\033[0m\n\n");

      while (1)
        ;
    }
    continue;
  }
}

void *mmap(void *__addr, size_t __len, int __prot, int __flags, int __fd,
           int __offset) {}