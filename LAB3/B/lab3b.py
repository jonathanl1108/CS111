import sys




block_dict = {}
ref_inode = {}
allocated_inode=set()
bfree = set()
ifree = set()
inode_dict_parents = {}
inode_dict_link_count = {}
inode_dict_refer_link_count = {}
inode_dict_link_refer = {}

total_blocks = 0
total_inodes = 0
inode_size = 0
block_size = 0
first_valid_block_index = 0
first_valid_inode = 0
offset = 0
linkcount = 0
links = 0
error_node = False


#================================================================================
#================================================================================
#================================================================================
# 
def create_error_msg(level, block, ref):
    if level == 0:
        print('DUPLICATE BLOCK ' + str(block) + ' IN INODE ' + str(ref[0]) + ' AT OFFSET ' + str(ref[1]))
    elif level == 1:
        print('DUPLICATE INDIRECT BLOCK ' + str(block) + ' IN INODE ' + str(ref[0]) + ' AT OFFSET ' + str(ref[1]))
    elif level == 2:
        print('DUPLICATE DOUBLE INDIRECT BLOCK ' + str(block) + ' IN INODE ' + str(ref[0]) + ' AT OFFSET ' + str(ref[1]))
    elif level == 3:
        print('DUPLICATE TRIPLE INDIRECT BLOCK ' + str(block) + ' IN INODE ' + str(ref[0]) + ' AT OFFSET ' + str(ref[1]))




def unreferenced_block(total_blocks, first_valid_block_index, block_dict, error_node, bfree):
    for x in range(1, total_blocks + 1):
        if x in bfree and x in block_dict:
            print('ALLOCATED BLOCK ' + str(x) + ' ON FREELIST')
            error_node = True
        elif x not in bfree and x not in block_dict and x > first_valid_block_index and x < 64:
            print('UNREFERENCED BLOCK ' + str(x))
            error_node = True
        



def allocated_inodes(first_valid_inode,total_inodes, ifree, inode_dict_link_count, inode_dict_link_refer, error_node, allocated_inode):

    for x in range(1, total_inodes + 1):
        if x in inode_dict_link_count and x in ifree:
            print('ALLOCATED INODE ' + str(x) + ' ON FREELIST')
            error_node = True
        # some of the reserve inode  like 1 and 3-10 
        elif x < first_valid_inode and x == 2 and x not in ifree and x not in inode_dict_link_count and x not in inode_dict_link_refer and x not in  allocated_inode:
            print('UNALLOCATED INODE ' + str(x) + ' NOT ON FREELIST')
            error_node = True
        elif x >= first_valid_inode and x not in ifree and x not in inode_dict_link_count and x not in inode_dict_link_refer and x not in  allocated_inode:
            print('UNALLOCATED INODE ' + str(x) + ' NOT ON FREELIST')
            error_node = True
        



def duplicate_block(block_dict, error_node, offset):

    for block in block_dict:
        block_info = block_dict[block]
        if len(block_info) > 1: 
            error_node = True
            for ref in block_info: 
                level = int(ref[2])
                create_error_msg(level, block, ref)
                



def directory_link( inode_dict_parents, inode_dict_link_refer, error_node):


    for parent in inode_dict_parents:

        if parent == 2 and inode_dict_parents[parent] == 2:
            continue
        elif parent == 2:
            print("DIRECTORY INODE 2 NAME '..' LINK TO INODE " + str(inode_dict_parents[parent]) + " SHOULD BE 2")
            error_node = True
        elif parent not in inode_dict_link_refer:
            print("DIRECTORY INODE " + str(inode_dict_parents[parent]) + " NAME '..' LINK TO INODE " + str(parent) + " SHOULD BE " + str(inode_dict_parents[parent]))
            error_node = True
        elif inode_dict_parents[parent] != inode_dict_link_refer[parent]: 
            print("DIRECTORY INODE " + str(parent) + " NAME '..' LINK TO INODE " + str(parent) + " SHOULD BE " + str(inode_dict_link_refer[parent]))
            error_node = True









def link_count(inode_dict_link_count, inode_dict_refer_link_count, linkcount, links):

    for x in inode_dict_link_count:
        linkcount = inode_dict_link_count[x]

        if x in inode_dict_refer_link_count:
            links = inode_dict_refer_link_count[x]
        else:
            links = 0

        if links != linkcount:
            print('INODE ' + str(x) + ' HAS ' + str(links) + ' LINKS BUT LINKCOUNT IS ' + str(linkcount))
            error_node = True




def unallocated_inode(ref_inode, inode_dict_link_refer, error_node):
    for x in ref_inode:
        if x in ifree and x in inode_dict_link_refer:
            parent_inode = inode_dict_link_refer[x]
            dir_name = ref_inode[x]
            print('DIRECTORY INODE ' + str(parent_inode) + ' NAME ' + dir_name[0:len(dir_name)- 2] + "' UNALLOCATED INODE " + str(x))
            error_node = True

