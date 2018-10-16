
// 对目标虚拟机特定内存页进行监控
// 输入一个起始的线性地址，转换成物理地址后，
// 对该物理地址开始的32张内存页进行读写操作监控
// 监控大小总计32＊4096 字节



#include <stdio.h>
#include <stdlib.h>

#include <libvmi/libvmi.h>
#include <string.h>
#include <error.h>
#include <sys/mman.h>
#include <inttypes.h>
#include <signal.h>

#define PAGE_SIZE 1 << 12
//监控内存页大小
#define PAGE_NUM  
//kmalloc()分配大小最大32内存页
#define MAX_PAGE_NUM 32


addr_t mem_vaddr;
addr_t mem_paddr;
vmi_event_t mem_event;

void mem_event_callback(vmi_instance_t vmi, vmi_event_t *event){
    printf("mem_event callback\n");
    vmi_clear_event(vmi,&mem_event);
}

static int interrupted = 0;
static void close_handler(int sig){
    interrupted = sig;
}

int main(int argc,char **argv){
    vmi_instance_t vmi = NULL;
    status_t status = VMI_SUCCESS;
    struct sigaction act;

    char *name = NULL;
    vmi_pid_t pid = -1;
    if(argc < 2){
        printf("Usage: monitor <name of VM>\n");
        exit(1);
    }
    //获取虚拟机名称
    name  = argv[1];
    //获取pid

    
    act.sa_handler = close_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction(SIGHUP, &act, NULL);
    sigaction(SIGTERM,&act,NULL);
    sigaction(SIGINT,&act,NULL);
    sigaction(SIGALRM,&act,NULL);

    //初始化libvmi库
    if(vmi_init(&vmi, VMI_XEN | VMI_INIT_COMPLETE | VMI_INIT_EVENTS , name) == VMI_FAILURE){
        printf("libvmi 库文件初始化失败!\n");
        if(vmi != NULL){
            vmi_destroy(vmi);
        }
        return 1;
    }else {
        printf("libvmi 库文件初始化成功!\n");
    }

    //获取监控内存页起始地址
    printf("请输入监控内存地址（线性地址）:\n");
    scanf("%lx",&mem_vaddr);
    printf("输入的线性地址为：%lx\n",mem_vaddr);

    mem_paddr = vmi_translate_kv2p(vmi,mem_vaddr);
    printf("线性地址转换后的物理地址为：%lx",mem_paddr);

    //生成内存页监听事件
    memset(&mem_event,0,sizeof(vmi_event_t));
    mem_event.type = VMI_EVENT_MEMORY;
    mem_event.callback = mem_event_callback;


    mem_event.mem_event.physical_address = mem_paddr;
    mem_event.mem_event.npages = MAX_PAGE_NUM;
    mem_event.mem_event.granularity = VMI_MEMEVENT_PAGE;
    mem_event.mem_event.in_access = VMI_MEMACCESS_RW;

    //注册监听事件
    vmi_register_event(vmi,&mem_event);

    //开始监听目标虚拟机
    while(!interrupted){
        printf("等待事件触发...\n");
        status = vmi_events_listen(vmi,500);
        if(status != VMI_SUCCESS){
            printf("等待事件时出现未知错误，退出.....");
            interrupted = -1;
        }
    }
    
    printf("监听任务完成！\n");

    //数据结构清理，程序退出
    vmi_destroy(vmi);
    return 0;
   
}