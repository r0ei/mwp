#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/pid.h>

#include "mwp.h"

char *dest;
char *src;
static int DEBUG;
static int PID;
module_param(PID, int, 0);
module_param(DEBUG, int, 0);
module_param(dest, charp, 0);
module_param(src, charp, 0);
MODULE_PARM_DESC(DEBUG, "DEBUG=1 will log debug information, you can view it with dmesg");
MODULE_PARM_DESC(PID, "The ID of the process you want to mess with.");
MODULE_PARM_DESC(dest, "The string that should be written to src");
MODULE_PARM_DESC(src, "The string in memory that you want to overwrite");

/* 
  * Overwrite desc with src, we loop BLOCK_SIZE_X over the stack, \
  * starting from dest_addr. */
static int vp_ow(struct mm_struct *mm, u64 dest_addr, const char *dest, const char *src)
{
    int i;
    int ret;
    void *kvaddr;
    struct page *p = NULL;

    /* Pin the pages into memory */
    mmap_read_lock(mm);
    ret = get_user_pages_remote(mm, dest_addr, NR_PAGES, FOLL_FORCE | FOLL_WRITE, &p, NULL, NULL);
    if (unlikely(ret <= 0))
        return -EFAULT;
    mmap_read_unlock(mm);

    kvaddr = kmap(p); /* Map the page(s), and return a kernel space virtual address of the mapping */

    if (DEBUG)
        pr_info("[I] Writing to kernel space mapped address: %p\n", kvaddr);
    
    for (i = 0; i < BLOCK_SIZE_X; i++)
        if (strcmp((char *)kvaddr + i, dest) == 0)
            memcpy(kvaddr + i, src, strlen(src));

    if (DEBUG)
        pr_info("[Y] Argument %s has been successfully overwritten to %s!\n", dest, src);

    /* No need to check for errors, this is unlikely to fail */
    kunmap(p);
    set_page_dirty(p); /* Mark page as dirty */
    put_page(p); /* Return the page */

    return 0;
}

/* Return the exact address of name within the process address space */
static u64 vp_fetch_addr(struct mm_struct *mm, struct task_struct *ts, struct vp_sections_struct vps, const char *name)
{
    int i = 0;
    char *kvbuf;
    int fetched = 0;
    unsigned long slength = vps.args.end_args - vps.args.start_args;
    
    if (!(kvbuf = kmalloc(BUF_SIZE, GFP_KERNEL)))
        return -EFAULT;

    /* Loop until the desired string is found */
    while (fetched == 0) {
        if (i == BLOCK_SIZE_X)
            break; /* We reached the limit, can't loop anymore */
        access_process_vm(ts, vps.args.start_args + i, kvbuf, slength, FOLL_FORCE);
        i++;
        if (strcmp(kvbuf, name) == 0)
            fetched = 1; } /* Stop if we found the desired string in memory */

    kfree(kvbuf);

    if (fetched == 0)
        return -EAGAIN;
    return vps.args.start_args + i;
}

/* Copy the user-space program arguments addresses into our variables */
static void vp_copy(struct mm_struct *mm, struct vp_sections_struct *vps)
{
    spin_lock(&mm->arg_lock);
    vps->args.start_args = mm->arg_start;
    vps->args.end_args = mm->arg_end;
    vps->data.start_data = mm->start_data;
    vps->data.end_data = mm->end_data;
    vps->text.start_text = mm->start_code;
    vps->text.end_text = mm->end_code;
    vps->env.start_env = mm->env_start;
    vps->env.end_env = mm->env_end;
    spin_unlock(&mm->arg_lock);
}

static int __init init_mod(void) 
{
    u64 vaddr;
    struct vp_sections_struct vps;
    struct mm_struct *pid_mm;
    struct task_struct *pid_task_struct;
    
    if (PID <= 0 || PID > PID_MAX_LIMIT)
        return -EINVAL;
    if (!src || !dest)
        return -EINVAL;
    if (!(pid_task_struct = pid_task(find_vpid(PID), PIDTYPE_PID)))
        return -EINVAL;
    
    pid_mm = pid_task_struct->mm;
    vp_copy(pid_mm, &vps);

    if ((vaddr = vp_fetch_addr(pid_mm, pid_task_struct, vps, src)) < 0)
        return -EFAULT;
    if (vp_ow(pid_mm, vaddr, src, dest) < 0) /* Finally, overwrite the memory */
        return -EFAULT;
    return 0;
}

static void __exit exit_mod(void) 
{
    /* Nothing here, we can return error value in `init_mod` to make our life easier. */
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Roi L");
MODULE_VERSION("1.0.1");

module_init(init_mod);
module_exit(exit_mod);
