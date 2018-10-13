#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>

char * vaddr;
int init_hello_module(void)
{
    char tmp[192];
    printk("the memory start addr is %p\n",&tmp[0]);
    printk("the memory end addr is %p\n",&tmp[191]);
    vaddr = vmalloc(1024);
    if(!vaddr){
    	printk("lihuhua:failed to vmalloc\n");
    }else{
    	printk("lihuhua:the vmalloc addr is %p\n",vaddr);
    }
    return 0;

}

void exit_hello_module(void){
    if(vaddr) vfree(vaddr);
    printk("lihuhua:rm get_memory mod success!\n");   
}

MODULE_LICENSE("GPL");
module_init(init_hello_module);
module_exit(exit_hello_module);
