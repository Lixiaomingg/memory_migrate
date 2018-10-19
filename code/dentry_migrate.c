/**
 * dentry 结构体迁移实验   
 * 搜索目标虚拟机内存空间，迁移特定数量的dentry结构体
 * ffff8800021c0000
*/

#include <stdio.h>
#include <stdlib.h>

#include <libvmi/libvmi.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <inttypes.h>
#include <stdbool.h>

//128KB/512B
#define MAX_FDT_COUNT 256
#define FDT_BYTE 512

//128KB/192B
#define MAX_DENTRY_COUNT 682
#define DENTRY_BYTE 192

unsigned long tasks_offset = 0;
unsigned long name_offset = 0;

//迁移数量和迁移目标地址
int migrate_count;
addr_t migrate_addr;

//实际检测到fdt和dentry数量
int fdt_count;
int dentry_count;

struct fdtable{
    addr_t pointer_addr;
    addr_t addr;
    int next_fd;
};

//搜寻待迁移的dentry结构体和fdt结构体
status_t get_dentry_addr(vmi_instance_t vmi,struct fdtable fdt_addrs[MAX_FDT_COUNT],addr_t dentry_addrs[MAX_DENTRY_COUNT]);
status_t get_fdt_addr(vmi_instance_t vmi,struct fdtable addrs[MAX_FDT_COUNT]);

//针对dentry结构体和fdt进行迁移
status_t migrate_dentry(vmi_instance_t vmi,addr_t dentry_addrs[MAX_DENTRY_COUNT]);
status_t migrate_fdt(vmi_instance_t vmi,struct fdtable fdt_addrs[MAX_FDT_COUNT]);

status_t memory_copy(vmi_instance_t vmi,addr_t old_addr,addr_t new_addr,int size);
status_t check_pointer(vmi_instance_t vmi,addr_t current_addr,uint64_t value,addr_t dentry_addrs[MAX_DENTRY_COUNT],int max_count);

int modify_count = 0;

int main(int argc,char **argv){
    vmi_instance_t vmi;
    char *name = NULL;
    struct fdtable fdt_addrs[MAX_FDT_COUNT]; 
    addr_t dentry_addrs[MAX_DENTRY_COUNT];
    int i,choice;

    if(argc != 2){
        printf("Usage:%s <vmname>\n",argv[0]);
        return 1;
    }

    name = argv[1];
    
    

    if(VMI_FAILURE ==
            vmi_init_complete(&vmi,name,VMI_INIT_DOMAINNAME,NULL,
                                VMI_CONFIG_GLOBAL_FILE_ENTRY,NULL,NULL)){
        printf("初始化library失败\n");
        return 1;
    }
    
    if(VMI_FAILURE == vmi_get_offset(vmi,"linux_tasks",&tasks_offset)){
        printf("获取linux_tasks偏移出错\n");
        goto error_exit;
    }

    if(VMI_FAILURE == vmi_get_offset(vmi,"linux_name",&name_offset)){
        printf("获取linux_name偏移出错\n");
        goto error_exit;
    }
  
    if(vmi_pause_vm(vmi) == VMI_FAILURE){
        printf("failed to pause vm\n");
        goto error_exit;
    }

    //获取所有的fdt信息
    if(VMI_FAILURE == get_fdt_addr(vmi,fdt_addrs)){
        printf("failed to get fdt address\n");
        goto error_exit;
    }
    printf("search task of fdt is finished ,the fdt_count is %d\n",fdt_count);

    if(VMI_FAILURE == get_dentry_addr(vmi,fdt_addrs,dentry_addrs)){
        printf("failed to get dentry address\n");
        goto error_exit;
    }
    printf("search task of dentry is finished ,the dentry_count is %d\n",dentry_count);   
    printf("1.dentry migrate\n");
    printf("2.fdt migrate\n");
    printf("3.exit\n"); 
    scanf("%d",&choice);

    printf("please input migrate start addr:\n");
    scanf("%lx",&migrate_addr);
    switch(choice){
        case 1:
            if(VMI_FAILURE == migrate_dentry(vmi,dentry_addrs)){
                printf("migrate dentry failure!\n");
            }else{
                printf("migrate dentry success\n");
            }
            break;
        case 2:
            if(VMI_FAILURE == migrate_fdt(vmi,fdt_addrs)){
                printf("migrate fdt failure\n");
            }else{
                printf("migrate fdt success\n");
            }
            break;
        default:
            break;
    }



    

error_exit:
    vmi_resume_vm(vmi);
    vmi_destroy(vmi);
    return 0;
}

