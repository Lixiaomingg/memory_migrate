//内存迁移实验 for Guest OS of Linux
//dentry 结构体迁移成功，name和refcount 经验证有效。 有时会迁移失败，文件操作失败，且程序崩溃。（跟迁移新地址有关）
//inode 结构体迁移不成功。一旦迁移，虚拟机崩溃
//file 结构体迁移成功


#include <stdio.h>
#include <libvmi/libvmi.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <inttypes.h>
#include <stdlib.h>

#define Maxlen 10

unsigned long tasks_offset = 0;
unsigned long name_offset = 0;

//从进程结构体出发寻找指定文件文件描述符
unsigned long files_offset = 0x590;
unsigned long fdtable_offset = 0x8;
unsigned long fd_offset = 0x8;

//从文件描述符file出发寻找inode结构体
unsigned long inode_offset = 0x20;

//从文件描述符file出发寻找dentry结构体
unsigned long dentry_offset = 0x18;

//dentry_struct结构体内变量偏移
unsigned long dname_offset = 0x20;
unsigned long refcount_offset = 0x5c;

//inode_struct 结构体成员变量偏移
unsigned long iwritecount_offset = 0x138;

addr_t findprocess(vmi_instance_t vmi,addr_t list_head,char *procname);
addr_t finddentry(vmi_instance_t vmi,addr_t file);
addr_t findfd(vmi_instance_t vmi,addr_t process,int index);
addr_t findinode(vmi_instance_t vmi,addr_t file);
int findpointer(vmi_instance_t vmi,addr_t pointer,int size);

int findinsidepointer(vmi_instance_t vmi,addr_t pointer ,int size);

int memory_migrate(vmi_instance_t vmi,addr_t old_addr ,addr_t new_addr , int size);

void modifyinodecache(vmi_instance_t vmi,addr_t pointer);

void modifystruct(vmi_instance_t vmi,addr_t pointer);

void inode_migrate(vmi_instance_t vmi,addr_t inode, addr_t new_addr);

int modifypointer(vmi_instance_t vmi,addr_t old_addr , addr_t new_addr , int offset);

int modifypointer2(vmi_instance_t vmi,addr_t old_addr, addr_t new_addr ,int offset);

void copymemory(vmi_instance_t vmi,addr_t old_addr,addr_t new_addr,int size);


