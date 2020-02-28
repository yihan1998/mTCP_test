import math
import os
import sys

with open("cycle.txt", "r") as f:
    total_cyc = 0.0
    total_hdl = 0.0
    total_acc = 0.0
    count = 0
    for line in f:
        res = line.split()
        if len(res) == 6:
           total_cyc += float(res[1])
           total_hdl += float(res[3])
           total_acc += float(res[5])
           count += 1
    f.close()
    avg_cyc = round(total_cyc/float(count), 4)
    avg_hdl = round(total_hdl/float(count), 4)
    avg_acc = round(total_acc/float(count), 4)

print('cycle avg: %.4f, handle avg: %.4f, accept avg: %.4f' % (avg_cyc, avg_hdl, avg_acc))
