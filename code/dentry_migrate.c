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
int migrate_count;

int fdt_count;

struct fdtable{
    addr_t addr;
    int next_fd;
};

void get_dentry_addr(vmi_instance_t vmi,addr_t addrs[MAX_COUNT]);
status_t get_fdt_addr(vmi_instance_t vmi,struct fdtable addrs[MAX_COUNT]);

int main(int argc,char **argv){
    vmi_instance_t vmi;
    char *name = NULL;
    struct fdtable fdt_addrs[MAX_COUNT]; 
    addr_t dentry_addrs[MAX_COUNT];
    int i;

    if(argc != 2){
        printf("Usage:%s <vmname>\n",argv[0]);
        return 1;
    }

    name = argv[1];
    
    printf("请输入需要迁移的dentry数量（0／10／20／30／40）\n");
    scanf("%d",&migrate_count);
 
    if(VMI_FAILURE == vmi_init(&vmi,VMI_AUTO | VMI_INIT_COMPLETE,name)){
        printf("初始化library失败\n");
        return 1;
    }
    
    tasks_offset = vmi_get_offset(vmi,"linux_tasks");
    name_offset = vmi_get_offset(vmi,"linux_name");
  
    if(vmi_pause_vm(vmi) == VMI_FAILURE){
        printf("failed to pause vm\n");
        goto error_exit;
    }

    //获取所有的fdt信息
    if(VMI_FAILURE == get_fdt_addr(vmi,fdt_addrs)){
        printf("failed to get fdt address\n");
        goto error_exit;
    }

    for(i = 0;i<fdt_count;i++){
        printf("%lx %d\n",fdt_addrs[i].addr,fdt_addrs[i].next_fd);
    }




error_exit:
    vmi_resume_vm(vmi);
    vmi_destroy(vmi);
    return 0;
}

//从目标虚拟机内存中获取指定数量fdt数组首地址
status_t get_fdt_addr(vmi_instance_t vmi,struct fdtable addrs[MAX_COUNT]){
    addr_t list_head;
    addr_t current_list_entry = list_head;
    addr_t next_list_entry;
    addr_t current_process;
    addr_t files;
    addr_t fdtable_struct;
    addr_t fdt;
    fdt_count = 0;

    list_head = vmi_translate_ksym2v(vmi,"init_task");
    list_head += tasks_offset;
    
    if(VMI_FAILURE == vmi_read_addr_va(vmi,current_list_entry,0,&next_list_entry)){
        printf("failed to read next pointer in loop at %lx\n",current_list_entry);
        return VMI_FAILURE; 
    }
    
    while(1){
        current_process = current_list_entry - tasks_offset;
        if(VMI_FAILURE == vmi_read_addr_va(vmi,current_process+0x590,0,&files)){
            printf("faild to read files address\n");
            return VMI_FAILURE;
        }

        if(VMI_FAILURE == vmi_read_addr_va(vmi,files+0x8,0,&fdtable_struct)){
            printf("faild to read fdtable_struct address\n");
            return VMI_FAILURE;
        }

        //获取next_fd
        uint32_t temp;
        if(VMI_FAILURE == vmi_read_32_va(vmi,files+0x44,0,&temp)){
            printf("faild to read next_fd\n");
            return VMI_FAILURE;
        }else{
            addrs[fdt_count].next_fd = temp;
        }

        //获取fdt数组首地址
        if(VMI_FAILURE == vmi_read_addr_va(vmi,fdtable_struct+0x8,0,&fdt)){
            printf("faild to read fdt\n");
            return VMI_FAILURE;
        }else{
            addrs[fdt_count].addr = fdt;
            fdt_count++;
        }

        current_list_entry = next_list_entry;
        if(VMI_FAILURE == vmi_read_addr_va(vmi,current_list_entry,0,&next_list_entry)){
            printf("faild to read next pointer in loop %lx\n",current_list_entry);
            return VMI_FAILURE;
        }
        if(current_list_entry == list_head) break;

    }
    return VMI_SUCCESS;
}



//从目标虚拟机内存中获取指定数量dentry结构体首地址
void get_dentry_addr(vmi_instance_t vmi, addr_t addrs[MAX_COUNT]){
    

}