import numpy as np
import matplotlib.pyplot as plt
import sys
from argparse import ArgumentParser

def scale(a):
    return a/1000000.0

parser = ArgumentParser(description="plot")

parser.add_argument('--dir', '-d',
                    help="Directory to store outputs",
                    required=True)

parser.add_argument('--name', '-n',
                    help="name of the experiment",
                    required=True)

args = parser.parse_args()

fig = plt.figure(figsize=(21,3), facecolor='w')
ax = plt.gca()

# plotting CWND
windowDL = []
timeDL = []

traceDL = open (args.dir+"/"+str(args.name), 'r')
traceDL.readline()

tmp = traceDL.readline().strip().split(",")
window = int(tmp[1])
startTime = float(tmp[0])
stime=float(startTime)

for time in traceDL:
    t = float(time.strip().split(",")[0])
    if (t - float(startTime)) >= 1.0:
        window = int(time.strip().split(",")[1]) # the latest CWND in the window
        windowDL.append(window)
        timeDL.append((t-stime)/1000)
        startTime = t

print (timeDL)
print (windowDL)

plt.plot(timeDL, windowDL, lw=2, color='g')

plt.ylabel("Window Size (Packets)")
plt.xlabel("Time (s)")
# plt.xlim([0,300])
plt.grid(True, which="both")
plt.savefig(args.dir+'/window.pdf',dpi=1000,bbox_inches='tight')
