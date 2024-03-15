# Ramdisk creation tool.
# Follows the format defined in doc/format/ramdisk.txt

import os
import sys
import struct

# Ramdisk format:
# - uint32_t total_sz
# - uint32_t num_files
# - uint32_t headers_sz
# - uint32_t num_root_files
# :FILE HEADERS:
# :FILE DATA:

# File header format:
# --------------------------------
# 0   FILE_ENTRY                 -
# --------------------------------
# 2   File Offset                -
# --------------------------------
# 6   File Name                  -
# --------------------------------
# 70  File Length                -
# --------------------------------
# 74  Reserved                   -
# --------------------------------

# Directory header format:
# --------------------------------
# 0   DIR_ENTRY                  -
# --------------------------------
# 2   Num. Files Contained       -
# --------------------------------
# 6   Directory Name             -
# --------------------------------
# 70  Number of Blocks Contained -
# --------------------------------
# 74  Reserved                   -
# --------------------------------

# Each header is 80 bytes long for future expansion.

def main(argv):
    if len(argv) < 2:
        print("Usage: ramdisk.py [output] [dir]")
        return 1
    if len(argv) == 1:
        output = "./ramdisk.bin"
    else:
        output = argv[1]
    if len(argv) == 2:
        directory = "./ramdisk"
    else:
        directory = argv[2]

    print("input: {}".format(directory))
    print("output: {}".format(output))


    if not os.path.isdir(directory):
        print("Error: {} is not a directory.".format(directory))
        return 1

    # Get the tree structure of the directory
    def tree_branch(tree = [], path = directory):
        for item in os.listdir(path):
            item_path = os.path.join(path, item)
            if os.path.isdir(item_path):
                tree.append([(item, "DIR"), tree_branch([], item_path)])
            else:
                tree.append((item, "FILE"))
        return tree

    tree = tree_branch()

    print(tree)

    global headers_num
    global num_files

    num_files = 0
    headers_num = 0
    order = []
    header_sz_dict = {}
    def order_recursive(inp, path=""):
        global headers_num
        global num_files
        if type(inp) is tuple:
            order.append(path + inp[0])
            num_files += 1
            headers_num += 1
            return
        headers_pre = headers_num
        for item in inp:
            if type(item) is tuple:
                order.append(path + item[0])
                num_files += 1
                headers_num += 1
                continue
            this_dir = item[0][0]
            order.append(path + this_dir + "/")
            headers_num += 1
            order_recursive(item[1], path + this_dir + "/")
        header_sz_dict[path] = headers_num - headers_pre


    order_recursive(tree)

    print(order)
    print(header_sz_dict)

    # Begin writing the ramdisk to an in-memory buffer
    ramdisk = bytearray()

    # We don't know the size of the ramdisk yet, so we'll just write a placeholder
    ramdisk.extend(struct.pack("<I", 0))
    # Write the number of files
    ramdisk.extend(struct.pack("<I", num_files))
    # Write the size of the headers
    ramdisk.extend(struct.pack("<I", (headers_num * 80) + 16))
    # Number of files/directories in the root directory
    ramdisk.extend(struct.pack("<I", len(tree)))

    # Write the headers
    # FILE_ENTRY = 0xBAE7
    # DIR_ENTRY = 0x7EAB
    file_bytepos = 0
    for i in range(len(order)):
        if order[i][-1] == "/":
            print("Directory: {}".format(order[i]))
            # This is a directory
            ramdisk.extend(struct.pack("<H", 0x7EAB)) # 2 bytes
            # Find the number of files within this level of the tree
            num_files = len(os.listdir(os.path.join(directory, order[i])))
            ramdisk.extend(struct.pack("<I", num_files)) # 4 bytes
            # 64-byte name (including null terminator)
            sent_name = order[i]
            if order[i][-1] == "/":
                sent_name = order[i][:-1]
            # If there are multiple path components, we only want the last one
            if "/" in sent_name:
                sent_name = sent_name.split("/")[-1]
            if len(sent_name) > 63:
                print("Error: Directory name {} is too long.".format(sent_name))
                return 1
            ramdisk.extend(sent_name.encode("ascii")) # 64 bytes
            ramdisk.extend(b"\x00" * (64 - len(sent_name)))
            # Number of blocks contained
            ramdisk.extend(struct.pack("<I", header_sz_dict[order[i]])) # 4 bytes
            # Reserved through 80 bytes
            ramdisk.extend(b"\x00" * 6)
        else:
            print("File: {}".format(order[i]))
            ramdisk_pre_len = len(ramdisk)
            # This is a file
            ramdisk.extend(struct.pack("<H", 0xBAE7)) # 2 bytes
            # File offset
            ramdisk.extend(struct.pack("<I", file_bytepos)) # 4 bytes
            file_bytepos += os.path.getsize(os.path.join(directory, order[i]))
            # 64-byte name (including null terminator)
            sent_name = order[i]
            if order[i][-1] == "/":
                sent_name = order[i][:-1]
            # If there are multiple path components, we only want the last one
            if "/" in sent_name:
                sent_name = sent_name.split("/")[-1]
            if len(sent_name) > 63:
                print("Error: File name {} is too long.".format(sent_name))
                return 1
            ramdisk.extend(sent_name.encode("ascii")) # 64 bytes
            ramdisk.extend(b"\x00" * (64 - len(sent_name))) # - - -
            # File length
            ramdisk.extend(struct.pack("<I", os.path.getsize(os.path.join(directory, order[i])))) # 4 bytes
            # Reserved through 80 bytes
            ramdisk.extend(b"\x00" * 6)
            
    # We now know the size of the ramdisk, so we can write it to the beginning
    ramdisk[0:4] = struct.pack("<I", len(ramdisk) + file_bytepos)

    # Write the files
    for i in range(len(order)):
        if order[i][-1] == "/":
            continue
        with open(os.path.join(directory, order[i]), "rb") as f:
            ramdisk.extend(f.read())

    # Write the ramdisk to the output file
    with open(output, "wb") as f:
        f.write(ramdisk)

    return 0




if __name__ == "__main__":
    sys.exit(main(sys.argv))