# File 				: plot_threads_timeline.py 
# Author 			: Eliot Ferragni
# Creation date		: 3 april 2020
# Last modif date	: 17 april 2020
# version			: 1.0
# Brief				: This script gathers the timestamps of each thread to log on an
#					  e-puck2 configured to use the threads_timestamp functions
#					  in the threads_utilities.c/.h files
#					  Then it prints them on a timeline in order to let the user visualize 
#					  how the threads are behaving in the time
#
#					  To run the script : "python3 plot_threads_timeline.py serialPort"

import matplotlib.pyplot as plt
import  matplotlib.ticker as tick
from matplotlib.widgets import Button
import numpy as np
import random
import serial
import struct
import sys
import time

GOODBYE = """
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

GOODBYE2 = """
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

NEW_RECEIVED_LINE = '> '

WINDOWS_SIZE_X 		= 15
WINDOWS_SIZE_Y 		= 10
WINDOWS_DPI			= 90
SUBPLOT_ADJ_RIGHT	= 0.97
SUBPLOT_ADJ_TOP		= 0.96

START_Y_TICKS 				= 10
SPACING_Y_TICKS 			= 10
RECT_HEIGHT 				= 10
MINIMUM_THREAD_DURATION 	= 1
VISUAL_WIDTH_TRIGGER		= 5 # no unit
DIVISION_FACTOR_TICK_STEP 	= 3
ZOOM_LEVEL_THRESHOLD 		= 8

# for the extracted values fields
EXTR_THREAD_OUT 	= 0
EXTR_THREAD_IN		= 1
EXTR_TIME			= 2

# for the raw_values field of a thread
RAW_TIME 		= 0
RAW_IN_OUT_TYPE	= 1
RAW_SHIFT_NB	= 2

threads = []
deleted_threads = []
threads_name_list = []
trigger_time = None
nb_subdivisions = 0

def flush_shell():
	# In case there was a communication problem
	# we send two return commands to trigger the sending of 
	# a new command line from the Shell (to begin from the beginning)
	port.write(b'\r\n')
	port.write(b'\r\n')
	time.sleep(0.1)

	# Flushes the input
	while(port.inWaiting()):
		port.read()

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
	# the beginning of a new command line "ch> " from the Shell
	while(True):
		rcv += port.read()
		if(rcv[-4:] == b'ch> '):
			break
	# Converts the bytearray into a string
	text_rcv = rcv.decode("utf-8")
	# Splits the strings into lines as we would see them on a terminal
	text_lines = text_rcv.split('\r\n')

	if(echo == True):
		for line in text_lines:
			print(NEW_RECEIVED_LINE, line)

	return text_lines

def append_thread(thread_list, name, nb, prio, log):

	if(log == 'Yes'):	
		# Adds a logged thread to the threads list
		thread_list.append({'name': name,'nb': nb,'prio': prio,'log': True, 'raw_values': [],'have_values': False, 'values': []})
	else:
		# Adds a non logged thread to the threads list
		thread_list.append({'name': name,'nb': nb,'prio': prio,'log': False, 'raw_values': [],'have_values': False, 'values_in': [],'values_out': []})


def process_threads_list_cmd(lines):
	# What we should receive :
	# line 0 			: threads_list
	# line 1 -> n-1 	: Thread number xx : Prio = xxx, Log = xxx, Name = str
	# line n			: ch>

	deleted = False
	
	# If the received text doesn't match what we expect, print it and quit
	if(lines[1][:len('Thread number')] != 'Thread number'):
		print('Bad answer received, see below :')
		for line in lines:
			print(NEW_RECEIVED_LINE, line)
		sys.exit(0)

	for i in range(1,len(lines)-1):
		if(not deleted and lines[i] == 'Deleted threads: '):
			deleted = True
			continue
		nb 		= int(lines[i][len('Thread number '):len('Thread number ')+2])
		prio 	= int(lines[i][len('Thread number xx : Prio = '):len('Thread number xx : Prio = ')+3])
		log 	= lines[i][len('Thread number xx : Prio = xxx, Log = '):len('Thread number xx : Prio = xxx, Log = ')+3]
		name 	= lines[i][len('Thread number xx : Prio = xxx, Log = xxx, Name = '):]

		if(deleted):
			append_thread(deleted_threads, name, nb, prio, log)
		else:
			append_thread(threads, name, nb, prio, log)


	# The deleted threads are given in the order they have been deleted
	# and the number they have was the thread number at the time they existed
	# -> By inserting them to the threads list in the reverse order at their old position, 
	# 	 we recover the original threads creation order

	while(len(deleted_threads)):
		threads.insert(deleted_threads[-1]['nb']-1, deleted_threads.pop(-1))


