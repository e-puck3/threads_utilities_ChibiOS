# Importing the matplotlb.pyplot 
# import matplotlib
# matplotlib.use('TkAgg')
import matplotlib.pyplot as plt 
import serial
import struct
import sys
import time

RECT_HEIGHT = 10
MINIMUM_THREAD_DURATION = 0.5

#test if the serial port as been given as argument in the terminal
if len(sys.argv) == 1:
	print('Please give the serial port to use as argument')
	sys.exit(0)


try:
	port = serial.Serial(sys.argv[1], timeout=0.5)
except:
	print('Cannot connect to the e-puck2')
	sys.exit(0)

print('Connecting to port {}'.format(sys.argv[1]))

time.sleep(0.5)

#flush the input
while(port.inWaiting()):
	port.read()
	time.sleep(0.1)

print('threads_timeline')
port.write(b'threads_timeline 0\r\n')


time.sleep(0.5)

rcv = bytearray([])

while(port.inWaiting()):
	rcv += port.read()

#converts the bytearray into a string
text_rcv = rcv.decode("utf-8")

#Splits every line into an array position 
text_lines = text_rcv.split('\r\n')

print(text_lines)	

threads = []

#extracts the name of the thread
name = text_lines[1][len('Thread : '):]
#extracts the prio of the tread
prio = int(text_lines[2][len('Prio : '):])

#extracts the values
# the "ch>" at the end
#len(text_lines)-1 to not take the "ch>" at the end
values = []
for i in range(3, len(text_lines)-1):
	values.append(int(text_lines[i]))

#removes the first entry if 0
if(values[0] == 0):
	values.pop(0)

print(values)

#removes the last value if the number of value is odd
#because we need a pair of values
if(len(values) % 2):
	values.pop(-1)

#adds a thread to the threads list
threads.append({'name': name,'prio': prio, 'values': []})

#adds the values by pair to the thread
for i in range(0, len(values), 2):
	#the format for broken_barh needs to be (begin, width)
	width = values[i+1] - values[i]
	if(width <  MINIMUM_THREAD_DURATION):
		threads[0]['values'].append((values[i], MINIMUM_THREAD_DURATION))
	else:
		threads[0]['values'].append((values[i], width))


# Declaring a figure "gnt" 
fig, gnt = plt.subplots() 

# Setting Y-axis limits 
gnt.set_ylim(0, 30) 

# # Setting X-axis limits 
# gnt.set_xlim(0, 160) 

# Setting labels for x-axis and y-axis 
gnt.set_xlabel('milliseconds since boot') 
gnt.set_ylabel('Threads') 

# Setting ticks on y-axis 
gnt.set_yticks([10, 20]) 
# Labelling tickes of y-axis 
gnt.set_yticklabels([threads[0]['name'],'autre']) 

# Setting graph attribute 
gnt.grid(b = True, which='both') 

# Declaring a set of br in the timeline
gnt.broken_barh(threads[0]['values'], (10 - RECT_HEIGHT/2, RECT_HEIGHT), facecolors ='orange') 
# Declaring a set of br in the timeline
#gnt.broken_barh([(1000,0)], (20 - RECT_HEIGHT/2, RECT_HEIGHT), facecolors ='red') 

plt.show()
