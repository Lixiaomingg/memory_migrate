/**
 * dentry 结构体迁移实验   
 * 搜索目标虚拟机内存空间，迁移特定数量的dentry结构体
 * ffffffff81d16e30 d dentry_hashtable
*/

#include <stdio.h>
#include <stdlib.h>

#include <libvmi/libvmi.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <inttypes.h>

#define MAX_COUNT 256

unsigned long tasks_offset = 0;
unsigned long name_offset = 0;

void get_dentry_addr(vmi_instance_t vmi,addr_t addrs[MAX_COUNT]);
void get_fdt_addr(vmi_instance_t vmi,addr_t addrs[MAX_COUNT]);

int main(int argc,char **argv){
    vmi_instance_t vmi;
    char *name = NULL;
    int count;
    addr_t fdt_addrs[MAX_COUNT]; 
    addr_t dentry_addrs[MAX_COUNT];


    if(argc != 2){
        printf("Usage:%s <vmname>\n",argv[0]);
        return 1;
    }

    name = argv[1];
    
    printf("请输入需要迁移的dentry数量（0／10／20／30／40）\n");
    scanf("%d",&count);

    tasks_offset = vmi_get_offset(vmi,"linux_tasks");
    name_offset = vmi_get_offset(vmi,"linux_name");

    if(vmi_pause_vm(vmi) == VMI_FAILURE){
        printf("failed to pause vm\n");
        goto error_exit;
    }


error_exit:
    vmi_resume_vm(vmi);
    vmi_destroy(vmi);
    return 0;
}

//从目标虚拟机内存中获取指定数量fdt数组首地址
void get_fdt_addr(vmi_instance_t vmi,addr_t addrs[MAX_COUNT]){
    addr_t list_head;
    list_head = vmi_translate_ksym2v(vmi,"init_task");
    list_head += tasks_offset;
}



//从目标虚拟机内存中获取指定数量dentry结构体首地址
void get_dentry_addr(vmi_instance_t vmi, addr_t addrs[MAX_COUNT]){
    

}