def process_threads_timestamps_cmd(lines):
	# What we should receive (one more line if the trigger mode is enabled) :
	# line 0 			: threads_timestamps
	# (line 1)			: Triggered at xxxxxxx
	# line 1 (2) -> n-1 : From xx to xx at xxxxxxx
	# line n			: ch>

	first_data_line = 1

	# Gets the trigger time if any
	if(lines[first_data_line][:len('Triggered at ')] == 'Triggered at '):
		global trigger_time
		trigger_time = int(lines[first_data_line][len('Triggered at '):])
		first_data_line = 2
	# No trigger
	else:
		trigger_time = None

	# If the received text doesn't match what we expect, print it and quit
	if(lines[first_data_line][:len('From ')] != 'From '):
		print('Bad answer received, see below :')
		for line in lines:
			print(NEW_RECEIVED_LINE, line)
		sys.exit(0)

	counter = 0
	max_counter = 0
	last_time = None
	for i in range(first_data_line,len(lines)-1):
		thread_out 	= int(lines[i][len('From '):len('From ')+2])
		thread_in 	= int(lines[i][len('From xx to '):len('From xx to ')+2])
		time 		= int(lines[i][len('From xx to xx at '):])

		# A thread has been deleted
		if(thread_out == thread_in):
			deleted_threads.append(threads.pop(thread_out-1))
		else:
			# Counts how many time we enter and leave threads during the same timestamp
			# to be able to show it on the timeline
			if (time != last_time):
				counter = 0
			else:
				counter += 1 

			# The line after a thread deletion contains a 0 because the out thread doesn't exist anymore
			# -> Add the OUT timestamp to the last deleted thread
			if(thread_out == 0):
				deleted_threads[-1]['raw_values'].append((time, 'out', counter))
			else:
				threads[thread_out-1]['raw_values'].append((time, 'out', counter))

			threads[thread_in-1]['raw_values'].append((time, 'in', counter))
			
			last_time = time
			# We need to know the maximum of IN and OUT times during the same timetamp to be able to do
			# correctly the subdivisions on the timeline
			if(counter > max_counter):
				max_counter = counter

	while(len(deleted_threads)):
		threads.insert(deleted_threads[-1]['nb']-1, deleted_threads.pop(-1))

	# Since the count of elements begins at 0 for computations reasons, we 
	# need t oadd one for the real number of elements used to draw the elements
	max_counter += 1
	step = 1/max_counter

	# size of an IN or OUT tick (for incomplete data)
	tick_step = step/DIVISION_FACTOR_TICK_STEP

	for thread in threads:
		if(len(thread['raw_values']) > 0):
			# Thread logged by the MCU
			if(thread['log']):

				if(thread['raw_values'][0][RAW_IN_OUT_TYPE] == 'out'):
					if(thread['raw_values'][0][RAW_TIME] == 0):
						# Insert an IN time in case the first we encounter is an out time and time is 0
						# Happens with the main thread that has no IN time at boot 
						# (no context switch to main since it's the first thread to begin)
						thread['raw_values'].insert(0, (0,'in', 0))
					else:
						# Deletes the first value if it's an OUT time to not mess the timeline
						thread['raw_values'].pop(0)

				# Removes the last value if the number of value is odd
				# because we need a pair of values
				if(len(thread['raw_values']) % 2):
					thread['raw_values'].pop(-1)

				# Adds the values by pair to the thread
				for i in range(0, len(thread['raw_values']), 2):
					# The format for broken_barh needs to be (begin, width)
					shift = thread['raw_values'][i][RAW_SHIFT_NB] * step
					begin = thread['raw_values'][i][RAW_TIME] + shift
					width = thread['raw_values'][i+1][RAW_TIME] - thread['raw_values'][i][RAW_TIME] - shift
					
					if(width <  1):
						thread['values'].append((begin, step))
					else:
						thread['values'].append((begin, width))
				# Indicates if we have timestamps to draw
				if(len(thread['values']) > 0):
					thread['have_values'] = True

			# Thread not logged by the MCU
			else:
				# Adds one by one the incomplete values
				for i in range(0, len(thread['raw_values']), 1):
					# The format for broken_barh needs to be (begin, width)
					shift = thread['raw_values'][i][RAW_SHIFT_NB] * step
					begin = thread['raw_values'][i][RAW_TIME] + shift
					if(thread['raw_values'][i][RAW_IN_OUT_TYPE] == 'in'):
						thread['values_in'].append((begin, tick_step))
					else:
						thread['values_out'].append((begin - tick_step, tick_step))

				# Indicates if we have timestamps to draw
				if((len(thread['values_in']) > 0) or (len(thread['values_out']) > 0)):
					thread['have_values'] = True

	#we can't draw 0 subdivisions
	if(max_counter == 0):
		max_counter = 1

	return max_counter

