#include "buddy.h"

#define IS_POWER_OF_2(N) ((N) > 0 && (((N) & ((N) - 1)) == 0))
#define LEFT_LEAF(INDEX) ((INDEX) * 2 + 1)
#define RIGHT_LEAF(INDEX) ((INDEX) * 2 + 2)
#define PARENT(INDEX) (((INDEX) - 1) / 2)
#define MAX(A, B) ((A) > (B) ? (A) : (B))

#define BUDDY_SIZE 128
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
    if (mybuddy.size == 0)
        return -1;

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
    if (mybuddy.bitmap[index] < npages)
        return -1;

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
    return (unsigned long long)((unsigned long long)&text_start + 0x1000 * offset);
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