# File 				: plot_threads_timeline.py 
# Author 			: Eliot Ferragni
# Creation date		: 3 april 2020
# Last modif date	: 17 april 2020
# version			: 1.0
# Brief				: This script gathers the timestamps of each thread runnnig on an
#					  e-puck2 configured to use the threads_timestamp functions
#					  in the threads_utilities.c/.h files
#					  Then it prints them on a timeline in order to let the user visualize 
#					  how the threads are behaving in the time
#
#					  To run the script : "python3 plot_threads_timeline.py serialPort"

import matplotlib.pyplot as plt
import numpy as np
import random
import serial
import struct
import sys
import time

goodbye = """
          |\      _,,,---,,_
          /,`.-'`'    -.  ;-;;,_
         |,4-  ) )-,_..;\ (  `'-'
 _______'---''(_/--'__`-'\_)______   ______            _______  _
(  ____ \(  ___  )(  ___  )(  __  \ (  ___ \ |\     /|(  ____ \| |
| (    \/| (   ) || (   ) || (  \  )| (   ) )( \   / )| (    \/| |
| |      | |   | || |   | || |   ) || (__/ /  \ (_) / | (__    | |
| | ____ | |   | || |   | || |   | ||  __ (    \   /  |  __)   | |
| | \_  )| |   | || |   | || |   ) || (  \ \    ) (   | (      |_|
| (___) || (___) || (___) || (__/  )| )___) )   | |   | (____/\ _ 
(_______)(_______)(_______)(______/ |______/    \_/   (_______/(_)                                         
"""

goodbye2 = """
                   /\_/\\
                 =( °w° )=
                   )   (  //
                  (__ __)//
 _____                 _ _                _ 
|  __ \               | | |              | |
| |  \/ ___   ___   __| | |__  _   _  ___| |
| | __ / _ \ / _ \ / _` | '_ \| | | |/ _ \ |
| |_\ \ (_) | (_) | (_| | |_) | |_| |  __/_|
 \____/\___/ \___/ \__,_|_.__/ \__, |\___(_)
                                __/ |       
                               |___/        
"""


START_Y_TICKS = 10
SPACING_Y_TICKS = 10
RECT_HEIGHT = 10
RED_DELIMITER_HEIGHT = 12
RED_DELIMITER_WIDTH = 0.1
MINIMUM_THREAD_DURATION = 0.5

threads = []
threads_count = 0
threads_name_list = []

def send_command(command, echo):
	if(echo == True):
		print('sent :',command)
	# A command should finish by a return, otherwise nothing happens
	command += '\r\n'
	# We send the command character by character with a small delay
	# because some UART to USB bridges could miss one if sent to quickly
	for char in command: 
		port.write(char.encode('utf-8'))
		time.sleep(0.001)


def receive_text(echo):
	rcv = bytearray([])

	# We read until the end of the transmission found by searching 
	# a the beginning of new command line "ch> " from the Shell
	while(True):
		rcv += port.read()
		if(rcv[-4:] == b'ch> '):
			break
	# Converts the bytearray into a string
	text_rcv = rcv.decode("utf-8")
	# Splits the strings into lines as we would see them on a terminal
	text_lines = text_rcv.split('\r\n')

	if(echo == True):
		print(text_rcv)

	return text_lines

def process_threads_count_cmd(lines, echo):
	# What we should receive :
	# line 0 			: threads_count
	# line 1 			: Number of threads : numberOfThreads
	# line 2			: ch>
	
	nb = int(lines[1][len('Number of threads : '):])
	if(echo == True):
		print('There are {} threads alive'.format(nb))

	return nb

def process_threads_timeline_cmd(lines):
	# What we should receive :
	# line 0 			: threads_timeline numberOfTheThread
	# line 1 			: Thread : nameOfTheThread
	# line 2 			: Prio : prioOfTheThread
	# lines 3 -> n-1	: timestamps
	# line n			: ch>

	# Extracts the name of the thread
	name = lines[1][len('Thread : '):]
	# If we can't extract a number, we print the lines to show the error
	try:
		# Extracts the prio of the tread
		prio = int(lines[2][len('Prio : '):])
	except:
		print(lines[1:-1])
		sys.exit(0)

	# Adds a thread to the threads list
	threads.append({'name': name,'prio': prio, 'values': []})

	if(len(lines) > 4):
		# Extracts the values
		# len(lines)-1 to not take the "ch> " at the end
		values = []
		for i in range(3, len(lines)-1):
			values.append(int(lines[i]))

		# Removes the last value if the number of value is odd
		# because we need a pair of values
		if(len(values) % 2):
			values.pop(-1)

		# Adds the values by pair to the last thread (aka the one we just created)
		for i in range(0, len(values), 2):
			# The format for broken_barh needs to be (begin, width)
			width = values[i+1] - values[i]
			if(width <  MINIMUM_THREAD_DURATION):
				threads[-1]['values'].append((values[i], MINIMUM_THREAD_DURATION))
			else:
				threads[-1]['values'].append((values[i], width))

