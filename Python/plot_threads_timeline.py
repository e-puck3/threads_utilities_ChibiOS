# File 				: plot_threads_timeline.py 
# Author 			: Eliot Ferragni
# Creation date		: 3 april 2020
# Last modif date	: 25 may 2020
# version			: 1.0
# Brief				: This script gathers the timestamps of each thread to log on an
#					  mcu configured to use the threads_timestamp functions
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
import os
from subprocess import Popen, PIPE

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

# https://developer.apple.com/library/archive/documentation/LanguagesUtilities/Conceptual/MacAutomationScriptingGuide/PromptforaFileName.html
OPEN_FILE_APPLESCRIPT = """the POSIX path of (choose file with prompt "Please choose a txt file:" of type {"TXT"} default location (get path to desktop folder))"""
SAVE_FILE_APPLESCRIPT = """the POSIX path of (choose file name with prompt "Please choose a file:" default name "timestamps.txt" default location (get path to desktop folder))"""
# https://stackoverflow.com/questions/15885132/file-folder-chooser-dialog-from-a-windows-batch-script
# https://docs.microsoft.com/en-us/dotnet/api/system.windows.forms.filedialog?view=netcore-3.1
OPEN_FILE_POWERSHELL = """
Add-Type -AssemblyName System.Windows.Forms
$FileBrowser = New-Object System.Windows.Forms.OpenFileDialog -Property @{ 
    InitialDirectory = [Environment]::GetFolderPath('Desktop') 
    Filter = 'Text Files (*.txt)|*.txt|All Files (*.*)|*.*'
}
$null = $FileBrowser.ShowDialog()
# Writes an error if empty (means we canceled)
if(-Not $FileBrowser.FileName){
	[Console]::Error.WriteLine("Canceled by user")
}else{
	$FileBrowser.FileName	
}
"""
SAVE_FILE_POWERSHELL = """
Add-Type -AssemblyName System.Windows.Forms
$FileBrowser = New-Object System.Windows.Forms.SaveFileDialog -Property @{ 
    InitialDirectory = [Environment]::GetFolderPath('Desktop') 
    FileName = "timestamps.txt"
    Filter = 'Text Files (*.txt)|*.txt|All Files (*.*)|*.*'
}
$null = $FileBrowser.ShowDialog()
# Writes an error if still default filename (means we canceled)
if($FileBrowser.FileName -eq "timestamps.txt"){
	[Console]::Error.WriteLine("Canceled by user")
}else{
	$FileBrowser.FileName
}
"""
# https://help.gnome.org/users/zenity/stable/file-selection.html.en
OPEN_FILE_BASH_LINUX = """
#!bin/bash

FILE=`zenity --file-selection --title="Select a File" --filename="$HOME/Desktop/" 2>&-`

case $? in
         0)
                echo $FILE;;
         1)
                echo No file selected>/dev/stderr;;
        -1)
                echo An unexpected error has occurred>/dev/stderr;;
esac
"""

SAVE_FILE_BASH_LINUX = """
#!bin/bash

FILE=`zenity --file-selection --title="Select a File" --filename="$HOME/Desktop/timestamps.txt" --save  2>&-`

case $? in
         0)
                echo $FILE;;
         1)
                echo No name selected>/dev/stderr;;
        -1)
                echo An unexpected error has occurred>/dev/stderr;;
esac
"""

NEW_RECEIVED_LINE = '> '

FUNC_SUCCESS = True
FUNC_FAILED = False

WINDOW_TITLE 		= 'Threads timeline' 
WINDOWS_SIZE_X 		= 15
WINDOWS_SIZE_Y 		= 10
WINDOWS_DPI			= 90
SUBPLOT_ADJ_LEFT	= 0.12
SUBPLOT_ADJ_RIGHT	= 0.97
SUBPLOT_ADJ_TOP		= 0.96
SUBPLOT_ADJ_BOTTOM	= 0.11