int main(int argc,char **argv){	
	vmi_instance_t vmi;
	addr_t list_head = 0;
	addr_t mainprocess = 0;
	addr_t file = 0, inode = 0, dentry = 0;
	status_t status;
	char procname[Maxlen];

	if(argc != 2){
		printf("Usage : %s <vmname>\n",argv[0]);
		return 1;
	}
	char *name = argv[1];
	printf("input the new_addr :\n");
	addr_t new_addr = 0;
	scanf("%lx",&new_addr);
	printf("the input new_addr is %lx \n",new_addr);
	if(VMI_FAILURE ==vmi_init_complete(&vmi,name,VMI_INIT_DOMAINNAME,NULL,VMI_CONFIG_GLOBAL_FILE_ENTRY,NULL,NULL)){
		printf("failed to init the library \n");
		return 1;
	}

	if(VMI_FAILURE == vmi_get_offset(vmi,"linux_tasks",&tasks_offset)) goto error_exit;
	if(VMI_FAILURE == vmi_get_offset(vmi,"linux_name",&name_offset)) goto error_exit;
	
	if(vmi_pause_vm(vmi) != VMI_SUCCESS){
		printf("Failed to pause vm\n");
		goto error_exit;
	}

	int choice = -1;
	
	if(VMI_FAILURE == vmi_translate_ksym2v(vmi,"init_task",&list_head)) goto error_exit;
	list_head += tasks_offset;
	
	printf("please input the procname:\n");
	scanf("%s",procname);
	printf("the input procname is %s",procname);

	mainprocess = findprocess(vmi,list_head,procname);
	if(mainprocess == 0) {
		printf("failed to find the process\n");
		goto error_exit;
	}else{
		printf("succeed to find the process,the addr is %lx\n",mainprocess);
	}

	file = findfd(vmi,mainprocess,3);
	if(file == 0){
		printf("faild to find the file_struct\n");
		goto error_exit;
	}else{
		printf("succeed to find the file_struct,the addr is %lx\n",file);
	}

	inode = findinode(vmi,file);
	if(file == 0){
		printf("failed to find the inode_struct\n");
		goto error_exit;
	}else{
		printf("succeed to find the inode_struct,the addr is %lx\n",inode);
	}
	//打印inode结构体相关信息
	uint16_t iwritecount = -1;
	status = vmi_read_16_va(vmi,inode + iwritecount_offset,0,&iwritecount);
	if(status == VMI_FAILURE) {
		printf("failed to read the i_count \n");
	}else{
		printf("the iwritecount is %d\n",iwritecount);
	}
	

	dentry = finddentry(vmi,file);
	if(dentry == 0){
		printf("failed to find the dentry_struct\n");
		goto error_exit;
	}else{
		printf("succeed to find the dentry_struct,the addr is %lx\n",dentry);
	}

	
		
	//打印dentry相关信息
	char *file_name = vmi_read_str_va(vmi,dentry + dname_offset,0);
	printf("the file name is %s\n",file_name);
	uint32_t refcount = -1;
	status = vmi_read_32_va(vmi,dentry+refcount_offset,0,&refcount);
	if(status == VMI_FAILURE){
		printf("failed to read the refconut\n");
	}else{
		printf("the refcount is %d\n",refcount);
	}

	

	
	printf("please choose the function:\n");
	printf("1.dentry migrate\n");
	printf("2.fd(file struct) migrate \n");
	printf("3.inode migrate \n");
	printf("4.find dentry pointer count\n");
	printf("5.find inode pointer count\n");
	printf("6.find inode inside pointer count\n");
	printf("7.find inode pointer in struct\n");

	printf("9.modify the inode cache\n");
	printf("10.modify the struct\n");
	scanf("%d",&choice);
	int result = -1;
	switch(choice){
		case 1:		
			result = memory_migrate(vmi,dentry,new_addr,192);
			if(result == 1) printf("failed to dentry migrate\n");
			break;
		case 2:
			result = memory_migrate(vmi,file,new_addr,216);
			if(result == 1) printf("failed to file migrate\n");
			break;
		case 3:
			//result = memory_migrate(vmi,inode,new_addr,576);
			//if(result == 1) printf("failed to inode migrate\n");
		 	inode_migrate(vmi,inode,new_addr);
			break;
		case 4:
			result = findpointer(vmi,dentry,192);
			printf("the find dentry pointer count is %d\n",result);
			break;
		case 5:
			result = findpointer(vmi,inode,576);
			printf("the find inode pointer count is %d\n",result);
			break;
		case 6:
			result = findinsidepointer(vmi,inode,576);
			printf("the find inode inside pointer count is %d\n",result);
			break;
		
		
		case 9:
			modifyinodecache(vmi,inode);
			break;
		case 10:
			modifystruct(vmi,inode);
		default:
			goto error_exit;
	}

error_exit:
	vmi_resume_vm(vmi);
	vmi_destroy(vmi);
	return 0;
}

addr_t findfd(vmi_instance_t vmi,addr_t process,int index){
	addr_t files_struct = 0 ,fdtable = 0,fd = 0 ,file = 0;
	
	status_t status = vmi_read_addr_va(vmi,process + files_offset,0,&files_struct);
	if(status == VMI_FAILURE) {
		printf("failed to find the files_struct addr\n");
		return 0;
	}
	
	status = vmi_read_addr_va(vmi,files_struct + fdtable_offset,0,&fdtable);
	if(status == VMI_FAILURE) {
		printf("failed to find the fdtable addr \n");
		return 0;
	}

	status = vmi_read_addr_va(vmi,fdtable + fd_offset,0,&fd);
	if(status == VMI_FAILURE) {
		printf("failed to find the fd addr \n");
		return 0;
	}

	status = vmi_read_addr_va(vmi,fd + index*8,0,&file);
	if(status == VMI_FAILURE){
		printf("failed to find the file addr \n");
		return 0;
	}
	return file;
}