def get_thread_prio(thread):
	return thread['prio']

def sort_threads_by_prio():
	threads.sort(key=get_thread_prio)

# Tests if the serial port as been given as argument in the terminal
if len(sys.argv) == 1:
	print('Please give the serial port to use as argument')
	sys.exit(0)


try:
	port = serial.Serial(sys.argv[1], timeout=0.1)
except:
	print('Cannot connect to the device')
	sys.exit(0)

print('Connecting to port {}'.format(sys.argv[1]))

# In case there was a communication problem
# we send two return commands to trigger the sending of 
# a new command line from the Shell (to begin from the beginning)
port.write(b'\r\n')
port.write(b'\r\n')
time.sleep(0.1)

# Flushes the input
while(port.inWaiting()):
	port.read()

# Sends command "threads_count"
send_command('threads_count', False)
rcv_lines = receive_text(False)
threads_count = process_threads_count_cmd(rcv_lines, True)

# Sends command "threads_timeline x" threads_count times to recover all the threads logs
for i in range(threads_count):
	send_command('threads_timeline ' + str(i), True)
	rcv_lines = receive_text(False)
	process_threads_timeline_cmd(rcv_lines)

sort_threads_by_prio()

for i in range(threads_count):
	# Splits th names into multiple lines to spare space next to the graph
	threads_name_list.append(threads[i]['name'].replace(' ','\n') + '\nPrio:'+ str(threads[i]['prio']))

# Sends command "threads_stat"
send_command('threads_stat', False)
print('Stack usage of the running threads')
receive_text(True)

# Sends command "threads_uc"
send_command('threads_uc', False)
print('Computation time taken by the running threads')
receive_text(True)

port.close()
print('Port {} closed'.format(sys.argv[1]))

# Declaring a figure "gnt" 
# figsize is in inch
fig, gnt = plt.subplots(figsize=(15, 10), dpi=90)

plt.subplots_adjust(right=0.97)

# Setting Y-axis limits 
#gnt.set_ylim(0, 30) 

# # Setting X-axis limits 
# gnt.set_xlim(0, 3000) 

plt.title('Threads timeline')

# Setting labels for x-axis and y-axis 
gnt.set_xlabel('milliseconds since boot')
gnt.set_ylabel('Threads')

# Setting ticks on y-axis 
gnt.set_yticks(range(START_Y_TICKS, (threads_count+1)*SPACING_Y_TICKS, SPACING_Y_TICKS))
# Labelling tickes of y-axis 
gnt.set_yticklabels(threads_name_list, multialignment='center')

# Setting graph attribute 
gnt.grid(b = True, which='both')

# Colors used for the timeline
colors=['green','blue','cyan','black']

# Draws a rectangle every time a thread is running
for i in range(threads_count):
	color = random.randrange(0,len(colors),1)
	y_row = (START_Y_TICKS +  SPACING_Y_TICKS * i) - RECT_HEIGHT/2
	gnt.broken_barh(threads[i]['values'], (y_row, RECT_HEIGHT), facecolors=colors[color])

# Draws a red delimiter on top of the rectangles 
# to show that a thread has stopped and begun on the same time stamp
for i in range(threads_count):
	last_end = 0
	red_bars = []
	y_row = (START_Y_TICKS +  SPACING_Y_TICKS * i) - RED_DELIMITER_HEIGHT/2
	for begin, width in threads[i]['values']:
		if(last_end == begin):
			red_bars.append((begin, RED_DELIMITER_WIDTH))
		last_end = begin + width
	# Draws all the red bars at once, otherwise the graph has a really bad response time
	gnt.broken_barh(red_bars, (y_row , RED_DELIMITER_HEIGHT), facecolors='red')

plt.show()

# Be polite, say goodbye :-)
print(goodbye)
