/*
 * Declarations for file handle and file table management.
 */
#include <vfs.h>
#include <vnode.h>
#include <synch.h>
#include <types.h>
struct openFileSlot{
    int number;//# of current slot
    int ref_count;
    off_t offset;//current unknown
    int flag;
    struct vnode *current_vnode;// pointer to a vnode  
};
//declare OFT at file.h then I can use it with extern key word at other files like main.c
// OFT will be initialized in main.c boot function
struct openFileSlot *of_table;

struct lock *OFT_lock;

#ifndef _FILE_H_
#define _FILE_H_

/*
 * Contains some file-related maximum length constants
 */
#include <limits.h>


/*
 * Put your function declarations and data types here ...
 */


// oft_index gives index number of open file table to FD table's slot
int sys_open(const char *filename,int flags,mode_t mode,int *retval);
int sys_close(int fd);
int sys_write(int fd,userptr_t buf,size_t nbytes,ssize_t *retval);
int sys_read(int fd,userptr_t buf,size_t nbytes,ssize_t *retval);
int sys_dup2(int oldfd,int newfd,int *retval);
int sys_lseek(int fd,off_t pos,int whence, off_t *retval);
#endif /* _FILE_H_ */

