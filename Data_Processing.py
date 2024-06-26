#%%

# Code development was assisted by ChatGPT

import wave
import struct
import numpy as np

def read_adc_data(filename):
    # Open the data.txt file
    with open(filename, 'r') as file:
        lines = file.readlines()

    # Initialize
    adc_data = []
    sampling_rate_changes = []
    current_rate = 44100  # Default sampling rate

    # This parses the text file to extract the samples and sampling rates
    for line in lines:
        line = line.strip()
        # Extract the sampling rates
        if 'Hz:' in line:
            print("Detected potential sampling rate line:", line)
            rate_str, values_str = line.split(':')
            rate_str = rate_str.replace('Hz', '').strip()
            current_rate = int(rate_str)
            sampling_rate_changes.append((len(adc_data), current_rate))
            values = [int(val) for val in values_str.split() if val.isdigit()]
            adc_data.extend(values)
        # Store the rest as the ADC readings
        else:
            values = [int(val) for val in line.split() if val.isdigit()]
            adc_data.extend(values)

    print(f"Parsed {len(adc_data)} data points.")
    return adc_data, sampling_rate_changes

def remove_dc_offset(adc_data):
    dc_offset = np.mean(adc_data) # Finds the average value
    return [sample - dc_offset for sample in adc_data] # returns the ADC readings - the DC offset

def convert_to_audio_signal(adc_data, max_adc_value=1023): 
    return [int(((sample / max_adc_value) * 65535) - 32768) for sample in adc_data]  # Adjust to 16-bit range

def write_to_wav(audio_data, filename, sampling_rate):
    # Open the .wav file and initialize all the necessary values to create it
    with wave.open(filename, 'w') as wav_file:
        num_channels = 1
        sampwidth = 2  # 2 bytes for 16-bit audio
        n_frames = len(audio_data)
        comptype = 'NONE'
        compname = 'not compressed'

        wav_file.setparams((num_channels, sampwidth, sampling_rate, n_frames, comptype, compname))
        
        # Input the ADC readings into the .wav file
        for sample in audio_data:
            clamped_sample = max(min(sample, 32767), -32768)  # Clamp to valid range
            data = struct.pack('<h', clamped_sample)  # 16-bit PCM
            wav_file.writeframes(data)

# Use all the functions to turn data.txt into a .wav file
adc_data, sampling_rate_changes = read_adc_data("DATA.TXT")
adc_data = remove_dc_offset(adc_data)
audio_data = convert_to_audio_signal(adc_data)

# Determine the most common sampling rate
common_rate = max(set(rate for _, rate in sampling_rate_changes), key=lambda rate: sum(1 for _, r in sampling_rate_changes if r == rate))
print(f"Common sampling rate: {common_rate}")

write_to_wav(audio_data, "output.wav", common_rate)


#%%
