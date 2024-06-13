# convert a file containing "af 0 44 33 22 11" etc. to a binary file containing those bytes
# usage: python hex2bin.py input.txt output.bin

import sys

if len(sys.argv) != 3:
    print("usage: python hex2bin.py input.txt output.bin")
    sys.exit(1)

with open(sys.argv[1], "r") as f:
    lines = f.readlines()

with open(sys.argv[2], "wb") as f:
    for line in lines:
        for byte in line.split():
            f.write(bytes([int(byte, 16)]))

print("done")