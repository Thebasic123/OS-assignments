#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/seek.h>
#include <lib.h>
#include <uio.h>
#include <thread.h>
#include <current.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>
#include <file.h>
#include <syscall.h>
#include <copyinout.h>
#include <proc.h>



/*
 * Add your file-related functions here ...
 */

// use OFT lock, when OFT data is required

int sys_open(const char *filename,int flags,mode_t mode,int *retval){

    //first I need to file an empty slot at current process's FD table
    int use_slot = 0;//slot index of fdt we are going to use
    int oft_index = 0;// index number which we will pass into FDT slot
    for(int i=1;i<32;i++){ // leave first slot 
        if(curproc->fd_table[i]== 0){
            use_slot = i;
            break;
        }
        if(i==31&&use_slot!=31){ // if we can't find an empty slot
            *retval = -1;            
            return EMFILE; // error message too many open files
        }   
    }
    
    //then we need to find an empty slot at open file table
    lock_acquire(OFT_lock);
    for(int i=2;i<32;i++){ // first two slots are always for stdout and stderr
	    if(of_table[i].ref_count == 0){
	        oft_index = of_table[i].number;
            break;
        }
	    if(i==31&&oft_index!=32){
           *retval = -1; 
           lock_release(OFT_lock);
	       return ENFILE;// error message too many open files in system
	    }
    }
    lock_release(OFT_lock);
	
    //last step, we need to use vfs_open set up this oft slot
    int error = 0;
    lock_acquire(OFT_lock);
    error=vfs_open((char *)filename,flags,mode,&of_table[oft_index-1].current_vnode);
    if(error!=0){ // if error is not 0 then return error to result
        *retval = -1;
        lock_release(OFT_lock);    
        return error;
    }
    of_table[oft_index-1].ref_count++;
    of_table[oft_index-1].flag=flags;
    lock_release(OFT_lock);
    curproc->fd_table[use_slot] = oft_index;//link fd_table to correct oft slot
    //set retval 
    *retval = use_slot;
    return 0;//sucessfully implement
}
//check whether few fd can link to same vnode
int sys_close(int fd){
    //check input
    if(fd<0 || fd>31){
        return EBADF;//error 
    }
    int OFT_index;
    OFT_index = curproc->fd_table[fd];
    OFT_index--;//since number is index+1
    //check index number
    if(OFT_index<0||OFT_index>31){
        return EBADF;
    }    
    //check whether slot is valid or not 
    lock_acquire(OFT_lock);     
    if(of_table[OFT_index].current_vnode==NULL){
        lock_release(OFT_lock);
        return EBADF;
    }
    
    //update FD node and OFT slot
    //not right, I need to keep eyes on ref_count
    curproc->fd_table[fd]=0;
    of_table[OFT_index].ref_count--;
    if(of_table[OFT_index].ref_count==0){
        vfs_close(of_table[OFT_index].current_vnode);
        of_table[OFT_index].current_vnode=NULL;
        of_table[OFT_index].flag = 0;
        of_table[OFT_index].offset = 0;
    }else{
        //do nothing
    }
    lock_release(OFT_lock);
    return 0;//successfully implement
}

int sys_write(int fd,userptr_t buf,size_t nbytes,ssize_t *retval){
    //check fd    
    if(fd<0 || fd>31){
        *retval = -1;
        return EBADF;//error 
    }
    int OFT_index;
    OFT_index = curproc->fd_table[fd];
    OFT_index--;//since number is index+1
    //check index number
    if(OFT_index<0||OFT_index>31){
        *retval = -1;
        return EBADF;
    }    
    //check whether slot is valid or not 
    lock_acquire(OFT_lock);       
    if(of_table[OFT_index].current_vnode==NULL){
        *retval = -1;
        lock_release(OFT_lock);
        return EBADF;
    }

    //check permission
    if((of_table[OFT_index].flag & (1<<0))||(of_table[OFT_index].flag & (1<<1))){
    //have permission to write
    // do nothing
    }else{// don't have permission
        *retval = -1;
        lock_release(OFT_lock);
        return EACCES;
    }
    lock_release(OFT_lock);

    //create uio and iovec for write
    struct iovec iov;
    struct uio u;
    enum uio_rw {UIO_READ,UIO_WRITE} rw = UIO_WRITE; 
    lock_acquire(OFT_lock);
    uio_kinit(&iov,&u,buf,nbytes,of_table[OFT_index].offset,rw);
    size_t old_resid = u.uio_resid;
    int error;
    error=VOP_WRITE(of_table[OFT_index].current_vnode,&u);
    size_t new_resid = u.uio_resid;
    lock_release(OFT_lock);
    //the difference between old_resid and new_resid is how many bytes were written
    if(error==0){
        *retval = old_resid - new_resid;
        //update offset
        lock_acquire(OFT_lock);
        of_table[OFT_index].offset += *retval;
        lock_release(OFT_lock); 
        return 0;
    }else{
        *retval = -1;
        return error;
    }
    return 0;//success    
}

