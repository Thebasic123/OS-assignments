#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>

/* Place your frametable data-structures here 
 * You probably also want to write a frametable initialisation
 * function and call it from vm_bootstrap
 */

// each entry uses n bytes 
struct frame_table_entry{
    bool isFree;
    // if 0, frame is locked, otherwise frame is not looked
    // we only lock frames which allocate by OS
    paddr_t address;
};

static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;
static struct spinlock frame_lock = SPINLOCK_INITIALIZER;
int total_frame_number = 0;
char hasInitialized = 0;

void ft_initialization(void){
    // find location for frame table first
    vaddr_t location = 0;
    paddr_t ram_size = ram_getsize();
    paddr_t first = ram_getfirstfree();
    int ft_frames = 0;//frames used by frame table
    //int os_frames = 0; //frames used by os at the beginning 
    int size_of_ft_entry = 0;
    size_of_ft_entry = sizeof(struct frame_table_entry);
    //number of total page frames
    total_frame_number = (ram_size - first)/PAGE_SIZE;

    if((total_frame_number*size_of_ft_entry)% PAGE_SIZE == 0){
      ft_frames = total_frame_number*size_of_ft_entry/PAGE_SIZE;
    }else{
      ft_frames = (total_frame_number*size_of_ft_entry/PAGE_SIZE) + 1;
    }
    location = ram_size - (total_frame_number*size_of_ft_entry);
    frame_table = (struct frame_table_entry *)(PADDR_TO_KVADDR(location));
   	//initialize all frame paddr
   	for(int i = 0;i<total_frame_number;i++){
   		if(i == 0){
   			frame_table[0].isFree = true;
   			frame_table[0].address = (first & PAGE_FRAME) + PAGE_SIZE;
        //frame_table[0].locked = 0;
   		}else {
        frame_table[i].isFree = true;
        frame_table[i].address = frame_table[i-1].address + PAGE_SIZE;
       // frame_table[0].locked = 0;
      }
   	}

   	for(int i=0;i<ft_frames;i++){
   		frame_table[total_frame_number-1-i].isFree = false;
   	}
    hasInitialized = 1;
}

/* Note that this function returns a VIRTUAL address, not a physical 
 * address
 * WARNING: this function gets called very early, before
 * vm_bootstrap().  You may wish to modify main.c to call your
 * frame table initialisation function, or check to see if the
 * frame table has been initialised and call ram_stealmem() otherwise.
 */

//get index of first free frame in frame table
//currently haven't implemented replacement policy like FIFO
paddr_t get_frame_address(void){
  paddr_t result = 0;
  int has_found = 0;//if we havn't found index, set to 0,otherwise to 1
  //if there is a free page in frame table
  //spinlock_acquire(&frame_lock);
  for(int i=0;i<total_frame_number;i++){
    spinlock_acquire(&frame_lock);
    if(frame_table[i].isFree==true){
      result = frame_table[i].address;
      frame_table[i].isFree = false;
      has_found = 1;
      spinlock_release(&frame_lock);
      break;
    }
    spinlock_release(&frame_lock);
  }
  //spinlock_release(&frame_lock);
  if(has_found == 0 && result == 0){//we haven't found, implement replacement policy
    //TODO
   // panic("frame table full\n");
    return ENOMEM;
  }
  return result;
}



vaddr_t alloc_kpages(unsigned int npages)
{
  /*
   * IMPLEMENT ME.  You should replace this code with a proper implementation.
   */

  //if frame table hasn't been initialized, we need to call ram_stealmem
  //otherwise, we just need to collect a page(frame)

  paddr_t addr;
  vaddr_t result;
  //int index = 0;
  // if we have initialized frame table, just use FT
  if(hasInitialized == 1){
    addr = get_frame_address();
  }else{
    spinlock_acquire(&stealmem_lock);
    addr = ram_stealmem(npages);
    spinlock_release(&stealmem_lock);
  }

  if(addr == 0)
    return 0;
  result = PADDR_TO_KVADDR(addr);
  bzero((void *)result,PAGE_SIZE);
  //we need to zero out the page we collect
  return result;
}


void free_kpages(vaddr_t addr)
{
  paddr_t target = 0;
  target = KVADDR_TO_PADDR(addr);
  for(int i=0;i<total_frame_number;i++){
    spinlock_acquire(&frame_lock);
    if(frame_table[i].address==target){
      frame_table[i].isFree=true;
     spinlock_release(&frame_lock);
      break;
    }
   spinlock_release(&frame_lock);
  }
}






