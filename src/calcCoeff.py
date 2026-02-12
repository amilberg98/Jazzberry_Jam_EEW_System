import numpy as np
from scipy import signal

# filter specs
order = 2       # 4th-order Butterworth
fs = 200.00     # sampling rate in Hz
lowcut = 0.1    # lowest relevant frequency in Hz
highcut = 20.0  # highest relelavant frequency in Hz


# Butterworth bandpass
hp = signal.butter(2, lowcut, btype='highpass', fs=fs, output='sos')
lp = signal.butter(2, highcut, btype='lowpass', fs=fs, output='sos')


print("Second-Order Sections (SOS):")
print(hp)
print(lp)


#[[ 0.99778102 -1.99556205  0.99778102  1.         -1.99555712  0.99556697]]
#[[ 0.06745527  0.13491055  0.06745527  1.         -1.1429805   0.4128016 ]]