START_Y_TICKS 					= 10
SPACING_Y_TICKS 				= 10
RECT_HEIGHT 					= 10
RECT_HEIGHT_EXIT 				= 15
MINIMUM_THREAD_DURATION 		= 1
VISUAL_WIDTH_TRIGGER			= 5 # no unit
VISUAL_MINIMUM_WIDTH_AUTO_ZOOM  = 260 # the bigger this number is, the smaller the smallest bars appear in auto zoom
MINIMUM_NB_OF_STEPS				= 2
AUTO_ZOOM_WINDOW_MAX_WIDTH 		= 20 # time unit
SUBDIVISION_FACTOR_TICK_STEP 	= 2
ZOOM_LEVEL_THRESHOLD 			= 8

DRAW_BACK 						= 0
DRAW_MIDDLE1 					= 5
DRAW_MIDDLE2					= 10
DRAW_FRONT 						= 15

# for the extracted values fields
REC_THREAD_OUT 		= 0
REC_THREAD_IN		= 1
REC_TIME			= 2
REC_STEP 	 		= 3
REC_NB_OF_STEPS 	= 4

# for the raw_values field of a thread
RAW_TIME 			= 0
RAW_IN_OUT_TYPE		= 1
RAW_STEP_NB			= 2
RAW_NB_OF_STEPS 	= 3

# input possibilities
port = None
serial_connected = False
READ_FROM_SERIAL = 0
READ_FROM_FILE = 1

default_graph_pos = [0, 1, 0, 1]
records = []

threads = []
deleted_threads = []
threads_name_list = []
trigger_time = None
trigger_bar = None

auto_zoom_window_visible = True
auto_zoom_window = None
auto_zoom_window_edges = None
auto_zoom_window_begin = 0
auto_zoom_window_width = 0

text_lines_list = []
text_lines_data = []

def connect_serial():
	global port
	global serial_connected

	if(not serial_connected):
		try:
			print('Connecting to port {}'.format(sys.argv[1]))
			port = serial.Serial(sys.argv[1], timeout=0.1)
			serial_connected = True
		except:
			print('Cannot connect to the device')
			serial_connected = False
			return FUNC_FAILED

	print('Connected')
	return FUNC_SUCCESS

def disconnect_serial():
	global port
	global serial_connected

	if(not serial_connected):
		print('Already disconnected')
	else:
		port.close()
		serial_connected = False
		print('Disconnected')

def toggle_serial(button):
	global port
	# We connect
	if(not serial_connected):
		if(connect_serial() == FUNC_SUCCESS):
			button.label.set_text("Disconnect")
			button.color='lightcoral'
	# We disconnect
	else:
		disconnect_serial()
		button.label.set_text("Connect")
		button.color='lightgreen'

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
		print('Sent :',command)
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
		print('Received:')
		for line in text_lines:
			print(NEW_RECEIVED_LINE, line)

	return text_lines

def clear_data_and_graph():
	global trigger_time
	global trigger_bar
	# Updates the values
	text_lines_list.clear()
	text_lines_data.clear()
	trigger_time = None

	if(trigger_bar != None):
		trigger_bar.remove()
		trigger_bar = None

	gnt.clear()
	threads_name_list.clear()

def append_thread(thread_list, name, nb, prio, log):

	if(log == 'Yes'):	
		# Adds a logged thread to the threads list
		thread_list.append({'name': name,'nb': nb,'prio': prio,'log': True, 'raw_values': [],'have_values': False, 'values': [], 'exit_value': [], 'no_data': []})
	else:
		# Adds a non logged thread to the threads list
		thread_list.append({'name': name,'nb': nb,'prio': prio,'log': False, 'raw_values': [],'have_values': False, 'in_values': [],'out_values': [], 'exit_value': [], 'no_data': []})


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
		return FUNC_FAILED

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

	return FUNC_SUCCESS


