import matplotlib.pyplot as plt
import numpy as np
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('--src', default='./topoNixVector.dat', type=str)
parser.add_argument('--protocol',default='NixVector',type=str)

opt = parser.parse_args()

rx_time = []
delay_time = []

scan_time = []
queue_len = []
avg_delay_time = [0,]
avg_rx_time = [0,]   

s = 0
t = 0
th = 0.5


file = open(opt.src).readlines()

for line in  file:
    if line.startswith('Delay'):
        tmp = line.split()
        rx_time.append(eval(tmp[1]))
        delay_time.append(eval(tmp[2])/1e6)
        t += 1

        if eval(tmp[1]) >= th:
            avg_delay_time.append(np.mean(delay_time[s:t]))
            avg_rx_time.append(th)
            th += 0.5
            s = t
    elif line.startswith('Max'):
        tmp = line.split()
        scan_time.append(eval(tmp[-1]))
        queue_len.append(eval(tmp[1]))

avg_queue = np.mean(queue_len)
avg_delay = np.mean(delay_time)
rx_rate = 1 - (np.array(delay_time) == 0).sum() / len(delay_time)

print('Protocol: ', opt.protocol )
print('avg_queue: ', avg_queue)
print('max_queue: ', np.max(queue_len))
print('avg_delay: ', avg_delay)
print('max_delay: ', np.max(delay_time))
print('success_trans_rate: ', rx_rate)



plt.plot(scan_time[::3], queue_len[::3])
plt.title("MaxQueueLength_{}".format(opt.protocol))
plt.xlabel("time/sec")
plt.ylabel("queue_len/byte")
plt.savefig("fig/MaxQueueLength_{}.png".format(opt.protocol))
plt.figure()

plt.plot(avg_rx_time, avg_delay_time)
plt.title("Delay_{}".format(opt.protocol))
plt.xlabel("time/sec")
plt.ylabel("delay/ms")
plt.savefig("fig/Delay_{}.png".format(opt.protocol))
plt.show()
