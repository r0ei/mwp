#ifndef _NOARGV_H
#define _NOARGV_H

#include <linux/kernel.h>

#define NR_PAGES        (1)
#define BLOCK_SIZE_X    (1024) /* Number of bytes to be written */
#define BUF_SIZE        (256) /* IDK why */

/* 
   * A struct with 64bit fields that holds the process address sections addresses \
   * such as args, .text, .data and env variables.. \ 
   * We can also fetch the stack and brk (heap) but not now
*/
struct vp_sections_struct {
    struct args { /* process arguments, located in the stack */
        u64 start_args;
        u64 end_args; } args;
    struct text { /* The code section, read only */
        u64 start_text;
        u64 end_text; } text;
    struct data {
        u64 start_data;
        u64 end_data; } data;
    struct env {
        u64 start_env;
        u64 end_env; } env;
};

#endif /* _NOARGV_H */