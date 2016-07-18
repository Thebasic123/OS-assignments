//#include <vm.h>

//this file is the data structure of region element

typedef struct region_element{
    vaddr_t vbase;
    size_t npages;
    int permission;
    struct region_element * next;
} region;
