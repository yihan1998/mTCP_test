import math
import os
import sys

core = int(sys.argv[1])

with open("throughput.txt", "r") as f:
    total_rps = 0.0
    total_thp = 0.0
    count = 0
    temp = 0
    total = 0
    for line in f:
        res = line.split()
        if len(res) == 4:
           total_rps += float(res[1])
           total_thp += float(res[3])
           count += 1
            if count == core:
                total += 1
                count = 0
    f.close()
    avg_rps = round(total_rps/float(total), 4)
    avg_thp = round(total_thp/float(total), 4)

print('rps avg: %.4f, throughput avg: %.4f' % (avg_rps, avg_thp))