def process_threads_timestamps_cmd(lines):

	global records
	# What we should receive (one more line if the trigger mode is enabled) :
	# line 0 			: threads_timestamps
	# (line 1)			: Triggered at xxxxxxx
	# line 1 (2) -> n-1 : From xx to xx at xxxxxxx
	# line n			: ch>

	first_data_line = 1

	# Gets the trigger time if any
	if(lines[first_data_line][:len('Triggered at ')] == 'Triggered at '):
		trigger = int(lines[first_data_line][len('Triggered at '):])
		first_data_line = 2
	# No trigger
	else:
		trigger = None

	# If the received text doesn't match what we expect, print it and quit
	if(lines[first_data_line][:len('From ')] != 'From '):
		print('Bad answer received, see below :')
		for line in lines:
			print(NEW_RECEIVED_LINE, line)
		return 0, FUNC_FAILED
		
	counter = None
	last_time = None
	# A first process of the records to count how many subdivisions we have per timestamp
	for i in range(first_data_line,len(lines)-1):
		time 		= int(lines[i][len('From xx to xx at '):])
		thread_out 	= int(lines[i][len('From '):len('From ')+2])
		thread_in 	= int(lines[i][len('From xx to '):len('From xx to ')+2])

		# Now that we know how many records are during the same timestamp
		# write it back to all the records which happened during this time
		if (time != last_time):
			if(counter != None):
				for j in range(counter+1):
					records[-(1+j)][REC_NB_OF_STEPS] = counter+1
			counter = 0
		else:
			counter += 1 

		last_time = time
		records.append([thread_out, thread_in, time, counter, None])

	# Adds the max_counter to the timestamps of the last time (not done in the loop)
	for j in range(counter+1):
		records[-(1+j)][REC_NB_OF_STEPS] = counter+1

	# Dispatches the records to the correct threads
	# Part where we simulate the deletion of the thread to be correct with the threads numbers
	for record in records:
		thread_out 	= record[REC_THREAD_OUT]
		thread_in 	= record[REC_THREAD_IN]
		time 		= record[REC_TIME]
		step 		= record[REC_STEP]
		nb_of_steps = record[REC_NB_OF_STEPS]

		# A thread has been deleted
		# We simulate the same to be coherent with the numbering of the timestamps
		if(thread_out == thread_in):
			deleted_threads.append(threads.pop(thread_out-1))
			deleted_threads[-1]['raw_values'].append((time, 'exit', step, nb_of_steps))
		else:
			# The line after a thread deletion contains a 0 because the out thread doesn't exist anymore
			# -> ignores the OUT because already written as an EXIT previously
			if(thread_out != 0):
				threads[thread_out-1]['raw_values'].append((time, 'out', step, nb_of_steps))

			threads[thread_in-1]['raw_values'].append((time, 'in', step, nb_of_steps))

	# Now that every timestamps are with their respective threads, we put every threads together
	# again in the same list for the rest of the code
	while(len(deleted_threads)):
		threads.insert(deleted_threads[-1]['nb']-1, deleted_threads.pop(-1))


	for thread in threads:
		if(len(thread['raw_values']) > 0):
			# Thread logged by the MCU
			if(thread['log']):

				if(thread['raw_values'][0][RAW_IN_OUT_TYPE] == 'out'):
					if(thread['raw_values'][0][RAW_TIME] == 0):
						# Insert an IN time in case the first we encounter is an out time and time is 0
						# Happens with the main thread that has no IN time at boot 
						# (no context switch to main since it's the first thread to begin)
						thread['raw_values'].insert(0, (0,'in', 0, 1))
					else:
						# Deletes the first value if it's an OUT time to not mess the timeline
						thread['raw_values'].pop(0)

				# Removes the last value if the number of value is odd
				# because we need a pair of values
				if(len(thread['raw_values']) % 2):
					thread['raw_values'].pop(-1)

				# Adds the values by pair to the thread
				for i in range(0, len(thread['raw_values']), 2):
					step = 1/thread['raw_values'][i][RAW_NB_OF_STEPS]
					# The format for broken_barh needs to be (begin, width)
					shift = thread['raw_values'][i][RAW_STEP_NB] * step
					begin = thread['raw_values'][i][RAW_TIME] + shift
					width = thread['raw_values'][i+1][RAW_TIME] - thread['raw_values'][i][RAW_TIME] - shift
					
					if(width <  1):
						width = step

					thread['values'].append((begin, width))

					# Draws a no data area to show where the first data is on the timeline
					if(i == 0):
						thread['no_data'].append((records[0][REC_TIME], begin - records[0][REC_TIME]))

					# Also draw something when we exit a thread
					if(thread['raw_values'][i+1][RAW_IN_OUT_TYPE] == 'exit'):
						thread['exit_value'].append((begin + step, records[-1][REC_TIME] - (begin + step)))
				# Indicates if we have timestamps to draw
				if(len(thread['values']) > 0):
					thread['have_values'] = True

			# Thread not logged by the MCU
			else:
				# Adds one by one the incomplete values
				for i in range(0, len(thread['raw_values']), 1):
					step = 1/thread['raw_values'][i][RAW_NB_OF_STEPS]
					# size of an IN or OUT tick (for incomplete data)
					tick_step = step/SUBDIVISION_FACTOR_TICK_STEP
					# The format for broken_barh needs to be (begin, width)
					shift = thread['raw_values'][i][RAW_STEP_NB] * step
					begin = thread['raw_values'][i][RAW_TIME] + shift

					if(thread['raw_values'][i][RAW_IN_OUT_TYPE] == 'in'):
						thread['in_values'].append((begin, tick_step))

					elif(thread['raw_values'][i][RAW_IN_OUT_TYPE] == 'out'):
						thread['out_values'].append((begin - tick_step, tick_step))

					# For incomplete data, exiting a thread is drawn the same as an OUT time
					# except for the color
					elif(thread['raw_values'][i][RAW_IN_OUT_TYPE] == 'exit'):
						thread['exit_value'].append((begin - tick_step, records[-1][REC_TIME] - (begin - tick_step)))

					# Draws a no data area to show where the first data is on the timeline
					if(i == 0):
						thread['no_data'].append((records[0][REC_TIME], begin - records[0][REC_TIME]))

				# Indicates if we have timestamps to draw
				if((len(thread['in_values']) > 0) or (len(thread['out_values']) > 0)):
					thread['have_values'] = True

	return trigger, FUNC_SUCCESS