addr_t finddentry(vmi_instance_t vmi,addr_t file){
	addr_t dentry = 0;
	status_t status = vmi_read_addr_va(vmi,file+dentry_offset,0,&dentry);
	if(status ==VMI_FAILURE){
		printf("failed to find the dentry addr \n");
		return 0;
	}
	return dentry;
}


addr_t findinode(vmi_instance_t vmi,addr_t file){
	addr_t inode = 0;
	status_t status = vmi_read_addr_va(vmi,file+inode_offset,0,&inode);
	if(status == VMI_FAILURE){
		printf("failed to find the inode addr\n");
		return 0;
	}
	return inode;
}

addr_t findprocess(vmi_instance_t vmi,addr_t list_head,char *procname){
	addr_t current_list_entry = list_head;
	addr_t current_process = 0;
	char *find_procname = NULL;
	addr_t next_list_entry = 0;
	status_t status = vmi_read_addr_va(vmi,current_list_entry,0,&next_list_entry);
	if(status == VMI_FAILURE){
		printf("failed to read next pointer in loop at %lx\n",current_list_entry);
		return 0;
	}
	
	while(1){
		current_process = current_list_entry - tasks_offset;
		find_procname = vmi_read_str_va(vmi,current_process + name_offset,0);

		if(!find_procname){
			printf("failed to find procname\n");
			return 0;
		}else if (strcmp(find_procname,procname) == 0){
			return current_process;
		}
			
		
		current_list_entry = next_list_entry;
		status = vmi_read_addr_va(vmi,current_list_entry,0,&next_list_entry);
		if(status == VMI_FAILURE){
			printf("failed to read next pointer in loop at %lx\n",current_list_entry);
		}
		if(current_list_entry == list_head) break;
	}
	return 0;
}

int memory_migrate(vmi_instance_t vmi,addr_t old_addr ,addr_t new_addr,int size){
	int count = 0;
	uint64_t value;
	status_t status;
	addr_t current_read_addr = old_addr;
	addr_t current_write_addr = new_addr;
	while(count<size){
		vmi_read_64_va(vmi,current_read_addr,0,&value);
		status = vmi_write_64_va(vmi,current_write_addr,0,&value);
		if(status == VMI_FAILURE) {
			printf("failed to write to the new addr \n");
			return 1;
		}
		current_read_addr += 8;
		current_write_addr += 8;
		count += 8;
	}
	printf("copy memory is finished\n");

	//搜索所有指向该结构体指针，并修改指向新的地址
	count = 0;
	addr_t current_addr = 0;
	addr_t end_addr = vmi_get_max_physical_address(vmi);
	uint64_t offset;
	printf("the begin addr is %lx,and the end addr is %lx\n",current_addr,end_addr);

	while(current_addr < end_addr){
		status = vmi_read_64_pa(vmi,current_addr,&value);
		if(value >= old_addr && value < (old_addr+size)){
			offset = value - old_addr;
			printf("the current_addr is %lx,and the value is %lx,the offset is %lx\n",current_addr,value,offset);
			count ++;
			value = new_addr + offset;
			status = vmi_write_64_pa(vmi,current_addr,&value);
			if(status == VMI_FAILURE){
				printf("failed to modify the pointer\n");
				return 1;
			}
		}
		current_addr += 8;
	}	
	printf("the modify pointer count is %d",count);
	return 0;
}

