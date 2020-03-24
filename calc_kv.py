import math
import os
import sys

with open(str(sys.argv[1]), "r") as f:
    total = 0.0
    count = 0
    for line in f:
        res = line.strip()
        total += float(res)
        count += 1
    f.close()
    avg = round(total/float(count), 4)

print('throughput avg: %.4f' % (avg))