def get_thread_prio(thread):
	return thread['prio']

def sort_threads_by_prio():
	threads.sort(key=get_thread_prio)

def show_all_data_graph(event):
	gnt.axes.set_xlim(default_graph_pos[0], default_graph_pos[1])
	gnt.axes.set_ylim(default_graph_pos[2], default_graph_pos[3])
	plt.draw()
	print('Now showing all data')

def auto_zoom_data_graph(event):
	a=gnt.axes.get_xlim()
	actual_x_pos = (a[1] + a[0]) / 2

	window_begin = actual_x_pos - auto_zoom_window_width/2
	window_end = actual_x_pos + auto_zoom_window_width/2

	# Searches for the maximum number of steps present inside the auto zoom window
	# This defines the zoom level in order to show correctly the smallest bars present
	number_of_steps = MINIMUM_NB_OF_STEPS
	for record in records:
		if record[REC_TIME] <= window_end and record[REC_TIME] >= window_begin:
			if record[REC_NB_OF_STEPS] > number_of_steps:
				number_of_steps = record[REC_NB_OF_STEPS]

	half_visual_auto_zoom_area = VISUAL_MINIMUM_WIDTH_AUTO_ZOOM / number_of_steps / 2

	gnt.axes.set_xlim(actual_x_pos-half_visual_auto_zoom_area, actual_x_pos+half_visual_auto_zoom_area)
	plt.draw()
	print('Auto zoom done')

def redraw_auto_zoom_window(x_nb_values_printed, x_pos):
	global auto_zoom_window
	global auto_zoom_window_edges
	global auto_zoom_window_width
	global auto_zoom_window_visible

	if(auto_zoom_window_visible):
		if(auto_zoom_window != None):
			auto_zoom_window.remove()
			auto_zoom_window_edges.remove()
		# Prints an area to show the auto zoom window
		# Resizes the window in order to never be bigger than one third of the timeline
		auto_zoom_window_width = AUTO_ZOOM_WINDOW_MAX_WIDTH
		if(auto_zoom_window_width >= x_nb_values_printed/3):
			auto_zoom_window_width = x_nb_values_printed/3
		elif(auto_zoom_window_width < x_nb_values_printed/12):
			auto_zoom_window_width = x_nb_values_printed/12

		# We need to draw two different objects. One on top of everything for the edges and one behind everything for the infill
		auto_zoom_window		= gnt.barh(0, auto_zoom_window_width, (len(threads_name_list)+1)*SPACING_Y_TICKS, x_pos-auto_zoom_window_width/2, align='edge', color='0.95', zorder=DRAW_BACK)
		auto_zoom_window_edges 	= gnt.barh(0, auto_zoom_window_width, (len(threads_name_list)+1)*SPACING_Y_TICKS, x_pos-auto_zoom_window_width/2, align='edge', edgecolor='0', linewidth=1, fill=False, zorder=DRAW_FRONT)
	else:
		if(auto_zoom_window != None):
			auto_zoom_window.remove()
			auto_zoom_window_edges.remove()
			auto_zoom_window = None

