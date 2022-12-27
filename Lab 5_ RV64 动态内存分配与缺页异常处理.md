# Lab 5: RV64 动态内存分配与缺页异常处理

## **1 **实验目的


在充分理解前序实验中 RISC-V 地址空间映射机制与任务调度管理的前提下，进一步了解与**动态**内存管理相关的重要结构，实现基本的内存管理算法，并了解缺页异常的处理流程。


## **2 实验内容及要求**


- 了解 **Buddy System** 和 **Slub Allocator** 物理内存管理机制的实现原理，并使用 **Buddy System** 和 **Slub Allocator** 管理和分配物理内存。
- 修改进程信息结构，补充虚拟内存相关信息字段** vm_area_struct **数据结构。
- 实现 **kmalloc**，**kfree** 函数，并实现 **mmap 函数**。
- 添加缺页异常处理 **Page Fault Handler**。



请各小组独立完成实验，任何抄袭行为都将使本次实验判为0分。
本实验将进行查重，请提供一定的代码注释。


**请跟随实验步骤完成实验并根据文档中的要求记录实验过程，最后删除文档末尾的附录部分**，并命名为“**学号1_姓名1_学号2_姓名2_lab5.pdf**"，你的代码请打包并命名为“**学号1_姓名1_学号2_姓名2_lab5**"，文件上传至学在浙大平台。


本实验以双人组队的方式进行，**仅需一人提交实验，**默认平均分配两人的得分（若原始打分为X，则可分配分数为2X，平均分配后每人得到X分）。如果有特殊情况请单独向助教反应，出示两人对于分数的钉钉聊天记录截图。单人完成实验者的得分为原始打分。



| 姓名 | 学号 | 分工 | 分配分数 |
| --- | --- | --- | --- |
| 孔郁杰 | 3200105109 | 3.2、3.3 | 50% |
| 何家骏 | 3200105876 | 3.4，3.5，3.6 | 50% |



## 3 实验步骤


### 3.1 搭建实验环境


#### 3.1.1 建立映射


你可以复用上一次实验中映射好的文件夹，也可以重新创建一个容器做映射。

#### 3.1.2 组织文件结构


