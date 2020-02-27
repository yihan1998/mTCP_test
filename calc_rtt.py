import math
import os
import sys

rttFile = "rtt.txt"

total = 0
count = 0

with open(rttFile, "r") as f:
    for line in f:
        res = line.split()
        if len(res) == 2 and res[1].isdigit():
            total += int(res[1])
            count += 1
    f.close()
    avg = round(float(total)/float(count), 4)

print("rtt average:",avg)