//从目标虚拟机内存中获取指定数量fdt数组首地址
status_t get_fdt_addr(vmi_instance_t vmi,struct fdtable addrs[MAX_FDT_COUNT]){
    addr_t list_head;
    addr_t current_list_entry = list_head;
    addr_t next_list_entry;
    addr_t current_process;
    addr_t files;
    addr_t fdt;
    fdt_count = 0;

    if(VMI_FAILURE == vmi_translate_ksym2v(vmi,"init_task",&list_head)){
        printf("获取init_task地址出错\n");
        return VMI_FAILURE;

    }
    list_head += tasks_offset;
    current_list_entry = list_head;

    if(VMI_FAILURE == vmi_read_addr_va(vmi,current_list_entry,0,&next_list_entry)){
        printf("failed to read next pointer in loop at %lx\n",current_list_entry);
        return VMI_FAILURE; 
    }

    while(1){
        current_process = current_list_entry - tasks_offset;
        if(VMI_FAILURE == vmi_read_addr_va(vmi,current_process+0x590,0,&files)){
            printf("faild to read files address\n");
            goto update_current_entry;
        }

        //获取next_fd
        uint32_t temp;
        if(VMI_FAILURE == vmi_read_32_va(vmi,files+0x44,0,&temp)){
            printf("faild to read next_fd\n");
            goto update_current_entry;
        }

        //获取fdt数组首地址
        if(VMI_FAILURE == vmi_read_addr_va(vmi,files+0x18,0,&fdt)){
            printf("faild to read fdt\n");
            goto update_current_entry;
        }else{
            if(temp){
                addrs[fdt_count].pointer_addr = files+0x18;
                addrs[fdt_count].next_fd = temp;
                addrs[fdt_count].addr = fdt;
                fdt_count++;
            }else {
                goto update_current_entry;
            }
        }

update_current_entry:
        current_list_entry = next_list_entry;
        if(VMI_FAILURE == vmi_read_addr_va(vmi,current_list_entry,0,&next_list_entry)){
            printf("faild to read next pointer in loop %lx\n",current_list_entry);
            return VMI_FAILURE;
        }

        //增加跳出循环条件
        if(current_list_entry == list_head) break;
        if(fdt_count == MAX_FDT_COUNT) break;

    }
    return VMI_SUCCESS;
}



//从目标虚拟机内存中获取指定数量dentry结构体首地址
status_t get_dentry_addr(vmi_instance_t vmi, struct fdtable fdt_addrs[MAX_FDT_COUNT],  addr_t dentry_addrs[MAX_DENTRY_COUNT]){
    int i,j,k;
    addr_t file;
    addr_t dentry;

    dentry_count = 0;
    bool flag = true;

    for(i=0;i<fdt_count;i++){
        for(j=0;j< fdt_addrs[i].next_fd;j++){
            if(VMI_SUCCESS == vmi_read_addr_va(vmi,fdt_addrs[i].addr + 0x8*j,0,&file)){
                if(file){
                    //读取dentry地址
                    if(VMI_SUCCESS == vmi_read_addr_va(vmi,file+0x18,0,&dentry)){
                        //判断dentry有无重复
                        for(k=0;k<dentry_count;k++){
                            if(dentry == dentry_addrs[k]){
                                flag = false;
                                break;
                            }
                        }

                        //无重复，则添加之dentry数组中
                        dentry_addrs[dentry_count] = dentry;
                        dentry_count++;
                        
                        //判断数组是否已满
                        if(dentry_count == MAX_DENTRY_COUNT) return VMI_SUCCESS;
                    }
                }
            }
        }
    }

    return VMI_SUCCESS;

}


