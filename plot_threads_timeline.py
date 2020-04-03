# Importing the matplotlb.pyplot 
import matplotlib.pyplot as plt 
import sys

RECT_HEIGHT = 10

threads = []

threads.append({'name': 'main','prio': 2, 'values': []})
threads.append({'name': 'idle','prio': 50, 'values': []})


for i in range(2):
	for j in range(50):
		threads[i]['values'].append((20*j + i * RECT_HEIGHT, RECT_HEIGHT))


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
gnt.set_yticklabels([threads[0]['name'],threads[1]['name']]) 

# Setting graph attribute 
gnt.grid(b = True, which='both') 

# Declaring a set of br in the timeline
gnt.broken_barh(threads[0]['values'], (10 - RECT_HEIGHT/2, RECT_HEIGHT), facecolors ='orange') 
# Declaring a set of br in the timeline
gnt.broken_barh([(10,30)], (20 - RECT_HEIGHT/2, RECT_HEIGHT), facecolors ='red') 

plt.show()