def toggle_auto_zoom_window(button):
	global auto_zoom_window_visible

	xlimits = gnt.axes.get_xlim()
	ylimits = gnt.axes.get_ylim()

	# Never draws the auto zoom window if we have no data
	# Updates the button
	if(len(text_lines_data) > 0):
		if button.label.get_text() == 'Hide auto zoom window':
			button.label.set_text('Show auto zoom window')
			button.color='lightcoral'
			auto_zoom_window_visible = False
		else:
			button.label.set_text('Hide auto zoom window')
			button.color='lightgreen'
			auto_zoom_window_visible = True

		redraw_auto_zoom_window(xlimits[1] - xlimits[0], (xlimits[1] + xlimits[0]) / 2)


def redraw_trigger_bar(x_nb_values_printed):
	global trigger_bar
	# Redraws the trigger only if we have one value
	if(trigger_time != None):
		trigger_width = VISUAL_WIDTH_TRIGGER * x_nb_values_printed/(fig.get_figwidth()*WINDOWS_DPI)
		if(trigger_bar != None):
			trigger_bar.remove()
		trigger_bar = gnt.broken_barh([(trigger_time - trigger_width/2, trigger_width)], (0, (len(threads_name_list)+1)*SPACING_Y_TICKS), facecolors='red', zorder=DRAW_FRONT)
	else:
		if(trigger_bar != None):
			trigger_bar.remove()
			trigger_bar = None

# We redraw the trigger bar in a way that its visual width is constant
def on_xlims_change(axes):
	a=axes.get_xlim()
	nb_values_printed = a[1]-a[0]
	actual_x_pos = (a[1] + a[0]) / 2
	redraw_trigger_bar(nb_values_printed)
	redraw_auto_zoom_window(nb_values_printed, actual_x_pos)

# Only for MacOS
def exec_applescript(script):
	p = Popen(['osascript', '-e', script], stdout=PIPE, stderr=PIPE)
	out = p.stdout.read()
	err = p.stderr.read()
	out = out.decode("utf-8")
	out = out.replace('\n', '')
	err = err.decode("utf-8")
	err = err.replace('\n', '')
	return out, err

# Only for Windows
def exec_powershell(script):
	p = Popen(['powershell.exe', script], stdout=PIPE, stderr=PIPE)
	out = p.stdout.read()
	err = p.stderr.read()
	out = out.decode("utf-8")
	out = out.replace('\r\n', '')
	err = err.decode("utf-8")
	err = err.replace('\r\n', '')
	return out, err

# Only for Linux
def exec_bash(script):
	p = Popen([script], shell = True, stdout=PIPE, stderr=PIPE)
	out = p.stdout.read()
	err = p.stderr.read()
	out = out.decode("utf-8")
	out = out.replace('\n', '')
	err = err.decode("utf-8")
	err = err.replace('\n', '')
	return out, err

def split_file_name_extension(file_path):
	# Splits the name and the extension of the file
	file_name = file_path
	while(True):
		file_name, extension = os.path.splitext(file_name)
		if(len(extension) == 0):
			break
	extension = file_path[len(file_name):]

	return file_name, extension

