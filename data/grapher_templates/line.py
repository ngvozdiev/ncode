import numpy as np
import matplotlib.pylab as plt

for filename, label in {{files_and_labels}}:
    data = np.loadtxt(filename)
    x = data[:,0]
    y = data[:,1]
    plt.plot(x, y, label=label)

ax = plt.gca()
for x_pos, label in {{lines_and_labels}}:
    next_color = ax._get_lines.get_next_color()
    plt.axvline(x_pos, label=label, color=next_color)

plt.title('{{title}}')
plt.xlabel('{{xlabel}}')
plt.ylabel('{{ylabel}}')
plt.legend()
plt.show()
