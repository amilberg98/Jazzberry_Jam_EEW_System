import numpy as np
import matplotlib.pyplot as plt
from scipy.signal import sosfreqz

# -----------------------
# SETTINGS
# -----------------------
fs = 200  # sampling rate Hz
filename = "device-monitor-260312-170735.log"

# -----------------------
# LOAD DATA
# -----------------------
values = []

with open(filename) as f:
    for line in f:
        line = line.strip()

        if line.startswith("STA") or line.startswith("LTA") or line == "":
            continue

        try:
            values.append(float(line))
        except:
            pass

values = np.array(values)

raw = values[0::2]
filtered = values[1::2]

N = min(len(raw), len(filtered))
raw = raw[:N]
filtered = filtered[:N]

t = np.arange(N)/fs

# -----------------------
# TIME DOMAIN
# -----------------------
plt.figure()
plt.plot(t, raw, label="Raw")
plt.plot(t, filtered, label="Filtered")
plt.xlabel("Time (s)")
plt.ylabel("Acceleration (m/s^2)")
plt.title("Accelerometer Signal")
plt.legend()
plt.show()

# -----------------------
# FFT SPECTRUM
# -----------------------
def compute_fft(signal):
    fft_vals = np.fft.rfft(signal)
    freqs = np.fft.rfftfreq(len(signal), 1/fs)
    mag = np.abs(fft_vals)
    return freqs, mag

freq_raw, mag_raw = compute_fft(raw)
freq_filt, mag_filt = compute_fft(filtered)

plt.figure()
plt.plot(freq_raw, mag_raw, label="Raw")
plt.plot(freq_filt, mag_filt, label="Filtered")
plt.xlabel("Frequency (Hz)")
plt.ylabel("Magnitude")
plt.title("FFT Spectrum")
plt.legend()
plt.xlim(0, 100)
plt.show()

# -----------------------
# FILTER RESPONSE
# -----------------------
# Convert your C++ biquad coefficients into SOS format
sos = np.array([
    [0.99778102, -1.99556205, 0.99778102, 1, -1.99555712, 0.99556697],
    [0.06745527, 0.13491055, 0.06745527, 1, -1.14298050, 0.41280160]
])

w, h = sosfreqz(sos, fs=fs)

plt.figure()
plt.plot(w, 20*np.log10(abs(h)))
plt.xlabel("Frequency (Hz)")
plt.ylabel("Gain (dB)")
plt.title("Filter Frequency Response")
plt.xlim(0,100)
plt.grid()
plt.show()