def write_timestamps_to_file():

	if(len(text_lines_data) == 0):
		print('No data to save !')
		return

	# MACOS
	if(sys.platform == 'darwin'):
		file_path, error = exec_applescript(SAVE_FILE_APPLESCRIPT)
	# Windows
	elif(sys.platform == 'win32'):
		file_path, error = exec_powershell(SAVE_FILE_POWERSHELL)
	# Linux
	elif(sys.platform == 'linux'):
		file_path, error = exec_bash(SAVE_FILE_BASH_LINUX)
	else:
		error = 'Your OS is not supported'

	if(len(error) > 0):
		print('File not saved')
		print('Error:', error)
		return

	# Splits the name and the extension of the file
	file_name, extension = split_file_name_extension(file_path)

	# Adds the txt extension if not present
	if(extension != '.txt'):
		extension += '.txt'
		print('.txt automatically added to the file name')
		# Adds a number to the name if the file already exists
		# Only if we added the .txt extension
		# Otherwise we replace an already existing file because it was warned
		# with the file name selection window
		i = 2
		if(os.path.exists(file_name + extension)):
			while(os.path.exists(file_name + str(i) + extension)):
				i += 1
			print('but', file_name + extension, 'already exists')
			file_name += str(i)
			print('Renamed to ', file_name + extension)

	file_path = file_name + extension

	# Opens the file as Write Text
	file = open(file_path,'wt')

	# Writes the position in the graph
	xlim = gnt.axes.get_xlim()
	ylim = gnt.axes.get_ylim()
	file.write('Saved position\n')
	file.write(str(xlim[0]) + '\n' + str(xlim[1]) + '\n' + str(ylim[0]) + '\n' + str(ylim[1]) + '\n')

	# Writes the lines to the file
	for line in text_lines_list:
		file.write(line + '\n')
	for line in text_lines_data:
		file.write(line + '\n')

	file.close()
	print(file_path, 'Saved !')


def load_timestamps_from_file():
	file_path = ''
	# MACOS
	if(sys.platform == 'darwin'):
		file_path, error = exec_applescript(OPEN_FILE_APPLESCRIPT)
	# Windows
	elif(sys.platform == 'win32'):
		file_path, error = exec_powershell(OPEN_FILE_POWERSHELL)
	# Linux
	elif(sys.platform == 'linux'):
		file_path, error = exec_bash(OPEN_FILE_BASH_LINUX)
	else:
		error = 'Your OS is not supported'

	if(len(error) > 0):
		print('Error:', error)
		error = True
	else:
		error = False

		try:
			# Opens the file as Read Text
			file = open(file_path, 'rt')
		except:
			print("File doesn't exist")
			error = True

	if(not error):
		print('Loading file ',file_path)
		rcv_text = file.read()
		file.close()
		rcv_lines = rcv_text.split('\n')
		index_pos = None
		index_list = None
		index_data = None

		# j is used to correct the index in the case we remove an empty line
		j = 0
		for i in range(len(rcv_lines)):
			# Removes empty lines
			if(len(rcv_lines[i-j]) == 0):
				rcv_lines.pop(i-j)
				j += 1
				continue
			# Searches for the beginning of the Saved position fields
			if((index_pos == None) and (rcv_lines[i-j].find('Saved position') != -1)):
				index_pos = i-j
			# Searches for the beginning of the thread_list fields
			if((index_list == None) and (rcv_lines[i-j].find('threads_list') != -1)):
				index_list = i-j
			#searches for the beginning of the threads_timestamps fields
			elif((index_data == None) and (rcv_lines[i-j].find('threads_timestamps') != -1)):
				index_data = i-j

		# Tests if the file ends with ch> 
		if(rcv_lines[-1] != 'ch> '):
			error = True

		# Tests if we found all the fields
		if(index_pos == None or index_list == None or index_data == None):
			error = True
		else:
			# Removes the Saved position for the rcv_lines_pos list
			rcv_lines_pos  = rcv_lines[index_pos + 1:index_list]
			rcv_lines_list = rcv_lines[index_list:index_data]
			rcv_lines_data = rcv_lines[index_data:]

	if(not error):
		print('File loaded')
		return rcv_lines_pos, rcv_lines_list, rcv_lines_data
	else:
		return [], [file_path,'File not recognized'], []