def get_thread_prio(thread):
	return thread['prio']

def sort_threads_by_prio():
	threads.sort(key=get_thread_prio)

trigger_bar = None

def redraw_trigger_bar(x_nb_values_printed):
	global trigger_bar
	# Redraws the trigger only if we have one value
	if(trigger_time != None):
		trigger_width = VISUAL_WIDTH_TRIGGER * x_nb_values_printed/(fig.get_figwidth()*WINDOWS_DPI)
		if(trigger_bar != None):
			trigger_bar.remove()
		trigger_bar = gnt.broken_barh([(trigger_time - trigger_width/2, trigger_width)], (0, (len(threads_name_list)+1)*SPACING_Y_TICKS), facecolors='red')
	else:
		if(trigger_bar != None):
			trigger_bar.remove()
			trigger_bar = None

# We enable the minor grid only when the zoom is close enough, 
# otherwise the graph is really slow
# We also redraw the trigger bar in a way that its visual width is constant
def on_xlims_change(axes):
	a=axes.get_xlim()
	values_number = a[1]-a[0]

	if(values_number > ZOOM_LEVEL_THRESHOLD):
		gnt.xaxis.set_minor_locator(tick.NullLocator())
	else:
		gnt.xaxis.set_minor_locator(tick.AutoMinorLocator(nb_subdivisions))

	redraw_trigger_bar(values_number)

