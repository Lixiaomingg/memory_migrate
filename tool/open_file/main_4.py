# encoding:utf-8

#定义打开文件最大数量
MAX_NUM_FILE = 61

process_name = __file__
process_name = process_name.split('.')[0]
file_list = []
for i in range(MAX_NUM_FILE):
    file_name = process_name + "_" + str(i) + ".txt"
    file = open(file_name,"w")
    file.write("hello world")
    file_list.append(file)


raw_input("input:")
for file in file_list:
    file.close

print "ok"