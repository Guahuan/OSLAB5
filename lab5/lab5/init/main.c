#include "put.h"
#include "sched.h"

int start_kernel() {
  const char *msg =
      "ZJU OS LAB 5\n"
      "Student1:123456 张三 Student2:123456 李四\n";
  puts(msg);

  slub_init();
  task_init();

  // 设置第一次时钟中断
  asm volatile("ecall");

  dead_loop();

  return 0;
}
