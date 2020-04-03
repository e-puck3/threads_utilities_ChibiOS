# Importing the matplotlb.pyplot 
# import matplotlib
# matplotlib.use('TkAgg')
import matplotlib.pyplot as plt 
import numpy as np
import random
import serial
import struct
import sys
import time


START_Y_TICKS = 10
SPACING_Y_TICKS = 10
RECT_HEIGHT = 10
MINIMUM_THREAD_DURATION = 0.5

threads = []
threads_count = 0
threads_name_list = []

def send_command(command, echo):
	#sends command "threads_count"
	if(echo == True):
		print('sent :',command)

	port.write(bytes(command, 'utf-8')+b'\r\n')
	time.sleep(0.1)


def receive_text(echo):
	rcv = bytearray([])
	while(port.inWaiting()):
		rcv += port.read()

	#converts the bytearray into a string
	text_rcv = rcv.decode("utf-8")
	#Splits every line into an array position 
	text_lines = text_rcv.split('\r\n')

	if(echo == True):
		print(text_rcv)

	return text_lines

def process_threads_count_cmd(lines, echo):
	nb = int(lines[1][len('Number of threads : '):])
	if(echo == True):
		print('There are {} threads alive'.format(nb))

	return nb

def process_threads_timeline_cmd(lines):
	#extracts the name of the thread
	name = lines[1][len('Thread : '):]
	#extracts the prio of the tread
	prio = int(lines[2][len('Prio : '):])

	#extracts the values
	#len(lines)-1 to not take the "ch>" at the end
	values = []
	for i in range(3, len(lines)-1):
		values.append(int(lines[i]))

	#removes the first entry if 0
	if(values[0] == 0):
		values.pop(0)

	#removes the last entries if -1
	#happens if the log is not full
	while(values[-1] == -1):
		values.pop(-1)

	#removes the last value if the number of value is odd
	#because we need a pair of values
	if(len(values) % 2):
		values.pop(-1)

	#adds a thread to the threads list
	threads.append({'name': name,'prio': prio, 'values': []})

	#adds the values by pair to the last thread (aka the one we just created)
	for i in range(0, len(values), 2):
		#the format for broken_barh needs to be (begin, width)
		width = values[i+1] - values[i]
		if(width <  MINIMUM_THREAD_DURATION):
			threads[-1]['values'].append((values[i], MINIMUM_THREAD_DURATION))
		else:
			threads[-1]['values'].append((values[i], width))

def get_thread_prio(thread):
	return thread['prio']

def sort_threads_by_prio():
	threads.sort(key=get_thread_prio)

#test if the serial port as been given as argument in the terminal
if len(sys.argv) == 1:
	print('Please give the serial port to use as argument')
	sys.exit(0)


try:
	port = serial.Serial(sys.argv[1], timeout=0.1)
except:
	print('Cannot connect to the e-puck2')
	sys.exit(0)

print('Connecting to port {}'.format(sys.argv[1]))

time.sleep(0.5)

#flush the input
while(port.inWaiting()):
	port.read()
	time.sleep(0.1)

#sends command "threads_count"
send_command('threads_count', False)
rcv_lines = receive_text(False)
threads_count = process_threads_count_cmd(rcv_lines, True)

#sends command "threads_timeline" threads_count times to recover all the threads logs
for i in range(threads_count):
	send_command('threads_timeline ' + str(i), True)
	rcv_lines = receive_text(False)
	process_threads_timeline_cmd(rcv_lines)

sort_threads_by_prio()

for i in range(threads_count):
	threads_name_list.append(threads[i]['name'])

send_command('threads_stat', False)
print('Stack usage of the running threads')
receive_text(True)

send_command('threads_uc', False)
print('Computation time taken by the running threads')
receive_text(True)

port.close()
print('Port {} closed'.format(sys.argv[1]))

# Declaring a figure "gnt" 
fig, gnt = plt.subplots() 

# Setting Y-axis limits 
#gnt.set_ylim(0, 30) 

# # Setting X-axis limits 
gnt.set_xlim(900, 3000) 

# Setting labels for x-axis and y-axis 
gnt.set_xlabel('milliseconds since boot') 
gnt.set_ylabel('Threads') 

# Setting ticks on y-axis 
gnt.set_yticks(range(START_Y_TICKS, (threads_count+1)*SPACING_Y_TICKS, SPACING_Y_TICKS)) 
# Labelling tickes of y-axis 
gnt.set_yticklabels(threads_name_list) 

# Setting graph attribute 
gnt.grid(b = True, which='both') 

colors=['red','green','blue','cyan','magenta','yellow','black']

for i in range(threads_count):
	# Declaring a set of br in the timeline
	color = random.randrange(0,len(colors),1)
	gnt.broken_barh(threads[i]['values'], ((START_Y_TICKS +  SPACING_Y_TICKS * i) - RECT_HEIGHT/2, RECT_HEIGHT), facecolors=colors[color]) 
# Declaring a set of br in the timeline
#gnt.broken_barh([(1000,0)], (20 - RECT_HEIGHT/2, RECT_HEIGHT), facecolors ='red') 

plt.show()
