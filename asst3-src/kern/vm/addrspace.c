/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>
#include <elf.h>
/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 *
 * UNSW: If you use ASST3 config as required, then this file forms
 * part of the VM subsystem.
 *
 */

struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		//panic("null as 1\n");
		return NULL;
	}

	/*
	 * Initialize as needed.
	 */
	as->head_region = NULL;
	//as->vstackbase = 0;
	as->isPrepared = 0;
	//we use one page for page table, each entry is 4bytes 
	//so we should have 1024 elements in page table including 1levle and 2level
	as->pagetable = (PTE **)alloc_kpages(1);
	//page table is a lazy strucutre, so we assign null pointers when we initialize
	for(int i=0;i<1024;i++){
		as->pagetable[i] = NULL;
	}

	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;

	newas = as_create();
	if (newas==NULL) {
		panic("null as 2\n");
		return ENOMEM;
	}

	/*
	 * Write this.
	 */
	kprintf("using wrong shit copy\n");
	newas->isPrepared = old->isPrepared;
	//newas->vstackbase = old->vstackbase;
	//copy page table
	for(int i=0;i<1024;i++){
		//newas->pagetable[i] = old->pagetable[i];
		// if(old->pagetable[i]!= NULL){
		// 	vaddr_t copy = alloc_kpages(1);
		// 	if(copy == 0){
		// 		as_destroy(newas);
		// 		return ENOMEM;
		// 	}
		// 	memmove((void *)copy,(const void *)old->pagetable[i],PAGE_SIZE);
		// 	newas->pagetable[i] = (PTE *)copy;
		// }
		newas->pagetable[i] = NULL;
	}


	//copy region list
	//old as headRegion is null then we don't need to do anything
	// if(old->head_region == NULL){
	// 	return 0;
	// }
	struct region_element * current_old;
	current_old = old->head_region;
	struct region_element * current_new;
	current_new = newas->head_region;
	while(current_old != NULL){
		struct region_element * temp = kmalloc(sizeof(struct region_element));
		if(temp == NULL){
			as_destroy(newas);
			//panic("null region 1\n");
			return ENOMEM;
		}
		temp->vbase = current_old->vbase;
		temp->npages = current_old->npages;
		temp->permission = current_old->permission;
		temp->next = NULL;
		if(newas->head_region == NULL){
			newas->head_region = temp;
		}else{
			current_new = newas->head_region;
			while(current_new->next != NULL){
				current_new = current_new->next;
			}
			current_new->next = temp;
		}
		current_old = current_old->next;

	}
	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	/*
	 * Clean up as needed.
	 */
	//free pagetable
	//free 2-level first
	for(int i=0;i<1024;i++){
		if(as->pagetable[i] != NULL){
			free_kpages((vaddr_t)as->pagetable[i]);
		}
	}

	//free 1-level
	free_kpages((vaddr_t)as->pagetable);
	//free regions
	struct region_element * current;
	while(as->head_region != NULL){
		current = as->head_region;
		as->head_region = as->head_region->next;
		kfree(current);
	}
	free_kpages((vaddr_t)as);
}

void
as_activate(void)
{
	int i, spl;
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		/*
		 * Kernel thread without an address space; leave the
		 * prior address space in place.
		 */
		return;
	}

	/*
	 * Write this.
	 */
	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */

int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
		 int readable, int writeable, int executable)
{
	/*
	 * Write this.
	 */
	int reminder = memsize % PAGE_SIZE;
	//vaddr = vaddr - reminder;
	vaddr &= PAGE_FRAME; // some address has offset
	vaddr = vaddr - reminder;
	size_t numberOfPages = 0;
	if(reminder == 0){
		numberOfPages = memsize / PAGE_SIZE;
	}else{
		numberOfPages = memsize / PAGE_SIZE + 1;
	}
	// we need to make sure it points to the base of a page

	int flag = 0;
	if(readable){
		flag |= PF_R;
	}
	if(writeable){
		flag |= PF_W;
	}
	if(executable){
		flag |= PF_X;
	}
	// go through region list, find first null pointer
	if(as->head_region == NULL){
		struct region_element * temp;
		temp = kmalloc(sizeof(struct region_element));
		if (temp == NULL) { // if we can't create temp then, return error
			//panic("null region 3\n");
			return ENOMEM;
		}
		temp->vbase = vaddr;
		temp->npages = numberOfPages;
		temp->permission = flag;
		temp->next = NULL;
		as->head_region = temp;
		return 0;
	}
	struct region_element * current = as->head_region;
	while(current->next != NULL){
		current = current->next;
	}
	//set current next
	struct region_element * temp = kmalloc(sizeof(struct region_element));
	if (temp == NULL) { 
		//panic("null region 4\n");
		return ENOMEM;
	}
	temp->vbase = vaddr;
	temp->npages = numberOfPages;
	temp->permission = flag;
	temp->next = NULL;
	current->next = temp;	
	return 0; 
}

int
as_prepare_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */
	// we are going to prepare the last one in region list and give it write permission
	region * current;
	current = as->head_region;
	while(current->next != NULL){
		current = current->next;
	}
	//prepare current
	//check whether region is writeable or not, 
	//if it is writeable already, then do nothing
	if(current->permission & PF_W){
		return 0;
	}else{
		as->isPrepared = 1;
		current->permission |= PF_W;
	}
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	//if region was writeable before prepare_load function call
	//then we shouldn't do anything
	if(as->isPrepared == 0){
		return 0;
	}
	as->isPrepared = 0;
	struct region_element * current;
	current = as->head_region;
	while(current->next != NULL){
		current = current->next;
	}
	current->permission &= ~PF_W;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{

	/*
	 * Write this.
	 */
	as_define_region(as,USERSTACK - 16*4096, 16*4096,
		 1, 1, 0);

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;
	//stackbase should be stack top - stacksize
	return 0;
}