def read_new_timestamps(input_src):

	global trigger_bar
	global trigger_time
	global text_lines_list
	global text_lines_data
	global serial_connected
	global default_graph_pos
	global records

	threads.clear()
	deleted_threads.clear()
	records.clear()

	if(input_src == READ_FROM_SERIAL):

		if(not serial_connected):
			print('Serial not connected')
			return

		flush_shell()

		print('Getting new data from serial\n')

		# Sends command "threads_list"
		send_command('threads_list', True)
		lines_list = receive_text(True)
		result = process_threads_list_cmd(lines_list)
		if(result == FUNC_FAILED):
			return

		# Sends command "threads_timestamps"
		send_command('threads_timestamps', True)
		lines_data = receive_text(False)
		trigger, result = process_threads_timestamps_cmd(lines_data)
		if(result == FUNC_FAILED):
			return

		# We only read a saved position from a file
		lines_pos = None

	elif(input_src == READ_FROM_FILE):
		lines_pos, lines_list, lines_data = load_timestamps_from_file()

		result = process_threads_list_cmd(lines_list)
		if(result == FUNC_FAILED):
			return

		trigger, result = process_threads_timestamps_cmd(lines_data)
		if(result == FUNC_FAILED):
			return

	clear_data_and_graph()

	# Updates the values
	text_lines_list = lines_list
	text_lines_data = lines_data
	trigger_time = trigger

	sort_threads_by_prio()

	for thread in threads:
		if(thread['have_values']):
			# Splits th names into multiple lines to spare space next to the graph
			threads_name_list.append(thread['name'].replace(' ','\n') + '\nPrio:'+ str(thread['prio']))

	print('New data received, redrawing the timeline')

	gnt.set_title('Threads timeline')

	# Setting labels for x-axis and y-axis 
	gnt.set_xlabel('System ticks since boot')
	gnt.set_ylabel('Threads')

	# Setting ticks on y-axis 
	gnt.set_yticks(range(START_Y_TICKS, (len(threads_name_list)+1)*SPACING_Y_TICKS, SPACING_Y_TICKS))
	# Labeling ticks of y-axis 
	gnt.set_yticklabels(threads_name_list, multialignment='center')

	gnt.xaxis.set_major_locator(tick.MaxNLocator(integer=True, min_n_ticks=0))
	gnt.xaxis.get_major_formatter().set_useOffset(False)
	gnt.grid(which='major', color='#000000', linestyle='-', zorder=DRAW_FRONT)
	gnt.grid(which='minor', color='#CCCCCC', linestyle='--')

	# Setting graph attribute 
	gnt.grid(b = True, which='both')

	# Draws a rectangle every time a thread is running
	row = 0
	for thread in threads:
		if(thread['have_values']):
			y_row = (START_Y_TICKS +  SPACING_Y_TICKS * row) - RECT_HEIGHT/2
			# Grey area to tell where the first data is
			gnt.broken_barh(thread['no_data'], (y_row, RECT_HEIGHT), facecolors='0.7', alpha=0.5, zorder=DRAW_MIDDLE1)
			# Red area to tell the thread is ended
			gnt.broken_barh(thread['exit_value'], (y_row, RECT_HEIGHT), facecolors='red', alpha=0.5, zorder=DRAW_MIDDLE1)

			if(thread['log']):
				# If de data are complete (aka this thread was logged), we draw the rectangles
				gnt.broken_barh(thread['values'], (y_row, RECT_HEIGHT), facecolors='blue', zorder=DRAW_MIDDLE2)
			else:
				# If the data are incomplete (IN and OUT times are missing because this thread wasn't logged),
				# we draw the IN times in Green and the OUT in RED
				gnt.broken_barh(thread['in_values'], (y_row, RECT_HEIGHT), facecolors='green', zorder=DRAW_MIDDLE2)
				gnt.broken_barh(thread['out_values'], (y_row, RECT_HEIGHT), facecolors='red', zorder=DRAW_MIDDLE2)
			row += 1

	# Draws the first time the trigger bar
	xlimits = gnt.axes.get_xlim()
	ylimits = gnt.axes.get_ylim()
	redraw_trigger_bar(xlimits[1] - xlimits[0])
	# Then draws the first auto zoom window. Need to take the new limits because the trigger
	# bar may have changed them
	xlimits = gnt.axes.get_xlim()
	ylimits = gnt.axes.get_ylim()
	redraw_auto_zoom_window(xlimits[1] - xlimits[0], (xlimits[1] + xlimits[0]) / 2)

	# Saves the default position
	default_graph_pos = [xlimits[0], xlimits[1], ylimits[0], ylimits[1]]

	# Set the callback for the next times (when we move or zoom)
	gnt.callbacks.connect('xlim_changed', on_xlims_change)

	# Updates the tool-bar to remove the history on zoom/displacement and set the new home
	fig.canvas.toolbar.update()

	# Updates the position in the graph if read from a file
	if(lines_pos != None):
		gnt.axes.set_xlim(float(lines_pos[0]), float(lines_pos[1]))
		gnt.axes.set_ylim(float(lines_pos[2]), float(lines_pos[3]))

	plt.draw()
	print('Drawing finished')

