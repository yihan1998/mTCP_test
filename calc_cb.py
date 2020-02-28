import math
import os
import sys

with open("cycle.txt", "r") as f:
    total = 0.0
    count = 0
    for line in f:
        res = line.split()
        if len(res) == 2:
           total += float(res[1])
           count += 1
    f.close()
    avg = round(total/float(count), 4)

print('cycle average: %.4f' % avg)
