#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/pid.h>

static int PID;
module_param(PID, int, 0);

#define NR_PAGES        (1)
#define BLOCK_SIZE_X    (1024) /* Number of bytes to be written */
#define BUF_SIZE        (256) /* IDK why */

/* A struct to store the start address of the process args, and the end address */
struct args {
    u64 start_addr;
    u64 end_addr;
};

/* 
  * Overwrite the arguments, we write BLOCK_SIZE_X to the stack, \
    * starting from args.start_addr (This might not be accurate due to kmap).
  * I know it's a little risky to fill BLOCK_SIZE_X bytes of the stack with Xs.
*/
int exploit(struct mm_struct *mm, struct args args)
{
    int ret;
    void *kvaddr;
    struct page *p = NULL;

    /* Pin the pages into memory */
    mmap_read_lock(mm);
    ret = get_user_pages_remote(mm, args.start_addr, NR_PAGES, FOLL_FORCE | FOLL_WRITE, &p, NULL, NULL);
    if (unlikely(ret <= 0))
        return -EFAULT;
    mmap_read_lock(mm);

    kvaddr = kmap(p); /* Map the page(s), and return a kernel space address of the page */

    pr_info("[I] Writing to address: %p\n", kvaddr);
    memset(kvaddr, 'X', BLOCK_SIZE_X);
    pr_info("[Y] argv has been successfully overwritten!\n");

    /* No need to check for errors, this is unlikely to fail */
    kunmap(p);
    set_page_dirty(p); /* Mark page as dirty */
    put_page(p); /* Return the page */

    return 0;
}

/* Copy the user-space program arguments addresses into our variables */
void psmem(struct mm_struct *mm, u64 *arg_start, u64 *arg_end)
{
    spin_lock(&mm->arg_lock);
    *arg_start = mm->arg_start;
    *arg_end = mm->arg_end;
    spin_unlock(&mm->arg_lock);
}

/* Fetch out argvs out of args.start_addr, IDK why it returns only ./main (in my case) though,
   * maybe because access_process_vm stops when it finds a null character */
int view_values(struct task_struct *ts, struct mm_struct *mm, struct args args)
{
    char *kbuf;
    size_t bytes_read;
    unsigned long length = args.end_addr - args.start_addr;

    if (!(kbuf = kmalloc(BUF_SIZE, GFP_KERNEL)))
        return -EFAULT;

    /* Read length bytes starting from args.start_addr */
    bytes_read = access_process_vm(ts, args.start_addr, kbuf, length, FOLL_FORCE);
    pr_info("[I] Number of bytes read: %zu out of %ld; Data: '%s'\n",
                    bytes_read, length, kbuf);
    kfree(kbuf);

    return 0;
}

static int __init init_mod(void) 
{
    struct args args;
    struct mm_struct *pid_mm;
    struct task_struct *pid_task_struct;
    
    if (PID <= 0 || PID > PID_MAX_LIMIT)
        return -EINVAL;

    pid_task_struct = pid_task(find_vpid(PID), PIDTYPE_PID);
    if (!pid_task_struct)
        return -EFAULT;
    
    pid_mm = pid_task_struct->mm;
    
    psmem(pid_mm, &args.start_addr, &args.end_addr);  
    /* TODO: Add DEBUG option and if (DEBUG == 0), comment it out */
    view_values(pid_task_struct, pid_mm, args);

    if (exploit(pid_mm, args) < 0)
        return -EFAULT;
    return 0;
}

static void __exit exit_mod(void) 
{
    /* Nothing here, we can return error value in `init_mod` to make our life easier. */
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Roi L");

module_init(init_mod);
module_exit(exit_mod);