def timestamps_trigger(event):
	if(not serial_connected):
		print('Serial not connected')
		return
	# Sends command "threads_stat"
	send_command('threads_timestamps_trigger', True)
	receive_text(True)

def timestamps_run(event):
	if(not serial_connected):
		print('Serial not connected')
		return
	# Sends command "threads_stat"
	send_command('threads_timestamps_run', True)
	receive_text(True)

###################              BEGINNING OF PROGRAMM               ###################

# Tests if the serial port as been given as argument in the terminal
if (len(sys.argv) == 1):
	print('No serial port given')
	print('To use the serial, provide the serial port as argument')
	serial_port_given = False
else:
	if(len(sys.argv) > 2):
		print('Too many arguments given. Only the first one will be used')
	serial_port_given = True

# Declaring a figure "gnt" 
# figsize is in inch
fig, gnt = plt.subplots(figsize=(WINDOWS_SIZE_X, WINDOWS_SIZE_Y), dpi=WINDOWS_DPI, num=WINDOW_TITLE)

plt.subplots_adjust(left = SUBPLOT_ADJ_LEFT, right=SUBPLOT_ADJ_RIGHT, top=SUBPLOT_ADJ_TOP, bottom = SUBPLOT_ADJ_BOTTOM)

loadAx 						= plt.axes([0.12, 0.025, 0.08, 0.04])
saveAx 						= plt.axes([0.20, 0.025, 0.08, 0.04])
zoomAx 						= plt.axes([0.325, 0.025, 0.08, 0.04])
showAllAx 					= plt.axes([0.405, 0.025, 0.08, 0.04])

showAutoZoomAx 				= plt.axes([0.325, 0.002, 0.16, 0.02])

loadButton 					= Button(loadAx, 'Load file', color='lightblue', hovercolor='0.7')
saveButton					= Button(saveAx, 'Save file', color='lightblue', hovercolor='0.7')
zoomButton 					= Button(zoomAx, 'Time Auto zoom', color='lightblue', hovercolor='0.7')
showAllButton 				= Button(showAllAx, 'Show all data', color='lightblue', hovercolor='0.7')

showAutoZoomButton 			= Button(showAutoZoomAx, 'Hide auto zoom window', color='lightgreen', hovercolor='0.7')

zoomButton.on_clicked(auto_zoom_data_graph)
showAllButton.on_clicked(show_all_data_graph)
loadButton.on_clicked(lambda x: read_new_timestamps(READ_FROM_FILE))
saveButton.on_clicked(lambda x: write_timestamps_to_file())

showAutoZoomButton.on_clicked(lambda x: toggle_auto_zoom_window(showAutoZoomButton))

if(serial_port_given):
	triggerAx             		= plt.axes([0.53, 0.025, 0.1, 0.04])
	runAx             			= plt.axes([0.63, 0.025, 0.1, 0.04])
	readAx             			= plt.axes([0.77, 0.025, 0.1, 0.04])
	connectionAx 				= plt.axes([0.87, 0.025, 0.1, 0.04])

	triggerButton             	= Button(triggerAx, 'Trigger', color='lightcoral', hovercolor='0.7')
	runButton	             	= Button(runAx, 'Run', color='lightgreen', hovercolor='0.7')
	readButton             		= Button(readAx, 'Get new data', color='lightgreen', hovercolor='0.7')
	connectionButton 			= Button(connectionAx, 'Connect', color='lightgreen', hovercolor='0.7')

	triggerButton.on_clicked(timestamps_trigger)
	runButton.on_clicked(timestamps_run)
	readButton.on_clicked(lambda x: read_new_timestamps(READ_FROM_SERIAL))
	connectionButton.on_clicked(lambda x: toggle_serial(connectionButton))

# Auto select the pan/zoom tool from the toolbar for convenience
plt.get_current_fig_manager().toolbar.pan()

plt.show()

disconnect_serial()
# Be polite, say goodbye :-)
print(GOODBYE)
