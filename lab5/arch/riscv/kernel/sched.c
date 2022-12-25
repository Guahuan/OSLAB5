#include "sched.h"

#include "buddy.h"
#include "put.h"
#include "rand.h"
#include "slub.h"

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

void task_init(void) {
  for(int i=0;i <5; ++i)
  {
    //init

   // printf("now init %d task \n", i);
    struct task_struct* init = (struct task_struct*)(alloc_pages(1));
    uint64_t* pgtbl;
    init->state = TASK_RUNNING;
    init->counter = COUNTER_INIT_COUNTER[i];
    init->priority = PRIORITY_INIT_COUNTER[i];
    init->blocked = 0;
    init->pid = i;
    task[i] = init;
    
    task[i]->thread.sp = (unsigned long long)task[i] + TASK_SIZE;
    task[i]->thread.ra=(long long unsigned)&init_epc;
    //printf("task sp %d 's address is %llx and ra is %llx \n", i, task[i]->thread.sp, task[i]->thread.ra);
    
    pgtbl=(uint64_t)alloc_pages(1);
   // printf("pgt %llx\n", pgtbl);
    
    if(pgtbl > (uint64_t)(0xf000000000000000)) pgtbl = (uint64_t)((uint64_t)(pgtbl) - (uint64_t)(offset));
    
   // printf("pgt %llx\n", pgtbl);
   // puts(" before createmapping\n");

    create_mapping(pgtbl, (uint64_t)0xffffffe000000000, (uint64_t)0x80000000, (uint64_t)1<<24,7);
    create_mapping(pgtbl, (uint64_t)0x80000000, (uint64_t)0x80000000,( uint64_t)1<<24,7);
    create_mapping(pgtbl, (uint64_t)0x10000000, (uint64_t)0x10000000, (uint64_t)1<<20,7);

   // puts(" after creatmapping\n");
    task[i]->mm.satp=pgtbl;
    task[i]->mm.vm=NULL;
    printf("[PID = %d] Process Create Successfully!\n", task[i]->pid);

  }

  task_init_done=1;
  current=task[0];
}

void do_timer(void) {
  if (!task_init_done) return;
  printf("[*PID = %d] Context Calculation: counter = %d,priority = %d\n",
  current->pid, current->counter, current->priority);
  // current process's counter -1, judge whether to schedule or go on.
  current->counter--;
  if (current->counter <= 0) {
    schedule();
  }
}

void schedule(void) {
  unsigned char next;
  int flag; next = 0;
  for (flag = 4; flag > 0; flag--)
    if (task[flag]->state == TASK_RUNNING && task[flag]->counter > 0) {
        next = flag;
        break;
    }
  for (int i = next - 1; i > 0; i--)
    if (task[i]->state == TASK_RUNNING && task[i]->counter > 0)
      if (task[i]->counter < task[next]->counter)
        next = i;
  if (next == 0) {
    printf("All tasks are done, it would assign new random case.\n\n");
    task[0]->counter = 0;
    for (int i = 1; i <= 4; ++i) {
      task[i]->counter = rand();
      task[i]->priority = rand();
      printf("[*PID = %d] Random reset: counter = %d, priority = %d\n", task[i]->pid, task[i]->counter, task[i]->priority);
    }
    schedule();
  }

  if (current->pid != task[next]->pid) {
    printf(
        "[ %d -> %d ] Switch from task %d[%lx] to task %d[%lx], prio: %d, "
        "counter: %d\n",
        current->pid, task[next]->pid, current->pid,
        (unsigned long)current->thread.sp, task[next]->pid,
        (unsigned long)task[next], task[next]->priority, task[next]->counter);
  }
  switch_to(task[next]);
}

void switch_to(struct task_struct *next) {
  if(next->pid!=current->pid){
    struct task_struct* prev=current;
    current=next;
    asm volatile(
      "csrw satp,%[csatp]\n"
      :
      :[csatp] "r"(next->mm.satp)
      :"memory"
    );
    __switch_to(prev,current);
  }

}


void dead_loop(void) {
  while (1) {
    if (current->pid) {
      //puts(" before dead_loop \n");
      mmap(0, 0x1000, 7, 0, 0, 0);
      int *a = (int *)(0x0000);
      *a = 1;
      printf("\033[32m[CHECK] page store OK\033[0m\n\n");

      mmap(0x1000, 0x9000, 7, 0, 0, 0);
      a = (int *)(0x3000);
      int b = (*a);
      printf("\033[32m[CHECK] page load OK\033[0m\n\n");

    //puts(" after dead_loop \n");
      while (1)
        ;
    }
    continue;
  }
}

void *mmap(void *__addr, size_t __len, int __prot, int __flags, int __fd,
           int __offset) {
  struct vm_area_struct *vm;
  vm=(struct vm_area_struct *)kmalloc(sizeof(struct vm_area_struct));
  vm->vm_start=__addr;
  vm->vm_end=__addr+__len;
  vm->vm_flags=__flags;
  // 进程第一次分配匿名内存页
  if(current->mm.vm==NULL){
    INIT_LIST_HEAD(&(vm->vm_list));
    current->mm.vm=vm;
  }
  else{
    list_add(&(vm->vm_list),&(current->mm.vm->vm_list));
  }
  return __addr;
}
