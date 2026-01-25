import serial
import time
import csv
import matplotlib.pyplot as plt

# === CONFIGURATION ===
PORT = '/dev/cu.usbmodem211401'  # Make same as ESP32
BAUDRATE = 115200
DURATION = 10  # seconds
CSV_FILENAME = 'upDown.csv' # Check save file name

# === INIT SERIAL ===
print("ESP32S3.py")
print("Waiting for Arduino Button Signal...")

ser = serial.Serial(PORT, BAUDRATE, timeout=1)
time.sleep(2)

# === WAIT FOR START ===
while True:
    line = ser.readline().decode().strip()
    if line == "START":
        print("START signal received.")
        break

print("Collecting data...")

# === SETUP PLOTTING ===
plt.ion()
fig, ax = plt.subplots()
line_x, = ax.plot([], [], label='ax')
line_y, = ax.plot([], [], label='ay')
line_z, = ax.plot([], [], label='az')

ax.set_ylim(-2, 2)
ax.set_xlim(0, 10000)  # in milliseconds
ax.set_title("Real-Time Sensor Data")
ax.set_xlabel("Time (ms)")
ax.set_ylabel("Value")
ax.legend()
plt.grid(True)

# === DATA COLLECTION ===
data = []
t_vals, x_vals, y_vals, z_vals = [], [], [], []
start_time = time.time()

while time.time() - start_time < DURATION:
    line = ser.readline().decode().strip()
    if line:
        try:
            values = list(map(float, line.split(',')))
            if len(values) == 3:
                timestamp_ms = int((time.time() - start_time) * 1000)
                data.append([timestamp_ms] + values)

                t_vals.append(timestamp_ms)
                x_vals.append(values[0])
                y_vals.append(values[1])
                z_vals.append(values[2])

                # Update plot
                line_x.set_data(t_vals, x_vals)
                line_y.set_data(t_vals, y_vals)
                line_z.set_data(t_vals, z_vals)
                ax.set_xlim(max(0, timestamp_ms - 10000), timestamp_ms+50)
                ax.relim()
                ax.set_ylim(-2, 2)
                #ax.autoscale_view()
                plt.pause(0.001)

        except ValueError:
            continue

print("Data collection finished.")

# === SAVE TO CSV ===
if data:
    with open(CSV_FILENAME, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['timestamp', 'ax', 'ay', 'az'])
        writer.writerows(data)
    print(f"Saved {len(data)} rows to {CSV_FILENAME}")
else:
    print(" No valid data collected.")

plt.ioff()
plt.show()