你可以从 [Lab 5_ 代码.zip](https://yuque.zju.edu.cn/attachments/yuque/0/2022/zip/25434/1667271118612-3757de1b-93c2-4957-b688-1a93d319be92.zip?_lake_card=%7B%22uid%22%3A%221667271118428-0%22%2C%22src%22%3A%22https%3A%2F%2Fyuque.zju.edu.cn%2Fattachments%2Fyuque%2F0%2F2022%2Fzip%2F25434%2F1667271118612-3757de1b-93c2-4957-b688-1a93d319be92.zip%22%2C%22name%22%3A%22Lab+5_+%E4%BB%A3%E7%A0%81.zip%22%2C%22size%22%3A31706%2C%22type%22%3A%22application%2Fzip%22%2C%22ext%22%3A%22zip%22%2C%22progress%22%3A%7B%22percent%22%3A99%7D%2C%22status%22%3A%22done%22%2C%22percent%22%3A0%2C%22id%22%3A%22gWLXW%22%2C%22card%22%3A%22file%22%7D) 下载本实验提供好的代码。

```cpp
 .
├──  arch
│  └──  riscv
│     ├──  boot
│     ├──  include
│     │  ├──  buddy.h 新加
│     │  ├──  sched.h 有修改
│     │  ├──  slub.h  新加
│     │  └──  vm.h    有修改
│     ├──  kernel
│     │  ├──  buddy.c 新加
│     │  ├──  entry.S 
│     │  ├──  head.S
│     │  ├──  Makefile
│     │  ├──  sched.c 有修改
│     │  ├──  slub.c  新加
│     │  ├──  strap.c
│     │  ├──  vm.c
│     │  └──  vmlinux.lds
│     └──  Makefile
├──  include
│  ├──  list.h
│  ├──  put.h
│  ├──  rand.h
│  ├──  riscv.h
│  ├──  stddef.h
│  └──  stdint.h
├──  init
│  ├──  main.c
│  └──  Makefile
├──  lib
│  ├──  Makefile
│  ├──  put.c
│  └──  rand.c
└──  Makefile
```


本次实验基于 Lab 4 的框架基础上继续进行，为你添加了 `buddy.h` ， `slub.h` ， `buddy.c` ， `slub.c` 这四个有关动态内存分配的文件。并为你添加了 `list.h` 的链表文件，提供一个基本的数据结构。


最后，本实验在 `sched.c` 中修改了 `dead_loop` 函数的内容，该函数是每个进程默认执行的函数，其会在当前进程 `PID!=0` 的时候，使用本实验要求编写的 `mmap` 函数和缺页异常检查，并输出相关信息。请不要修改本函数内容。


### 3.2 实现 buddy system （40%）


在 Lab 4 中，我们为每个进程创建的页（用来保存 `task_struct` 和内核栈）都是直接从当前内存的末尾位置直接追加一个页实现的。这样虽然实现简单，但难以不再使用的页，也无法有效的按需分配页的大小。从本节开始，我们将实现一个页级的动态内存分配器——Buddy System。


#### 3.2.1 基础思路


如果只是简单的分配页，那还有更加简单的方法，比如用一个队列来存储当前空闲的页，需要分配时就从队列中取出，不使用后再入队即可。然而，内核经常需要和硬件交互，有些硬件要求数据存放在内存中的位置是连续的，即连续的物理内存，因此内核需要一种能够高效分配连续内存的方法。


Buddy System 的思路十分简单，请看下图。


```c
假设我们系统一共有8个可分配的页面，可分配的页面数需保证是2^n

memory: （这里的 page 0～7 计做 page X）
+--------+--------+--------+--------+--------+--------+--------+--------+
| page 0 | page 1 | page 2 | page 3 | page 4 | page 5 | page 6 | page 7 |
+--------+--------+--------+--------+--------+--------+--------+--------+

Buddy System 将连续的内存不断二等分，直到达到最小的单位页为止。

以上图为例，假设 Buddy System 管理 8 个页面，那么其将会被分成如下的连续物理内存区域：

连续 8 个 Page：0~7
连续 4 个 Page：0~3     4~7
连续 2 个 Page：0~1 2~3 4~5 6~7
连续 1 个 Page：0 1 2 3 4 5 6 7

把它画成树形结构，如下（值表示连续的可以分配的页的长度）
 
8 +---> 4 +---> 2 +---> 1 Page 0
  |       |       |
  |       |       +---> 1 Page 1
  |       |
  |       +---> 2 +---> 1 Page 2
  |               |
  |               +---> 1 Page 3
  |
  +---> 4 +---> 2 +---> 1 Page 4
          |       |
          |       +---> 1 Page 5
          |
          +---> 2 +---> 1 Page 6
                  |
                  +---> 1 Page 7
```


这样分配内存的时候，我们首先需要将大小对齐到 2 的幂次，然后在上面的区域中寻找可以分配的位置。比如：如果现在需要一个长度为 2 的区域，只需要分配 `0~1` 即可，如果还需要长度为 2 的区域，就分配 `2~3` 即可。如果需要长度为 4 的区域，那就分配 `4~7` 即可（ `0~3` 已经被占用了）。


再配合一下上面画的图，我们分配内存的过程可以抽象成如下过程：

1. 首先需要将分配的内存大小对齐到 2 的幂次
1. 从树根开始寻找
1. 如果当前节点恰好和需要分配的内存长度相等，那就分配到当前节点对应的位置上
1. 如果左儿子剩余的连续空间大于内存，递归左儿子
1. 如果右儿子剩余的连续空间大于内存，递归右儿子
1. 否则无法分配



例子如下：
```c
memory: （这里的 page 0～7 计做 page X）
  
+--------+--------+--------+--------+--------+--------+--------+--------+
| page 0 | page 1 | page 2 | page 3 | page 4 | page 5 | page 6 | page 7 |
+--------+--------+--------+--------+--------+--------+--------+--------+
  
alloc_pages(3) 请求分配3个页面

由于buddy system每次必须分配2^n个页面，所以我们将 3 个页面向上扩展至 4 个页面。
  
我们从树根出发查找到恰好满足当前大小需求的节点： 
        ↓
8 +---> 4 +---> 2 +---> 1
  |       |       |
  |       |       +---> 1
  |       |
  |       +---> 2 +---> 1
  |               |
  |               +---> 1
  |
  +---> 4 +---> 2 +---> 1
          |       |
          |       +---> 1
          |
          +---> 2 +---> 1
                  |
                  +---> 1

找到了 0~3 这个对应连续长 4 的节点，由于分配出去了，所以现在该节点只能能分配的连续长度为 0。

↓       ↓
4 +---> 0 +---> 2 +---> 1
  |       |       |
  |       |       +---> 1
  |       |
  |       +---> 2 +---> 1
  |               |
  |               +---> 1
  |
  +---> 4 +---> 2 +---> 1
          |       |
          |       +---> 1
          |
          +---> 2 +---> 1
                  |
                  +---> 1  

由于树根所对应的 8 个物理地址连续页，其中的 4 个被分配出去了

所以树根对应的可分配物理地址连续的页数为 4 个。
```


对应的，回收内存的时候，只需要把对应节点的连续内存大小标回原始大小即可。但需要注意，如果回收之后，左右儿子都处于空闲状态，那么还需要更新其父亲节点的信息。（相当于有两块区域连起来了）


比如，如果我们占用了 `0~1` 这个区域，那么 `0~3` 这个连续长 4 的区域其实就没法用了。但因此回收 `0~1` 区域的时候，也要把 `0~3` 这个连续长 4 的区域标记成可用。


如下例子：


```c
我们考虑 alloc_pages(1) 之后的状态：

4 +---> 2 +---> 2 +---> 1
  |       |       |
  |       |       +---> 1
  |       |           
  |       +---> 1 +---> 0 ← 要回收这个地方
  |               |
  |               +---> 1
  |
  +---> 4 +---> 2 +---> 1
          |       |
          |       +---> 1
          |
          +---> 2 +---> 1
                  |
                  +---> 1
  
    
先恢复对应节点的大小

4 +---> 2 +---> 2 +---> 1
  |       |       |
  |       |       +---> 1
  |       |   这个区域连接起来了，连续长 1 要变成连续成 2       
  |       +---> 2← +---> 1 恢复之前的状态  
  |                |
  |                +---> 1
  |
  +---> 4 +---> 2 +---> 1
          |       |
          |       +---> 1
          |
          +---> 2 +---> 1
                  |
                  +---> 1

：
      递归判断上去
4 +---> 4← +---> 2 +---> 1
  |       |       |
  |       |       +---> 1
  |       |        
  |       +---> 2 +---> 1
  |               |
  |               +---> 1
  |
  +---> 4 +---> 2 +---> 1
          |       |
          |       +---> 1
          |
          +---> 2 +---> 1
                  |
                  +---> 1
```


思想理解之后，接下来就是实现部分，一个满二叉树可以用一个数组实现。比如 `arr[1]` 代表根，他的左儿子就是 `arr[2]` ，右儿子就是 `arr[3]` ，其下标变化规律如下：如果当前节点的下标是 X，那么左儿子就是 `X * 2` ，右儿子就是 `X * 2 + 1` 。


这一部分可以用二进制来理解。假如当前节点是 `1110111` ，那么左儿子就是 `1110111 0` ，右儿子就是 `1110111 1` ，从树根 `1` 开始，一个满二叉树恰好可以填满 `1` 到 `1111..111` 的所有数字。(可以在纸上算算试试)


#### 3.2.2 实现对应数据结构以及接口


数据结构和接口定义在 `buddy.h` 文件当中，代码实现写在 `buddy.c` 中。


```c
struct buddy {
  unsigned long size;
  unsigned bitmap[8192];
};

void init_buddy_system(void);
void *alloc_pages(int);
void free_pages(void*);
```


- size：buddy system 管理的**页**数
- bitmap：用来记录未分配的内存页信息
- void init_buddy_system(void)：初始化 buddy system
- void *alloc_pages(int npages)：申请 n 个 page 空间，返回起始虚拟地址
- void free_pages(void* addr)：释放以 addr 为基地址的空间块



这里提供一个实现讲解 [https://coolshell.cn/articles/10427.html](https://coolshell.cn/articles/10427.html)，你可以做参考理解过程，但请不要直接复制代码。


#### 3.2.3 实验相关但 Buddy System 无关部分


1. 你需要定义一个 `struct buddy` 类型的全局变量，用来管理内核内存空间分配。
1. 将 `init_buddy_system`  添加至**建立内核映射**之前。【即 `paging_init` 函数的开头】
1. 由于 kernel 的代码占据了一部分空间，在 `init_buddy_system` 初始化结束后，需要将 kernel 部分已经占用的空间标记为已分配。可以直接使用  `alloc_pages`  将这段空间标记为已分配，已经占据的物理内存空间为 `text_start` 到 `_end` ，需要计算一下占用了多少页。
1. 修改之前 `create_mapping` 实现的物理页分配逻辑【之前是直接从 `_end` 之后分配新的页，现在需要使用 `alloc_pages` 分配】
1. 在虚拟地址空间下， `alloc_pages` 要返回虚拟地址空间下的 page 地址。比如当前运行的地址空间是 `0xffffffe000000000` 虚拟地址的时候，就要返回 `0xffffffe00000????` ，可以利用 `(uint64_t)&text_start` 做偏移，如果忘记了怎么使用，可以查看 [Lab 4 3.1.1 的蓝色文字提示](https://yuque.zju.edu.cn/os/lab/wbqw31#a77ed9c5)。



例子：buddy system 管理 `0x80000000` 开始的 16MB 空间，Kernel 代码占掉了 `0x80000000` ~ `0x80007000` 的位置，buddy system 的数据结构存在 `0x80008000` ~ `0x8000a000` ，因此，第一个空闲的 page 的地址就是 `0x8000b000` ，当调用 `alloc_page(1)` 的时候，需要返回 `0x8000b000` ，在运行 `paging_init` 函数之前先运行 `init_buddy_system` ，这样创建页表的时候，如果需要新的页，就调用 `alloc_page(1)` 即可。页表建立完成后，程序会进入到虚拟地址空间 `0xffffffe000000000` 去运行，之后再调用 `alloc_page(1)` 就要返回 `0xffffffe000000c00` 了。

最后，请将你编写好的 buddy.c 文件复制到下面代码框内。

```c++
#include "buddy.h"

#define IS_POWER_OF_2(N) ((N) > 0 && (((N) & ((N) - 1)) == 0))
#define LEFT_LEAF(INDEX) ((INDEX) * 2 + 1)
#define RIGHT_LEAF(INDEX) ((INDEX) * 2 + 2)
#define PARENT(INDEX) (((INDEX) - 1) / 2)
#define MAX(A, B) ((A) > (B) ? (A) : (B))

#define BUDDY_SIZE 1024
buddy mybuddy;
extern unsigned long long text_start;

// the root's index is 0
void init_buddy_system(void)
{
    unsigned node_size;

    mybuddy.size = BUDDY_SIZE;
    node_size = BUDDY_SIZE * 2;

    for (int i = 0; i < 2 * BUDDY_SIZE - 1; i++) {
        if (IS_POWER_OF_2(i + 1))
            node_size /= 2;
        mybuddy.bitmap[i] = node_size;
    }
    return;
}

void *alloc_pages(int npages)
{
    unsigned index = 0;
    unsigned node_size;
    unsigned offset = 0;

    // if uninited
    if (mybuddy.size == 0) {
        printf("no init");
        return -1;
    }

    // fix npages
    if (npages <= 0)
        npages = 1;
    else if (!IS_POWER_OF_2(npages)) {
        unsigned ret = 1;
        while (ret < npages)
            ret = ret << 1;
        npages = ret;
    }

    // unenough place
    if (mybuddy.bitmap[index] < npages) {
        printf("no place");
        return -1;
    }

    // find the node index
    for(node_size = mybuddy.size; node_size != npages; node_size /= 2 ) {
        if (mybuddy.bitmap[LEFT_LEAF(index)] >= npages)
            index = LEFT_LEAF(index);
        else
            index = RIGHT_LEAF(index);
    }

    // get offset index of pages
    mybuddy.bitmap[index] = 0;
    offset = (index + 1) * node_size - mybuddy.size;

    // trace back
    while (index) {
        index = (int)PARENT(index);
        mybuddy.bitmap[index] = MAX(mybuddy.bitmap[LEFT_LEAF(index)], mybuddy.bitmap[RIGHT_LEAF(index)]);
    }
    return (void*)(unsigned long long)((unsigned long long)&text_start + 0x1000 * offset);
}

void free_pages(void* addr)
{
    unsigned node_size, index = 0;
    unsigned left_bitmap, right_bitmap;

    unsigned offset = ((unsigned long long)addr - (unsigned long long)&text_start) / (unsigned long long)0x1000;

    node_size = 1;
    index = offset + mybuddy.size - 1;

    for(; mybuddy.bitmap[index]; index = PARENT(index)) {
        node_size *= 2;
        if (index == 0)
            return;
    }

    mybuddy.bitmap[index] = node_size;

    while (index) {
        index = PARENT(index);
        node_size *= 2;

        left_bitmap = mybuddy.bitmap[LEFT_LEAF(index)];
        right_bitmap = mybuddy.bitmap[RIGHT_LEAF(index)];

        if (left_bitmap + right_bitmap == node_size)
            mybuddy.bitmap[index] = node_size;
        else
            mybuddy.bitmap[index] = MAX(left_bitmap, right_bitmap);
    }
}
```



### 3.3 实现 kmalloc 和 kfree （10%）


Buddy System 管理的是页级别，还不能满足内核需要动态分配小内存的需求（比如动态分配一个比较小的 `struct` ），管理小内存使用的是 SLUB 分配器，由于 SLUB 分配器代码比较复杂，本次实验直接给出，其功能依赖 Buddy System 的功能，因此你需要正确的实现 Buddy System 的接口后，才能使用 SLUB 分配器。


> SLUB 不要求做过多了解，这里仅仅简单介绍，更多信息可以搜索学习。



SLUB 如何分配小内存？首先从 Buddy System 申请一个页 （4096字节），分成长度为 8 字节的对象 512 个，每当申请一个小于 8 字节的空间的时候，就分配一个出去，回收的时候直接收回即可，【具体实现中使用了链表】


那如果需要长度大于 8 的空间呢？只需要再创建一个 SLUB，分成长度为 16 字节的对象 256 个即可，申请空间在 9 ~ 16 的时候，就用这个 SLUB 来分配。


同理还可以创建 32，64， ...，2048 的 SLUB 分配器，这样就完成了小内存动态分配的功能。


以下是两者的关系。


- Buddy System 是以**页 (Page) **为基本单位来分配空间的，其提供两个接口 **alloc_pages (申请若干页)** 和**free_pages (释放若干页)**
- SLUB 在初始化时，需要预先申请一定的空间来做数据结构和 Cache 的初始化，此时依赖于 Buddy System提供的上述接口。
- 实验所用的 SLUB 提供8, 16, 32, 64, 128, 256, 512, 1024, 2048（单位：Byte）九种object-level（的内存分配/释放功能。(**kmem_cache_alloc （分配内存） /  kmem_cache_free （释放内存）**)
- slub.c 中的 (**kmalloc / kfree**) 提供内存动态分配/释放的功能。根据请求分配的空间大小，来判断是通过kmem_cache_alloc 来分配 object-level 的空间，还是通过 alloc_pages 来分配 page-level 的空间。kfree同理。



#### 3.3.1  slub 接口说明


`slub.c` 中有全局变量 `struct kmem_cache *slub_allocator[NR_PARTIAL] = {};` ，存储了所有的 SLUB 分配器的指针，他们管理的 Object-level 大小分别为 `8, 16, 32, 64, 128, 256, 512, 1024, 2048` 。


你只需要用到以下两个接口。


```c
void *kmem_cache_alloc(struct kmem_cache *cachep)
```


传入 SLUB 分配器的指针，返回该分配器管理的 Object-level 对应大小一个内存的首地址。
例子： `kmem_cache_alloc(slub_allocator[0])` 代表分配一个大小为 8 字节的空间，返回值就是这段空间的首地址。


```c
void kmem_cache_free(void *obj)
```


传入一个内存地址，管理该内存地址的 SLUB 分配器将回收该内存地址。
例子： `kmem_cache_free(obj)` 代表回收 obj 地址开头的这段空间，这个地址对应的空间大小在 slub 分配的时候有对应的自动存储管理，调用该函数不需要考虑。


#### 3.3.2 实现内存分配接口 kmalloc 和 kfree


最后，将你编写好的 kmalloc 函数复制到下面代码框内。


```c
void *kmalloc(size_t size) {
  int objindex;
  void *p;

  if (size == 0) return NULL;

  // size 若在 kmem_cache_objsize 所提供的范围之内，则使用 slub allocator
  // 来分配内存
  for (objindex = 0; objindex < NR_PARTIAL; objindex++) {
    // YOUR CODE HERE
    // 1. 判断 size 是否处于 slub_allocator[objindex] 管理的范围内
    // 2. 如果是就使用 kmem_cache_alloc 接口分配
    if(size <= slub_allocator[objindex]->size) {
      p = kmem_cache_alloc(slub_allocator[objindex]);
      break;
    }
  }

  // size 若不在 kmem_cache_objsize 范围之内，则使用 buddy system 来分配内存
  if (objindex >= NR_PARTIAL) {
    // YOUR CODE HERE
    // 1. 使用 alloc_pages 接口分配
    int npages = size / PAGE_SIZE;
    p = alloc_pages(npages);
    set_page_attr(p, (size - 1) / PAGE_SIZE, PAGE_BUDDY);
  }

  return p;
}
```


该函数的功能是申请大小为 size 字节的连续物理内存，成功返回起始地址，失败返回 NULL。


- 每个 slub allocator 分配的对象大小见**kmem_cache_objsize**。
- 当请求的物理内存**小于等于**2^11字节时，从`slub allocator` 中分配内存。
- 当请求的物理内存大小**大于**2^11字节时，从`buddy system` 中分配内存。



最后，将你编写好的 kfree 函数复制到下面代码框内。


```c
void kfree(const void *addr) {
  struct page *page;

  if (addr == NULL) return;

  // 获得地址所在页的属性
  page = ADDR_TO_PAGE(addr);

  // 判断当前页面属性
  if (page->flags == PAGE_BUDDY) {
    // YOUR CODE HERE
    // 1. 使用 free_pages 接口回收
    free_pages(addr);

    clear_page_attr(ADDR_TO_PAGE(addr)->header);

  } else if (page->flags == PAGE_SLUB) {
    // YOUR CODE HERE
    // 1. 使用 kmem_cache_free 接口回收
    kmem_cache_free(addr);
  }

  return;
}
```


该函数的功能是回收 `addr` 内存，每一块分配出去的内存都有用 flags 变量保存是 buddy system 分配的还是用 slub 分配器分配的，你只需要对应接口回收即可。


### 3.4 实现各进程页表 （20%）


现在我们要为模拟每个进程真正运行时的内存地址。在 Lab 4 中提到，内存地址 63~38 位均为 0 的时候，为每个进程的用户态内存地址空间，均为 1 的时候为内核态地址空间。也就是说，用户态下，每个进程都平等的占用 `0x0` ~ `0x0000003fffffffff` 的虚拟内存地址空间。那么如果每个进程都访问 0x0 这个位置，难道不会产生冲突吗？答案是为了解决这个冲突，我们需要为每个进程新加一个 `uint64_t satp` 变量，用来存储本进程的页表地址，当切换进程的时候，页表也一同切换到该进程的页表即可（也就是修改一下 satp 寄存器即可），这样只要每个进程 `0x0` 对应的物理内存地址不同就不会冲突了。


Lab 6 中将会进一步实现进程用户态的切换，本实验仍然均在内核态下执行。


#### 3.4.1 数据结构


```cpp
struct mm_struct {
  unsigned long long satp;
  struct vm_area_struct *vm;
};
```


在 `sched.h` 中，我们定义了 `mm_struct` 结构体来保存页表的首地址 `satp` ， `vm_area_struct` 将在下文介绍，此处可忽略。


#### 3.4.2 建立页表


虽然每个进程的页表都是独立的，比如 0x0 虚拟地址对应的物理地址均不一样，但是他们对于内核态下的虚拟内存地址的映射都是一致的。这里简单起见，我们沿用 Lab 4 的虚拟内存分配方法。


- 调用 `create_mapping` 函数将内核起始（`0x80000000`）的 16MB 空间映射到高地址（以 `0xffffffe000000000` 为起始地址）。
- 对内核起始地址（ `0x80000000` ）的16MB空间做等值映射。
- 将必要的硬件地址（如 `0x10000000` 为起始地址的 UART ）进行等值映射，无偏移。



然后我们在 `task_init` 初始化 `task_struct` 结构体时，可以为每个进程都创建一模一样的页表。


目前为止，task_init 需要做的事情整理如下

1. 循环每个进程
1. 为每个进程 `alloc_pages` 一个页
1. 页的开头存储本进程的 `task_struct` 结构体，如同 Lab 3 设置好相关信息
1. 本进程的栈指针 sp 指向本页的末尾
1. 为该进程创建一模一样的页表。页表映射方法如上所示。（基本可以理解为直接复制 paging_init 的代码，但要此时程序已经运行在 `0xfff....` 的虚拟内存地址空间下了，代码可能需要做些调整，create_mapping 的时候要注意转换为 `0x80000000` 的物理内存空间）
1. 该进程的 `mm.satp` 赋值成创建好的页表的首地址。



最后，将你编写好的 task_init 函数复制到下面代码框内。



```c++
void task_init(void) {
  for(int i=0;i <5; ++i)
  {
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
    pgtbl=(uint64_t)alloc_pages(1);
    
    if(pgtbl > (uint64_t)(0xf000000000000000)) pgtbl = (uint64_t)((uint64_t)(pgtbl) - (uint64_t)(offset));
    
    task[i]->mm.satp = 0x8000000000000000|((uint64_t)pgtbl>>12);;
    task[i]->mm.vm=NULL;
    create_mapping(pgtbl, (uint64_t)0xffffffe000000000, (uint64_t)0x80000000, (uint64_t)1<<24,7);
    create_mapping(pgtbl, (uint64_t)0x80000000, (uint64_t)0x80000000,( uint64_t)1<<24,7);
    create_mapping(pgtbl, (uint64_t)0x10000000, (uint64_t)0x10000000, (uint64_t)1<<20,7);

    printf("[PID = %d] Process Create Successfully!\n", task[i]->pid);

  }

  task_init_done=1;
  current=task[0];
}
```




#### 3.4.3 切换页表


有了上述建立好的页表和 `satp` 变量之后，只需要在 `switch_to` 的时候修改一下 `satp` 寄存器就好了。


你需要在 `switch_to` 函数内， `__switch_to` 函数之前修改 `satp` 寄存器为要切换的进程的 `satp` 变量，这一步你可以使用内联汇编实现。


最后，将你编写好的切换页表的部分代码复制到下面代码框内。



```c
void switch_to(struct task_struct *next) {
  if(next->pid!=current->pid){
    struct task_struct* prev=current;
    current=next;
    asm volatile(
      "csrw satp,%[csatp]\n"
      "sfence.vma\n"
      :
      :[csatp] "r"(next->mm.satp)
      :"memory"
    );
    __switch_to(prev,current);
  }
}
```




### 3.5 实现 mmap 函数 （10%）


现在我们有了 kmalloc 和 kfree 的动态内存分配函数，这两个函数仅能在内核下使用，是为内核进程提供动态内存分配用的（比如在内核态下可以写链表了），下面就用 mmap 的例子测试一下。


#### 3.5.1 前置知识
#### 
```c
void *mmap (void *__addr, size_t __len, int __prot,
                   int __flags, int __fd, __off_t __offset)
```


Linux 系统一般会提供一个 mmap 的系统调用，其作用是为当前进程的虚拟内存地址空间中分配新地址空间（从 `void *__addr` 开始, 长度为 `size_t __len`字节），创建和文件的映射关系（ `__flags` 和 `__fd` 和 `__offset` 是文件相关的参数）。它可以分配一段匿名的虚拟内存区域，参数 `int __prot`表示了对所映射虚拟内存区域的权限保护要求。


为什么是在进程的虚拟内存地址空间中分配，而不是在直接分配一块物理内存空间内存给进程呢？mmap 这样的设计可以保证 mmap 快速分配，并且按需分配物理内存给进程使用。比如一个进程申请了 1GB 大小的空间，那么该空间可以先申请在虚拟内存地址中，直到真正使用的时候，再通过缺页异常来分配物理内存。


而创建和文件的映射关系也有诸多好处，例如通过 mmap 机制以后，磁盘上的文件仿佛直接就在内存中，把访问磁盘文件简化为按地址访问内存。这样一来，应用程序自然不需要使用文件系统的 write（写入）、read（读取）、fsync（同步）等系统调用，因为现在只要面向内存的虚拟空间进行开发。

而又因为 Linux 下一切皆为文件，除了管理磁盘文件，还可以用 mmap 简化 socket，设备等其他 IO 管理。


#### 3.5.2 vm_area_struct


既然 mmap 系统调用只是在虚拟内存地址中分配空间，只要在缺页异常发生时才分配实际的物理内存，那么就肯定需要一个数据结构来记录每个进程申请过的虚拟内存地址空间。因此，为了在发生缺页异常的时候提供必要的信息给内核，我们需要一个结构来存储当前进程的虚拟内存地址空间分配的相关信息。即 `vm_area_struct` 。


`vm_area_struct` 是虚拟内存管理的基本单元，`vm_area_struct` 保存了有关连续虚拟内存区域(简称vma)的信息。linux 具体某一进程的虚拟内存区域映射关系可以通过 [procfs【Link】](https://man7.org/linux/man-pages/man5/procfs.5.html)读取 `/proc/pid/maps`的内容来获取:


比如，如下一个常规的 `bash`进程，假设它的进程号为 `7884`，则通过输入如下命令，就可以查看该进程具体的虚拟地址内存映射情况(部分信息已省略)。


```shell
#cat /proc/7884/maps
556f22759000-556f22786000 r--p 00000000 08:05 16515165                   /usr/bin/bash
556f22786000-556f22837000 r-xp 0002d000 08:05 16515165                   /usr/bin/bash
556f22837000-556f2286e000 r--p 000de000 08:05 16515165                   /usr/bin/bash
556f2286e000-556f22872000 r--p 00114000 08:05 16515165                   /usr/bin/bash
556f22872000-556f2287b000 rw-p 00118000 08:05 16515165                   /usr/bin/bash
556f22fa5000-556f2312c000 rw-p 00000000 00:00 0                          [heap]
7fb9edb0f000-7fb9edb12000 r--p 00000000 08:05 16517264                   /usr/lib/x86_64-linux-gnu/libnss_files-2.31.so
7fb9edb12000-7fb9edb19000 r-xp 00003000 08:05 16517264                   /usr/lib/x86_64-linux-gnu/libnss_files-2.31.so               
...
7ffee5cdc000-7ffee5cfd000 rw-p 00000000 00:00 0                          [stack]
7ffee5dce000-7ffee5dd1000 r--p 00000000 00:00 0                          [vvar]
7ffee5dd1000-7ffee5dd2000 r-xp 00000000 00:00 0                          [vdso]
ffffffffff600000-ffffffffff601000 --xp 00000000 00:00 0                  [vsyscall]
```


从中我们可以读取如下一些有关该进程内虚拟内存映射的关键信息：


- `vm_start` :  (第1列) 指的是该段虚拟内存区域的开始地址；
- `vm_end`   :  (第2列) 指的是该段虚拟内存区域的结束地址；
- `vm_flags`  :  (第3列) 该`vm_area`的一组权限(rwx)标志，`vm_flags`的具体取值定义可参考linux源代码的[这里(linux/mm.h)](https://elixir.bootlin.com/linux/latest/source/include/linux/mm.h#L250)
- `vm_pgoff`  :  (第4列) 虚拟内存映射区域在文件内的偏移量；
- `vm_file`  :  (第5/6/7列)分别表示：映射文件所属设备号/以及指向关联文件结构的指针(如果有的话，一般为文件系统的inode)/以及文件名；



其它保存在 `vm_area_struct`中的信息还有：


- `vm_ops`   :  该 `vm_area` 中的一组工作函数;
- `vm_next/vm_prev`: 同一进程的所有虚拟内存区域由**链表结构**链接起来。



`vm_area_struct` 结构已为你定义在 `sched.h` 文件中，如下。
```c
/* 虚拟内存区域 */
typedef struct {
  unsigned long pgprot;
} pgprot_t;
struct vm_area_struct {
  /* Our start address within vm_area. */
  unsigned long vm_start;
  /* The first byte after our end address within vm_area. */
  unsigned long vm_end;
  /* linked list of VM areas per task, sorted by address. */
  struct list_head vm_list;
  /* Access permissions of this VMA. */
  pgprot_t vm_page_prot;
  /* Flags*/
  unsigned long vm_flags;
};
```


#### 3.5.3 本实验的 mmap


`mmap` 的功能如上阐述。不过，在本次实验中 `mmap` 的功能有所简化，不涉及到文件映射，而是负责分配对应长度的若干个匿名内存页区域，并将这段内存区域的信息插入到当前进程 `current` 中的 `vm_area_struct`中，作为链表的一个新节点。具体的函数定义及参数含义如下：


```c
void *mmap (void *__addr, size_t __len, int __prot,
                   int __flags, int __fd, __off_t __offset)
```


- `size_t`与`__off_t`为`unsigned long`的类型别名
- `__addr`:**建议**映射的虚拟首地址，需要按页对齐
- `__len`: 映射的长度，需要按页对齐
- `__prot`: 映射区的权限
- `__flags`: 由于本次实验不涉及`mmap`在Linux中的其他功能，该参数无意义
- `__fd`: 由于本次实验不涉及文件映射，该参数无意义
- `__offset`: 由于本次实验不涉及文件映射，该参数无意义
- 返回值：**实际**映射的虚拟首地址，若虚拟地址区域 `[__addr, __addr+__len)` 未与其它地址映射冲突，则返回值就是建议首地址 `__addr`，若发生冲突，则需要更换到无冲突的虚拟地址区域，返回值为该区域的首地址。【为简化实验，你不需要考虑冲突的情况，只需要严格按照 __addr, __len 进行分配即可】



`mmap` 映射的虚拟地址区域采用需求分页（Demand Paging）机制，即：`mmap` 中不为该虚拟地址区域分配实际的物理内存，仅当进程试图访问该区域时，通过触发 Page Fault，由 Page Fault 处理函数进行相应的物理内存分配以及页表的映射。


**该函数的操作步骤如下：**

- 利用 kmalloc 创建一个 `vm_area_struct` 记录该虚拟地址区域的信息，并添加到 `mm` 的链表中
   - 调用 `kmalloc` 分配一个新的 `vm_area_struct`
   - 设置 `vm_area_struct` 的起始和结束地址为`start`, `start + length`
   - 设置 `vm_area_struct` 的 `vm_flags` 为 `prot`
   - 将该 `vm_area_struct` 插入到 `vm` 的链表中
- 返回地址区域的首地址



这样，使用 mmap 函数就可以为当前进程注册一段虚拟内存了，这段申请的相关信息存在本进程的 `task_struct` 的 `vm_area_struct` 中。


最后，将你编写好的 mmap 函数复制到下面代码框内。



```c++
void *mmap(void *__addr, size_t __len, int __prot, int __flags, int __fd,
           int __offset) {
  struct vm_area_struct *vm;
  vm = (struct vm_area_struct *)kmalloc(sizeof(struct vm_area_struct));
  vm->vm_start = (uint64_t)__addr;
  vm->vm_end = (uint64_t)__addr+__len;
  vm->vm_flags = __prot;
  INIT_LIST_HEAD(&(vm->vm_list));
  if(current->mm.vm==NULL)
  {
    current->mm.vm=(struct vm_area_struct *)kmalloc(sizeof(struct vm_area_struct));
    INIT_LIST_HEAD(&(current->mm.vm->vm_list));
  }
  list_add_tail(&(vm->vm_list), &(current->mm.vm->vm_list));
  return __addr;
}

```



### 3.6 缺页异常处理 （20%）


现在，每个进程分配过的虚拟内存地址空间可以使用 `current->vm` 来访问，在前面我们讲过，我们的进程是需求分页机制，直到进程使用该虚拟内存地址的时候，我们才会分配物理内存给他。


也就是说，进程使用该虚拟内存地址的时候会发生缺页异常，内核在缺页异常的处理函数中，通过检查 `current->vm` 数据结构，检查缺页异常的地址落在什么范围内。最后根据对应范围的相关信息做物理内存映射即可。


#### 3.6.1 缺页异常


缺页异常是一种正在运行的程序访问当前未由内存管理单元（MMU）映射到虚拟内存的页面时，由计算机硬件引发的异常类型。访问未被映射的页或访问权限不足，都会导致该类异常的发生。处理缺页异常通常是操作系统内核的一部分。当处理缺页异常时，操作系统将尝试使所需页面在物理内存中的位置变得可访问（建立新的映射关系到虚拟内存）。而如果在非法访问内存的情况下，即发现触发 Page Fault 的虚拟内存地址(Bad Address)不在当前进程 `vm_area_struct` 链表中所定义的允许访问的虚拟内存地址范围内，或访问位置的权限条件不满足时，缺页异常处理将终止该程序的继续运行。


当系统运行发生异常时，可即时地通过解析 scause 寄存器的值，识别如下三种不同的 Page Fault。



| Interrupt/Exception
scause[XLEN-1] | Exception Code
scause[XLEN-2:0] | Description |
| --- | --- | --- |
| 0 | 12 | Instruction Page Fault |
| 0 | 13 | Load Page Fault |
| 0 | 15 | Store/AMO Page Fault |

#### 
#### 3.6.2 处理缺页异常


RISC-V 下的中断异常可以分为两类，一类是 interrupt，另一类是 exception，具体的类别信息可以通过解析 scause[XLEN-1] 获得。在前面的实验中，我们已经实现在 interrupt 中添加了时钟中断处理，在本次实验中，我们将添加对于 Page Fault 的处理，即当 Page Fault 发生时，能够根据 scause 寄存器的值检测出该异常，并处理该异常。当 Page Fault 发生时 scause 寄存器可能的取值声明如下：


```c
# define CAUSE_FETCH_PAGE_FAULT    12
# define CAUSE_LOAD_PAGE_FAULT     13
# define CAUSE_STORE_PAGE_FAULT    15
```


具体处理流程如下：


1. 修改 `strap.c` 文件, 添加对于 Page Fault 异常的检测，（检查 scause 寄存器）
1. 读取 `csr STVAL` 寄存器，获得访问出错的虚拟内存地址（`Bad Address`），并打印出该地址和缺页异常类型。
1. 检查访问出错的虚拟内存地址（`Bad Address`）是否落在该进程所定义的某一`vm_area`中，即遍历当前进程的 `vm_area_struct` 链表，找到匹配的`vm_area`。若找不到合适的`vm_area`，则退出 Page Fault 处理，同时打印输出 `Invalid vm area in page fault`。
1. 根据 `csr SCAUSE` 判断 Page Fault 类型。
1. 根据 Page Fault 类型，检查权限是否与当前 `vm_area` 相匹配。
   1. 当发生 `Page Fault caused by an instruction fetch` 时，Page Fault 映射的 `vm_area` 权限需要为可执行；
   1. 当发生 `Page Fault caused by a read` 时，Page Fault 映射的 `vm_area` 权限需要为可读；
   1. 当发生 `Page Fault caused by a write` 时，Page Fault 映射的 `vm_area` 权限需要为可写；
   1. 若发生上述权限不匹配的情况，也需要退出 Page Fault 处理函数，同时打印输出 `Invalid vm area in page fault`。
   1. 只有匹配到合法的 `vma_area` 后，才可进行相应的物理内存分配以及页表的映射。



为了简化实验，本次实验缺页异常不要求检查权限是否匹配。只需要分配对应的物理内存和建立页表映射即可，同时需要打印缺页异常的地址和类型。也就是说，如果缺页异常的地址落在 vm_area 中，你需要为该 vm_area 对应的虚拟内存地址分配一段物理内存空间，这里用前面写好的 kmalloc 分配一段即可（注意 kmalloc 返回的空间是虚拟地址，需要用 offset 转换为物理空间）。接下来异常结束后，会回到原先发生缺页异常的指令，这时指令将不会再异常。


最后，将你编写好的有关缺页异常处理的部分复制到下面代码框内。



```c++
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

```




### 3.6 编译和测试 


仿照 Lab 4 进行编译及调试，对 `main.c` 做修改，确保输出你的学号与姓名。在项目最外层输入 `make run` 命令调用 Makefile 文件完成整个工程的编译及执行。


**如果编译失败，及时使用 `make clean` 命令清理文件后再重新编译。**


如果程序能够正常执行并打印出相应的字符串及你的学号，则实验成功。预期的实验结果如下：


```c
ZJU OS LAB 5
Student1:123456 张三 Student2:123456 李四
[PID = 0] Process Create Successfully!
[PID = 1] Process Create Successfully!
[PID = 2] Process Create Successfully!
[PID = 3] Process Create Successfully!
[PID = 4] Process Create Successfully!
[*PID = 0] Context Calculation: counter = 0,priority = 0
[ 0 -> 1 ] Switch from task 0[0xffffffe000036000] to task 1[0xffffffe0000d1000], prio: 1, counter: 1
0x0 CAUSE_STORE_PAGE_FAULT
[CHECK] page store OK
0x3000 CAUSE_LOAD_PAGE_FAULT
[CHECK] page load OK
[*PID = 1] Context Calculation: counter = 1,priority = 1
[ 1 -> 2 ] Switch from task 1[0xffffffe0000d1000] to task 2[0xffffffe0000f8000], prio: 2, counter: 2
0x0 CAUSE_STORE_PAGE_FAULT
[CHECK] page store OK
0x3000 CAUSE_LOAD_PAGE_FAULT
[CHECK] page load OK
[*PID = 2] Context Calculation: counter = 2,priority = 2
[*PID = 2] Context Calculation: counter = 1,priority = 2
[ 2 -> 3 ] Switch from task 2[0xffffffe0000f8000] to task 3[0xffffffe00011f000], prio: 3, counter: 3
0x0 CAUSE_STORE_PAGE_FAULT
[CHECK] page store OK
0x3000 CAUSE_LOAD_PAGE_FAULT
[CHECK] page load OK
[*PID = 3] Context Calculation: counter = 3,priority = 3
[*PID = 3] Context Calculation: counter = 2,priority = 3
[*PID = 3] Context Calculation: counter = 1,priority = 3
[ 3 -> 4 ] Switch from task 3[0xffffffe00011f000] to task 4[0xffffffe000146000], prio: 4, counter: 4
0x0 CAUSE_STORE_PAGE_FAULT
[CHECK] page store OK
0x3000 CAUSE_LOAD_PAGE_FAULT
[CHECK] page load OK
[*PID = 4] Context Calculation: counter = 4,priority = 4
[*PID = 4] Context Calculation: counter = 3,priority = 4
[*PID = 4] Context Calculation: counter = 2,priority = 4
[*PID = 4] Context Calculation: counter = 1,priority = 4
```


请在此附上你的实验结果截图。

![image](https://github.com/Guahuan/OSLAB5/blob/main/test.png)