def mutex_output_gen(i):
    if i == 24:
         return [" INDIRECT","12","1"]
    elif i == 25:
        return [" DOUBLE INDIRECT","268","2"]
    elif i == 26:
        return [" TRIPLE INDIRECT","65804","3"]
    else:
        return ["","0","0"]


def header_indirect(offset, error_node, block_dict, total_blocks, entry):

    output=mutex_output_gen(int(entry[2]))

    if int(entry[5]) < 0 or int(entry[5]) > total_blocks: 
        print('INVALID ' + output[0] + ' BLOCK ' + str(int(entry[5])) + ' IN INODE ' + str(int(entry[1])) + ' AT OFFSET ' + output[1])
        error_node = True

    elif int(entry[5]) < first_valid_block_index:
        print('RESERVED ' + output[0] + ' BLOCK ' + str(int(entry[5])) + ' IN INODE ' + str(int(entry[1]))+ ' AT OFFSET ' + output[1])
        error_node = True

    elif int(entry[5]) not in block_dict: 
        block_dict[int(entry[5])] = [ [int(entry[1]), output[1], int(entry[2])] ]

    else: 
        block_dict[int(entry[5])].append([int(entry[1]), output[1], int(entry[2])])



def process_inode(i, block_num,error_node,total_blocks,entry,block_dict,first_valid_block_index):
    output = mutex_output_gen(i)

    if block_num < 0 or block_num > total_blocks: 
        print('INVALID' + output[0] + ' BLOCK ' + str(block_num) + ' IN INODE ' + str(int(entry[1])) + ' AT OFFSET ' +output[1] )
        error_node = True
    elif block_num > 0 and block_num < first_valid_block_index:
        print('RESERVED' + output[0] + ' BLOCK ' + str(block_num) + ' IN INODE ' + str(int(entry[1])) + ' AT OFFSET ' + output[1])
        error_node = True
    elif block_num not in block_dict: 
        block_dict[block_num] = [ [int(entry[1]), output[1], output[2]]]
    else: 
        block_dict[block_num].append([int(entry[1]), output[1], output[2]])



def main():

    global error_node

    try:
        input_file = open(sys.argv[1], "r")
    except:
        sys.stderr.write('Error Occured When Opening the File \n')
        exit(1)

    lines = input_file.readlines()
    input_file.close()


 
    for i in lines:
        entry = i.split(",")
        header = entry[0]


        if header == 'SUPERBLOCK': 
            total_blocks = int(entry[1])
            total_inodes = int(entry[2])
            block_size = int(entry[3])
            inode_size = int(entry[4])
            first_valid_inode = int(entry[7])

        elif header == 'GROUP':
            first_valid_block_index = int(int(entry[8])+int(entry[3])*inode_size/block_size)

        elif header == 'BFREE': 
            bfree.add(int(entry[1])) 
        elif header == 'IFREE': 
            ifree.add(int(entry[1])) 

        elif header == 'INODE':
            inode_number=int(entry[1])
            inode_dict_link_count[inode_number] = int(entry[6])
            allocated_inode.add(inode_number)
            for i in range(12, 27): 
                block_num = int(entry[i])
                if block_num == 0: 
                    continue
                process_inode(i, block_num,error_node,total_blocks,entry,block_dict,first_valid_block_index)

        elif header == 'INDIRECT':
            header_indirect(offset, error_node, block_dict, total_blocks, entry)
        elif header == 'DIRENT': 

            dir_name = entry[6]
            dir_inum= int(entry[3])
            ref_inode[dir_inum] = dir_name
           

            if dir_inum not in inode_dict_refer_link_count:
                inode_dict_refer_link_count[dir_inum] = 1
            else:
                inode_dict_refer_link_count[dir_inum] += 1
            if dir_inum < 1 or dir_inum > total_inodes:
                print('DIRECTORY INODE ' + str(int(entry[1])) + ' NAME ' + dir_name[0:len(dir_name)- 2] + "' INVALID INODE " + str(dir_inum))
                error_node = True
                continue

            if dir_name[0:3] == "'.'" and int(entry[1]) != dir_inum:
                print('DIRECTORY INODE ' + str(int(entry[1])) + " NAME '.' LINK TO INODE " + str(dir_inum) + ' SHOULD BE ' + str(int(entry[1])))
                error_node = True

            elif dir_name[0:3] == "'.'":
                continue

            elif dir_name[0:4] == "'..'":
                inode_dict_parents[int(entry[1])] = dir_inum

            else:
                inode_dict_link_refer[dir_inum] = int(entry[1])

    unreferenced_block(total_blocks, first_valid_block_index, block_dict, error_node, bfree)
    allocated_inodes(first_valid_inode,total_inodes, ifree, inode_dict_link_count, inode_dict_link_refer, error_node,  allocated_inode)
    duplicate_block(block_dict, error_node, offset)
    directory_link( inode_dict_parents, inode_dict_link_refer, error_node)
    link_count(inode_dict_link_count, inode_dict_refer_link_count, linkcount, links)
    unallocated_inode(ref_inode, inode_dict_link_refer, error_node)


    if error_node:
        exit(2)
    else:
        exit(0)





if __name__ == '__main__':
	main()