int findpointer(vmi_instance_t vmi,addr_t pointer,int size){
	int count = 0;
	addr_t current_addr = 0;
	addr_t end_addr =  vmi_get_max_physical_address(vmi);
	uint64_t value;
	status_t status;
	while(current_addr < end_addr){
		status = vmi_read_64_pa(vmi,current_addr,&value);
		if(value >= pointer && value <(pointer + size)) {
			printf("succeed to find pointer ,the pa is %lx , the offset is %lx\n",current_addr,value-pointer);
			count++;
		}
		current_addr +=8;
	}
	return count;
}

int findinsidepointer(vmi_instance_t vmi,addr_t pointer ,int size){
	int count = 0;
	addr_t current_addr = pointer;
	addr_t end_addr = pointer + size;
	uint64_t value;
	status_t status;
	while(current_addr < end_addr){
		status = vmi_read_64_va(vmi,current_addr,0,&value);
		if(value >= pointer && value <(pointer + size)){
			printf("from %lx to %lx\n",current_addr - pointer,value - pointer);
			count++;
		}
		current_addr +=8;
	}
	return count;
}





void modifyinodecache(vmi_instance_t vmi,addr_t pointer){    
	addr_t current_addr = 0;
	addr_t end_addr = vmi_get_max_physical_address(vmi);
	addr_t inode_cache_pa = 0;
	status_t status;
	uint64_t value;
	while(current_addr < end_addr){
		status = vmi_read_64_pa(vmi,current_addr,&value);
		if(value == pointer){
			uint64_t tmp1,tmp2;
			vmi_read_64_pa(vmi,current_addr-0x10,&tmp1);
			vmi_read_64_pa(vmi,current_addr-0x8,&tmp2);
			if(tmp1 == tmp2 && (tmp1 & 0xffffffff) == (current_addr - 0x10)){
				printf("succeed to find the inode cache addr,the addr is %lx\n",current_addr);
				inode_cache_pa = current_addr;
				break;
			}
		}
		current_addr+=8;
	}
	value = 0;
	if(inode_cache_pa != 0){
		status = vmi_write_64_pa(vmi,inode_cache_pa,&value);
		if(status == VMI_SUCCESS) printf("succeed to modify the inode cache content\n");
	}else{
		printf("can not find the inode cache content\n");
	}
}

void modifystruct(vmi_instance_t vmi,addr_t pointer){
	addr_t current_addr = 0;
	addr_t end_addr = vmi_get_max_physical_address(vmi);
	status_t status;
	uint64_t value;
	addr_t startpa = 0;
	while(current_addr < end_addr){
		status = vmi_read_64_pa(vmi,current_addr,&value);
		if(value == pointer){
			uint64_t tmp1,tmp2,tmp3;
			vmi_read_64_pa(vmi,current_addr+0x8,&tmp1);
			vmi_read_64_pa(vmi,current_addr+0x90,&tmp2);
			vmi_read_64_pa(vmi,current_addr+0x1e0,&tmp3);
			if(tmp1 == tmp2 && tmp2 == tmp3 && tmp1 == value){
				printf("succeed to find the startpa ,the addr is %lx\n",current_addr);
				startpa = current_addr;
				break;
			}
		}
		current_addr +=8;
	}

	value = 0;
	if(startpa != 0){
		vmi_write_64_pa(vmi,startpa,&value);
		value = 0;
		vmi_write_64_pa(vmi,startpa,&value);
		value = 0;
		vmi_write_64_pa(vmi,startpa,&value);
		value = 0;
		vmi_write_64_pa(vmi,startpa,&value);
		printf("succeed to modify the struct\n");
	}else{
		printf("failed to find the startpa\n");
	}
}

