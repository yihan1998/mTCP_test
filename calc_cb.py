import math
import os
import sys

with open("read_cb.txt", "r") as f:
    total = 0.0
    count = 0
    for line in f:
        res = line.strip()
        total += float(res)
        count += 1
    f.close()
    avg = round(total/float(count), 4)

print('read_cb average: %.4f' % avg)

#with open("accept_cb.txt", "r") as f:
#    tot_accept = 0
#    tot_pthread_create = 0
#    count = 0
#    for line in f:
#        res = line.split()
#        if len(res) == 4 and res[1].isdigit() and res[3].isdigit():
#            tot_accept += int(res[1])
#            tot_pthread_create += int(res[3])
#            count += 1
#    f.close()
#   avg_accept = round(float(tot_accept)/float(count), 4)
#    avg_pthread_create = round(float(tot_pthread_create)/float(count), 4)

#print('accept average: %.4f' % (avg_accept))
#print('pthread_create average: %.4f' % (avg_pthread_create))
