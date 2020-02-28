import math
import os
import sys

with open(str(sys.argv[1]), "r") as f:
    total_rps = 0.0
    total_thp = 0.0
    count = 0
    for line in f:
        res = line.split()
        if len(res) == 4:
           total_rps += float(res[1])
           total_thp += float(res[3])
           count += 1
    f.close()
    avg_rps = round(total_rps/float(count), 4)
    avg_thp = round(total_thp/float(count), 4)

print('rps avg: %.4f, throughput avg: %.4f' % (avg_rps, avg_thp))
