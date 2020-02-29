import math
import os
import sys

rttFile = "rtt.txt"

total = 0.0
count = 0

with open(rttFile, "r") as f:
    for line in f:
        res = line.split()
        if len(res) == 2:
            total += float(res[1])
            count += 1
    f.close()
    avg = round(total/float(count), 4)

print("rtt average:",avg)