status_t migrate_dentry(vmi_instance_t vmi,addr_t dentry_addrs[MAX_DENTRY_COUNT]){
    int max_count,i;

    printf("please input migrate count(<682):\n");
    scanf("%d",&migrate_count);
    
    //内存拷贝
    max_count = (migrate_count < dentry_count) ? migrate_count : dentry_count;
    for(i = 0;i<max_count;i++){
        if(VMI_FAILURE == memory_copy(vmi,dentry_addrs[i],migrate_addr+DENTRY_BYTE*i,DENTRY_BYTE)){
            printf("migrate dentry failure\n");
            return VMI_FAILURE;
        }
    }
    printf("dentry memory copy all is finished\n");

    //修改相关指针
    addr_t current_addr = 0;
    addr_t end_addr = vmi_get_memsize(vmi);
    uint64_t value;

    while(current_addr < end_addr){
        vmi_read_64_pa(vmi,current_addr,&value);
        if(VMI_FAILURE == check_pointer(vmi,current_addr,value,dentry_addrs,max_count)){
            printf("modify pointer failure\n");
            return VMI_FAILURE;
        }
        current_addr += 8;
    }
    printf("modify pointer success,the count is %d\n",modify_count);

    return VMI_SUCCESS;
}


status_t migrate_fdt(vmi_instance_t vmi,struct fdtable fdt_addrs[MAX_FDT_COUNT]){
    int i,max_count;
    uint64_t value;
    printf("please input migrate count(<256):\n");
    scanf("%d",&migrate_count);

    //fdt内存迁移
    max_count = (migrate_count < fdt_count) ? migrate_count : fdt_count; 
    for(i = fdt_count-max_count;i<max_count;i++){
        if(VMI_FAILURE == memory_copy(vmi,fdt_addrs[i].addr,migrate_addr+FDT_BYTE*i,FDT_BYTE)){
            printf("memory copy failure\n");
            return VMI_FAILURE;
        }
    }
    printf("fdt memory copy all is finished\n");

    //修改相关指针
    for(i = fdt_count-max_count;i<max_count;i++){
        value = migrate_addr+FDT_BYTE*i;
        if(VMI_FAILURE == vmi_write_64_va(vmi,fdt_addrs[i].pointer_addr,0,&value)){
            printf("modify pointer failure\n");
            return VMI_FAILURE;
        }
    }
    printf("modify pointer success,the count is :%d\n",max_count);
    return VMI_SUCCESS;
}

//内存拷贝
status_t memory_copy(vmi_instance_t vmi,addr_t old_addr,addr_t new_addr,int size){
    int count;
    uint64_t value;
    addr_t current_read_addr = old_addr;
    addr_t current_write_addr = new_addr;
    while(count < size){
        if(VMI_FAILURE == vmi_read_64_va(vmi,current_read_addr,0,&value)){
            printf("failed to read from the old addr\n");
            return VMI_FAILURE;
        }
        if(VMI_FAILURE == vmi_write_64_va(vmi,current_write_addr,0,&value)){
            printf("faild to write to the new addr\n");
            return VMI_FAILURE;
        }
        current_read_addr +=8;
        current_write_addr +=8;
        count += 8 ;
    }
    return VMI_SUCCESS;
}

//检查相关内容是否为需要修改的指针
status_t check_pointer(vmi_instance_t vmi,addr_t current_addr,uint64_t value,addr_t dentry_addrs[MAX_DENTRY_COUNT],int max_count){
    int i;
    uint64_t offset;
    for(i = 0;i<max_count;i++){
        if(value >= dentry_addrs[i] && value < (dentry_addrs[i] + DENTRY_BYTE)){
            offset = value - dentry_addrs[i];
            value = migrate_addr+DENTRY_BYTE*i + offset;
            if(VMI_FAILURE == vmi_write_64_pa(vmi,current_addr,&value)){
                return VMI_FAILURE;
            }
            modify_count++;
            return VMI_SUCCESS;
        }
    }
    return VMI_SUCCESS;
}