int sys_read(int fd,userptr_t buf,size_t nbytes,ssize_t *retval){
        //check fd    
    if(fd<0 || fd>31){
        *retval = -1;
        return EBADF;//error 
    }
    int OFT_index;
    OFT_index = curproc->fd_table[fd];
    OFT_index--;//since number is index+1
    //check index number
    if(OFT_index<0||OFT_index>31){
        *retval = -1;
        return EBADF;
    }    
    //check whether slot is valid or not  
    lock_acquire(OFT_lock);      
    if(of_table[OFT_index].current_vnode==NULL){
        *retval = -1;
        lock_release(OFT_lock);
        return EBADF;
    }

    //check permission
    int test = of_table[OFT_index].flag;
    if(((test>>31)==0)||(of_table[OFT_index].flag & (1<<1))){
    //have permission to write
    // do nothing
    }else{
        // doesn't have permission to write
        *retval = -1;
        lock_release(OFT_lock);
        return EACCES;
    }
    lock_release(OFT_lock);

    //create uio and iovec for write
    struct iovec iov;
    struct uio u;
    enum uio_rw {UIO_READ,UIO_WRITE} rw = UIO_READ; 
    lock_acquire(OFT_lock);
    uio_kinit(&iov,&u,buf,nbytes,of_table[OFT_index].offset,rw);
    size_t old_resid = u.uio_resid;
    int error;
    error=VOP_READ(of_table[OFT_index].current_vnode,&u);
    size_t new_resid = u.uio_resid;
    lock_release(OFT_lock);
    //the difference between old_resid and new_resid is how many bytes were written
    if(error==0){
        *retval = old_resid - new_resid;
        //update offset
        lock_acquire(OFT_lock);
        of_table[OFT_index].offset += *retval; 
        lock_release(OFT_lock);
        return 0;
    }else{
        *retval = -1;
        return error;
    }
    return 0;//success  
}



int sys_dup2(int oldfd,int newfd,int *retval){
    //check input oldfd like sys_close
    if(oldfd<0 || oldfd>31){
        return EBADF;//error 
    }
    int OFT_index;
    OFT_index = curproc->fd_table[oldfd];
    OFT_index--;//since number is index+1
    if(OFT_index<0||OFT_index>31){
        *retval = -1;
        return EBADF;
    } 
    lock_acquire(OFT_lock);
    if(of_table[OFT_index].current_vnode==NULL){
        *retval = -1;
        lock_release(OFT_lock);
        return EBADF;
    }
    lock_release(OFT_lock);
    
    //check newfd
    if(newfd<0 || newfd>31){
        *retval = -1;
        return EBADF;
    }
    //if newFD != 0, it points to some OFT slot, we need to close it
    if(curproc->fd_table[newfd]!=0){
        int error;
        error = sys_close(newfd);
        if(error!=0){ // if sys_close didn't set newFD successfully
            curproc->fd_table[newfd]=0;
        }
    }
    curproc->fd_table[newfd]=OFT_index+1;//give oldfd OFT index number to newfd
    lock_acquire(OFT_lock);
    of_table[OFT_index-1].ref_count++;
    lock_release(OFT_lock);
    //return value     
    *retval = newfd;
    return 0;//successfully implement
}

int sys_lseek(int fd,off_t pos, int whence,off_t *retval){
    //check input
    if(fd<0 || fd>31){
        *retval=-1;
        return EBADF;//error 
    }
    int OFT_index;
    OFT_index = curproc->fd_table[fd];
    OFT_index--;//since number is index+1
    //check index number
    if(OFT_index<0||OFT_index>31){
        *retval=-1;
        return EBADF;
    }    
    //check whether slot is valid or not 
    lock_acquire(OFT_lock);    
    if(of_table[OFT_index].current_vnode==NULL){
        *retval=-1;
        lock_release(OFT_lock);
        return EBADF;
    }
    lock_release(OFT_lock);
    //check whence
    if(whence!=SEEK_SET&&whence!=SEEK_CUR&&whence!=SEEK_END){
        //invalid whence code
        *retval=-1;
        return EINVAL;
    }

    if(whence==SEEK_SET){
        if(pos>=0){
            lock_acquire(OFT_lock);
            of_table[OFT_index].offset=pos;
            lock_release(OFT_lock);
            *retval = pos;
            return 0;
        }else{
            //negative pos
            *retval = -1;
            return EINVAL;
        }

    }
    if(whence==SEEK_CUR){
        lock_acquire(OFT_lock);
        off_t set_cur = of_table[OFT_index].offset + pos;
        if(set_cur>=0){
            of_table[OFT_index].offset = set_cur;
            lock_release(OFT_lock);
            *retval = set_cur;
            return 0;
        }else{
            //negative pos
            *retval = -1;
            lock_release(OFT_lock);
            return EINVAL;
        }
    }
    if(whence==SEEK_END){
        //create a pointer for VOP_STAT
        struct stat ptr;// I don't know what this pointer is for
        lock_acquire(OFT_lock);
        off_t end_of_file = VOP_STAT(of_table[OFT_index].current_vnode,&ptr);
        lock_release(OFT_lock);
        off_t set_end = end_of_file + pos;
        if(set_end>=0){
            lock_acquire(OFT_lock);
            of_table[OFT_index].offset = set_end;
            lock_release(OFT_lock);
            *retval = set_end;
            return 0;
        }else{
            //negative pos
            *retval = -1;
            return EINVAL;
        }

    }
    return 0;//successfully implement
}

