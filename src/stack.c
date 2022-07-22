#include <linux/kernel.h>

#include "mwp.h"

/* Overwrite dest with src, we loop MWP_BLOCK_SIZE over the stack, \
  * starting from dest_addr. */
size_t vp_ow(struct mm_struct *mm, u64 dest_addr, const char *dest, const char *src)
{
    int i;
    int ret;
    void *kvaddr;
    bool success = false;
    struct page *p = NULL;

    /* Pin the pages into memory */
    mmap_read_lock(mm);
    ret = get_user_pages_remote(mm, dest_addr, NR_PAGES, FOLL_FORCE, &p, NULL, NULL);
    if (unlikely(ret <= 0))
        return 0;
    mmap_read_unlock(mm);

    kvaddr = kmap(p); /* Map the page(s), and return a kernel space virtual address of the mapping */
    if (kvaddr == NULL)
        return 0;

    /* in the future we must be able to find the exact position without \
        looping over MWP_BLOCK_SIZE addresses, right now we do that because it is difficult \
            to find the right address */
    for (i = 0; i < MWP_BLOCK_SIZE; i++) 
    {
        if (strcmp((char *)kvaddr + i, src) == 0) 
        {
            memcpy(kvaddr + i, dest, strlen(dest));
            success = true;
            break;
        }
    }

    /* No need to check for errors, this is unlikely to fail */
    kunmap(p);
    set_page_dirty(p); /* Mark page as dirty */
    put_page(p); /* Return the page */

    return success;
}

/* Return the exact address of name within the process address space */
u64 vp_fetch_addr(struct mm_struct *mm, struct task_struct *ts, struct vp_sections_struct vps, const char *name)
{
    int i = 0;
    char *kvbuf;
    int fetched = 0;
    
    if (!(kvbuf = kmalloc(BUF_SIZE, GFP_KERNEL)))
        return 0;

    /* Loop until the desired string is found */
    while (fetched == 0) {
        if (i == MWP_BLOCK_SIZE)
            break; /* We reached the limit, can't loop anymore */
        access_process_vm(ts, vps.args.start_args + i, kvbuf, BUF_SIZE, FOLL_FORCE);
        i++;
        if (strcmp(kvbuf, name) == 0)
            fetched = 1; } /* Stop if we found the desired string in memory */
    
    kfree(kvbuf);
    return fetched == 0 ? 0 : vps.args.start_args + i;
}

void incusage(void)
{
    spin_lock(&pinfo.pwlock);
    pinfo.usage_count++;
    spin_unlock(&pinfo.pwlock);
}

void incrdwr(void)
{
    spin_lock(&pinfo.pwlock);
    pinfo.nrdwr++;
    spin_unlock(&pinfo.pwlock);
}