void inode_migrate(vmi_instance_t vmi,addr_t inode,addr_t new_addr){

	uint64_t value;
	status_t status;
	addr_t current_read_addr = 0;
	addr_t current_write_addr = 0;
	

	copymemory(vmi,inode,new_addr,576);

	int count = 0;
	int total = 0;
	uint64_t current_next_value = 0;
	uint64_t current_prev_value = 0;
	uint64_t next_prev_value = 0;
	uint64_t prev_next_value = 0;
	
	//修改指向0xb0 mutex.wait_list 指针
	count = modifypointer(vmi,inode,new_addr,0xb0);
	printf("finished to modify the pointer 0xb0,the count is %d\n",count);
	total += count;

	//修改0xd8 i_hash指针
	count = modifypointer(vmi,inode,new_addr,0xd8);
	printf("finished to modify the pointer 0xd8,the count is %d\n",count);
	total += count;

	//修改0xe8 i_wb_list指针
	count = modifypointer(vmi,inode,new_addr,0xe8);
	printf("finished to modify the pointer 0xe8,the count is %d\n",count);
	total += count;

	//修改0xf8 i_lru指针
	count = modifypointer(vmi,inode,new_addr,0xf8);
	printf("finished to modify the pointer 0xf8,the count is %d\n",count);
	total += count;

	//修改0x108 i_sb_list指针
	count = modifypointer(vmi,inode,new_addr,0x108);
	printf("finished to modify the pointer 0x108,the count is %d\n",count);
	total += count;

	//修改0x118 i_dentry 指针
	count = modifypointer2(vmi,inode,new_addr,0x118);
	printf("finished to modify the pointer 0x118,the count is %d\n",count);	
	total += count;

	//修改0x178 address_space.immap_nonlinear
	count = modifypointer(vmi,inode,new_addr,0x178);
	printf("finished to modify the pointer 0x178,the count is %d\n",count);
	total += count;

	//修改0x190 address_space.mutex.wait_list
	count = modifypointer(vmi,inode,new_addr,0x190);
	printf("finished to modify the pointer 0x190,the count is %d\n",count);
	total += count;

	//修改0x1e0 address_space.private_list
	count = modifypointer(vmi,inode,new_addr,0x1e0);
	printf("finished to modify the pointer 0x1e0,the count is %d\n",count);
	total += count;

	//修改0x208 i_devices 指针
	count = modifypointer(vmi,inode,new_addr,0x208);
	printf("finished to modify the pointer 0x208,the count is %d\n",count);
	total += count;

	//修改0x150 i_data 指针
	count = 0;

	addr_t files[100];
	int filecount = 0; 

	//修改内部inode.i_mapping
	value = new_addr + 0x150;
	uint64_t i_mapping  = 0;
	vmi_read_64_va(vmi,new_addr + 0x30,0,&i_mapping);
	if(i_mapping ==  inode + 0x150){
		status = vmi_write_64_va(vmi,new_addr + 0x30,0,&value);
		if(status == VMI_SUCCESS) {
			count++;
			printf("succeed to modify the pointer i_mapping\n");
		}
	}

	//修改所有file.f_imapping指针
	current_read_addr = 0;
	current_write_addr = vmi_get_max_physical_address(vmi);
	while(current_read_addr < current_write_addr){
		vmi_read_64_pa(vmi,current_read_addr,&value);
		if(value == (inode+ 0x150)){
			uint64_t tmp = 0;
			vmi_read_64_pa(vmi,current_read_addr - 0xb0,&tmp);
			if(tmp == inode){
				value = new_addr + 0x150;
				status = vmi_write_64_pa(vmi,current_read_addr,&value); 
				if(status == VMI_SUCCESS) count++;
				
				files[filecount] = current_read_addr - 0xd0;
				filecount++;
			}
		}
		current_read_addr +=8;
	}
	printf("finished to modify all file.f_imapping pointer\n");
	printf("finished to modify the pointer 0x150 , the count is %d\n",count);
	total += count;

	//修改0x0指针
	//修改address_space.host 指针
	count = 0;
	value = new_addr;
	uint64_t host = 0;
	vmi_read_64_va(vmi,new_addr+0x150,0,&host);
	if(host == inode){
		status = vmi_write_64_va(vmi,new_addr+0x150,0,&value);
		if(status == VMI_SUCCESS) count++;
	}

	//修改所有file.f_inode指针
	for (int i = 0;i<filecount;i++){
		status = vmi_write_64_pa(vmi,files[i]+0x20,&new_addr);
		if(status == VMI_SUCCESS) count++;
	}

	//修改所有dentry.d_inode指针
	addr_t dentry = 0;
	vmi_read_64_va(vmi,new_addr+0x118,0,&dentry);
	while(dentry){
		addr_t tmp = 0;
		vmi_read_64_va(vmi,dentry-0x80,0,&tmp);
		if(tmp == inode) {
			status = vmi_write_64_va(vmi,dentry-0x80,0,&new_addr);
			if(status == VMI_SUCCESS) count++;
		}
		vmi_read_64_va(vmi,dentry,0,&dentry);
	}
	printf("finished to modify the pointer 0x0,the count is %d\n",count);
	total += count;
	
	printf("finished to modify all pointer ,the total is %d\n",total);
		
	
}


