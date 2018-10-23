// 通过读取文件，获取目标结构体地址，
// 然后对一定数量的结构体目标所在内存页进行监控 

#include <stdio.h>
#include <stdlib.h>

#include <libvmi/libvmi.h>
#include <libvmi/events.h>
#include <string.h>
#include <error.h>
#include <sys/mman.h>
#include <inttypes.h>
#include <signal.h>

#define PAGE_SIZE 1 << 12

//定义最大监控数量
#define MAX_EVENT_NUM 400
//定义读取文件路径
#define DENTRY_FILE "data/dentry_addrs.txt"
#define FDT_FILE "data/fdt_addrs.txt"

vmi_event_t mem_event[MAX_EVENT_NUM];
addr_t mem_addrs[MAX_EVENT_NUM];

int event_count;

void print_event(vmi_event_t *event)
{
    printf("\tPAGE ACCESS: %c%c%c for GFN %"PRIx64" (offset %06"PRIx64") gla %016"PRIx64" (vcpu %u)\n",
           (event->mem_event.out_access & VMI_MEMACCESS_R) ? 'r' : '-',
           (event->mem_event.out_access & VMI_MEMACCESS_W) ? 'w' : '-',
           (event->mem_event.out_access & VMI_MEMACCESS_X) ? 'x' : '-',
           event->mem_event.gfn,
           event->mem_event.offset,
           event->mem_event.gla,
           event->vcpu_id
          );
}

event_response_t step_callback(vmi_instance_t vmi, vmi_event_t *event){
    printf("Re-registering event\n");
    vmi_register_event(vmi,event);
    return 0;
}

event_response_t mem_event_callback(vmi_instance_t vmi, vmi_event_t *event){
    print_event(event);
    vmi_clear_event(vmi,event,NULL);
    vmi_step_event(vmi,event,event->vcpu_id,1,step_callback);
}

static int interrupted = 0;
static void close_handler(int sig){
    interrupted = sig;
}



int main(int argc,char **argv){
    vmi_instance_t vmi;
    status_t status;
    struct sigaction act;
    int i,choice,count;
    FILE *fp = NULL;
    char *name = NULL;

    addr_t paddr;
    unsigned long page_num;


    if(argc < 2){
        printf("Usage:monitor <name of VM>\n");
        exit(1);
    }

    //获取虚拟机名称
    name = argv[1];
    act.sa_handler = close_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction(SIGHUP,&act,NULL);
    sigaction(SIGTERM,&act,NULL);
    sigaction(SIGINT,&act,NULL);
    sigaction(SIGALRM,&act,NULL);

    //初始化libvmi库文件
    if(VMI_FAILURE ==
            vmi_init_complete(&vmi,name,VMI_INIT_DOMAINNAME | VMI_INIT_EVENTS,
                            NULL,VMI_CONFIG_GLOBAL_FILE_ENTRY,NULL,NULL)){
        printf("libvmi库文件初始化失败\n");                            
        if(vmi != NULL) vmi_destroy(vmi);
        return 1;
    }else{
        printf("libvmi库文件初始化成功\n");
    }

    //从文件中读取目标结构体地址信息
    printf("请选择监控目标:\n");
    printf("1.dentry\n");
    printf("2.fdt\n");
    printf("0.exit\n");
    scanf("%d",&choice);
    switch(choice){
        case 1:
            fp = fopen(DENTRY_FILE,"r");
            break;
        case 2:
            fp = fopen(FDT_FILE,"r");
            break;
        default:
            exit(1);
    }
    if(fp){
        fscanf(fp,"%d",&count);
        count = (count < MAX_EVENT_NUM) ? count : MAX_EVENT_NUM;
        for(i=0;i<count;i++){
            fscanf(fp,"%lx",&mem_addrs[i]);
        }
    }else{
        printf("open file failed\n");
    }

    //生成内存页监听事件
    for(i = 0;i<count;i++){
        vmi_translate_kv2p(vmi,mem_addrs[i],&paddr);
        page_num = paddr >> 12;
        memset(&mem_event[i],0,sizeof(vmi_event_t));
        mem_event[i].version = VMI_EVENTS_VERSION;
        mem_event[i].type = VMI_EVENT_MEMORY;
        mem_event[i].callback = mem_event_callback;

        mem_event[i].mem_event.gfn = page_num;
        mem_event[i].mem_event.in_access = VMI_MEMACCESS_RW;

        //注册内存监听事件
        vmi_register_event(vmi,&mem_event[i]);
    }

    //开始监听目标虚拟机
    while(!interrupted){
        printf("等待事件触发...\n");
        status = vmi_events_listen(vmi,500);
        if(status != VMI_SUCCESS){
            printf("等待事件时出现未知错误，退出...");
            interrupted = -1;
        }
    }

    printf("监听任务完成！");


    vmi_destroy(vmi);
    return 0;
}