def read_new_timestamps(event):

	global nb_subdivisions
	global trigger_bar

	flush_shell()

	threads.clear()
	deleted_threads.clear()
	threads_name_list.clear()

	if(trigger_bar != None):
		trigger_bar.remove()
		trigger_bar = None

	gnt.clear()

	print('Getting new data')

	# Sends command "threads_list"
	send_command('threads_list', True)
	rcv_lines = receive_text(True)
	process_threads_list_cmd(rcv_lines)

	# Sends command "threads_timestamps"
	send_command('threads_timestamps', True)
	rcv_lines = receive_text(False)
	nb_subdivisions = process_threads_timestamps_cmd(rcv_lines)

	# sort_threads_by_prio()

	for thread in threads:
		if(thread['have_values']):
			# Splits th names into multiple lines to spare space next to the graph
			threads_name_list.append(thread['name'].replace(' ','\n') + '\nPrio:'+ str(thread['prio']))

	# Setting Y-axis limits 
	#gnt.set_ylim(0, 30) 

	# # Setting X-axis limits 
	# gnt.set_xlim(0, 3000) 

	gnt.set_title('Threads timeline')

	# Setting labels for x-axis and y-axis 
	gnt.set_xlabel('milliseconds since boot')
	gnt.set_ylabel('Threads')

	# Setting ticks on y-axis 
	gnt.set_yticks(range(START_Y_TICKS, (len(threads_name_list)+1)*SPACING_Y_TICKS, SPACING_Y_TICKS))
	# Labelling tickes of y-axis 
	gnt.set_yticklabels(threads_name_list, multialignment='center')

	gnt.xaxis.set_major_locator(tick.MaxNLocator(integer=True, min_n_ticks=0))
	gnt.grid(which='major', color='#000000', linestyle='-')
	gnt.grid(which='minor', color='#CCCCCC', linestyle='--')

	# Setting graph attribute 
	gnt.grid(b = True, which='both')

	# Draws a rectangle every time a thread is running
	row = 0
	for thread in threads:
		if(thread['have_values']):
			y_row = (START_Y_TICKS +  SPACING_Y_TICKS * row) - RECT_HEIGHT/2
			if(thread['log']):
				# If de data are complete (aka this thread was logged), we draw the rectangles
				gnt.broken_barh(thread['values'], (y_row, RECT_HEIGHT), facecolors='blue')
			else:
				# If the data are incomplete (IN and OUT times are missing because this thread wasn't logged),
				# we draw the IN times in Green and the OUT in RED
				gnt.broken_barh(thread['values_in'], (y_row, RECT_HEIGHT), facecolors='green')
				gnt.broken_barh(thread['values_out'], (y_row, RECT_HEIGHT), facecolors='red')
			row += 1

	# Draws the first time the trigger bar
	limits = gnt.axes.get_xlim()
	redraw_trigger_bar(limits[1] - limits[0])

	# Set the callback for the next times (when we move or zoom)
	gnt.callbacks.connect('xlim_changed', on_xlims_change)

	# Updates the toolbar to remove the history on zoom/diplacement and set the new home
	fig.canvas.toolbar.update()

def timestamps_trigger(event):
	# Sends command "threads_stat"
	send_command('threads_timestamps_trigger', True)
	receive_text(True)

def timestamps_run(event):
	# Sends command "threads_stat"
	send_command('threads_timestamps_run', True)
	receive_text(True)

###################              BEGINNING OF PROGRAMM               ###################

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

# # Sends command "threads_stat"
# send_command('threads_stat', False)
# print('Stack usage of the running threads')
# receive_text(True)

# # Sends command "threads_uc"
# send_command('threads_uc', False)
# print('Computation time taken by the running threads')
# receive_text(True)

# Declaring a figure "gnt" 
# figsize is in inch
fig, gnt = plt.subplots(figsize=(WINDOWS_SIZE_X, WINDOWS_SIZE_Y), dpi=WINDOWS_DPI)

plt.subplots_adjust(right=SUBPLOT_ADJ_RIGHT, top=SUBPLOT_ADJ_TOP)

colorAxBlue             	= 'lightblue'
colorAxBlueHovering        	= 'lightblue'
colorAxGreen            	= 'lightgreen'
triggerAx             		= plt.axes([0.6, 0.025, 0.1, 0.04])
runAx             			= plt.axes([0.7, 0.025, 0.1, 0.04])
readAx             			= plt.axes([0.8, 0.025, 0.1, 0.04])
triggerButton             	= Button(triggerAx, 'Set trigger', color=colorAxBlue, hovercolor='0.7')
runButton	             	= Button(runAx, 'Run mode', color=colorAxBlue, hovercolor='0.7')
readButton             		= Button(readAx, 'Get new data', color=colorAxGreen, hovercolor='0.7')

triggerButton.on_clicked(timestamps_trigger)
runButton.on_clicked(timestamps_run)
readButton.on_clicked(read_new_timestamps)

read_new_timestamps(None)

plt.show()

port.close()
print('Port {} closed'.format(sys.argv[1]))

# Be polite, say goodbye :-)
print(GOODBYE)