//修改'list_head' 'hlist_node' 结构体指针 ; 考虑指针指向自己的特殊情况
int modifypointer(vmi_instance_t vmi,addr_t old_addr , addr_t new_addr , int offset){
	int count = 0;
	uint64_t value = new_addr + offset;
	uint64_t current_next_value = 0;
	uint64_t current_prev_value = 0;
	uint64_t next_prev_value = 0;
	uint64_t prev_next_value = 0;
	status_t status;

	vmi_read_64_va(vmi,new_addr+offset,0,&current_next_value);
	if(current_next_value != 0){
		if(current_next_value == (old_addr+offset)){
			status = vmi_write_64_va(vmi,new_addr+offset,0,&value);
			if(status == VMI_SUCCESS) count++;
		}else{	
			vmi_read_64_va(vmi,current_next_value+0x8,0,&next_prev_value);
			if(next_prev_value == (old_addr+offset)){
				status = vmi_write_64_va(vmi,current_next_value+0x8,0,&value);
		
				if(status == VMI_SUCCESS) count++;
			}
		}
	}

	vmi_read_64_va(vmi,new_addr+offset+0x8,0,&current_prev_value);
	if(current_prev_value != 0){
		if(current_prev_value == (old_addr+offset)){
			status = vmi_write_64_va(vmi,new_addr+offset+0x8,0,&value);
			if(status == VMI_SUCCESS) count++;
		}else{
			vmi_read_64_va(vmi,current_prev_value,0,&prev_next_value);
			if(prev_next_value == (old_addr+offset)){
				status = vmi_write_64_va(vmi,current_prev_value,0,&value);
			
				if(status == VMI_SUCCESS) count++;
			}	
		}
			
	}
	return count;
}

//修改'hlist_head'结构体指针
int modifypointer2(vmi_instance_t vmi,addr_t old_addr, addr_t new_addr ,int offset){
	uint64_t value = new_addr + offset;
	uint64_t first = 0;
	uint64_t next_prev_value = 0;
	status_t status;
	int count = 0;
	vmi_read_64_va(vmi,new_addr+offset,0,&first);
	if(first != 0){
		if(first == (old_addr+offset)){
			status = vmi_write_64_va(vmi,new_addr+offset,0,&value);
			if(status == VMI_SUCCESS) count++;
		}else{
			vmi_read_64_va(vmi,first+0x8,0,&next_prev_value);
			if(next_prev_value == (old_addr + offset)){
				status = vmi_write_64_va(vmi,first+0x8,0,&value);
				if(status == VMI_SUCCESS) count++;
			}
		}
	}
	return count;
}


void copymemory(vmi_instance_t vmi,addr_t old_addr,addr_t new_addr,int size){
	int count = 0;
	uint64_t value;
	status_t status;
	addr_t current_read_addr = old_addr;
	addr_t current_write_addr = new_addr;
	while(count < size){
		status = vmi_read_64_va(vmi,current_read_addr+count,0,&value);
		status = vmi_write_64_va(vmi,current_write_addr+count,0,&value);
		if(status == VMI_FAILURE){
			printf("failed to write to the new addr\n");
			return;
		}
		count += 8;
	}
	printf("copy memory is finished\n");
}