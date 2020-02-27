import math
import os
import sys

with open("handle_read.txt", "r") as f:
    total = 0
    count = 0
    for line in f:
        res = line.split()
        if len(res) == 2 and res[1].isdigit():
           total += int(res[1])
           count += 1
    f.close()
    avg = round(float(total)/float(count), 4)

print('read_cb average: %.4f' % avg)
