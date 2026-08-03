import os

# ------------------------------------------------------------------------------------------------------
# This code writes the summary for the SPS data into a more organized manner
# SPS summmary includes the sipm index, corrected/uncorrected breakdown voltage values, slope, and y-int
# ------------------------------------------------------------------------------------------------------

# Insert the folder with the trays
folder_with_trays = "/Users/mekail/Documents/Wright Lab/Michael's Code/production/robot_production"

trays = [
    f for f in os.listdir(folder_with_trays)
    if os.path.isdir(os.path.join(folder_with_trays, f))
]

for tray in trays:

    # Tray folder
    tray_folder = os.path.join(folder_with_trays, tray)

    # Debrecen Folder
    debrecen_folder = os.path.join(tray_folder, 'debrecen')

    # Make a folder called summaries if it doesn't already exist
    summary_folder = os.path.join(tray_folder, 'summary')
    os.makedirs(summary_folder, exist_ok = True)

    # Use the only numbers.txt to write an SPS summary file
    SPS_input_file = os.path.join(debrecen_folder, "SPS_result_onlynumbers.txt")
    Tray_ID = SPS_input_file.split('/')[-3]
    
    SPS_output_file = os.path.join(summary_folder, "SPS summary")

    print(f"Writing the SPS summary for {Tray_ID}")

    # Open the SPS file with data
    with open(SPS_input_file, 'r') as f:
        lines = f.readlines()
    
    # Dictionary for the relevant data
    sps_data = {} # {SiPM_index: VBD1, VBD2, Avg_Temp, slope, y-intercept}
    for line in lines:
        parts = line.split()

        # Extract and calculate the SiPM ID
        SiPMpos = parts[0].split('_')
        row, col = float(SiPMpos[-2]),float(SiPMpos[-1])
        SIPMindex = int((20 * row) + col)

        spsVBD1 = float(parts[3]) # Raw Breakdown Voltage
        avg_temp = float(parts[4]) # Average Temperature
        temp_uncertainty = float(parts[5]) # Uncertainty of the Temperature
        spsVBD2 = float(parts[6]) # Breakdown Voltage @ 25°C
        y_intercept = float(parts[9]) # y-int for the gain line
        slope = float(parts[10]) # slope for the gain line
        sps_data[SIPMindex] = spsVBD1, spsVBD2, avg_temp, y_intercept, slope

        # Sorts the dictionary in ascending sipm index
        sps_data = {k: sps_data[k] for k in sorted(sps_data)}

    # Writes the SPS summary file
    with open(SPS_output_file, 'w') as f:
        # Title
        f.write("=== Summary of SPS Results ===\n")

        # Header
        f.write("\n[Header]\n")
        f.write(f"Tray ID: {Tray_ID}\n")

        # Breakdown Voltages from SPS 
        f.write("\n[Breakdown Voltage (V)]\n")
        f.write("Index \t Raw_VBD \t VBD @ 25°C \t Avg Temp °C\t y-int \t\t slope \n")
        f.write("-" * 80 + '\n')

        for sipm_index, (vbd1, vbd2, avg_temp, slope, y_int) in sps_data.items():
            # Using string formatting ensures columns stay perfectly aligned 
            # even when index numbers change from 1 to 3 digits (e.g., index 9 vs 399)
            f.write(f"{sipm_index:}\t {vbd1:.4f}\t {vbd2:.4f}\t {avg_temp:.2f}\t\t {slope:.2f}\t {y_int} \n")
    print(f"The SPS summary for {Tray_ID} is complete\n")


"""
# This writes the SPS summary for one tray folder instead of an entire folder of trays
tray_folder = "/Users/mekail/Documents/Wright Lab/Michael's Code/Trays/260212-1317"

# Debrecen Folder
debrecen_folder = os.path.join(tray_folder, 'debrecen')

# Make a folder called summaries if it doesn't already exist
summary_folder = os.path.join(tray_folder, 'summary')
os.makedirs(summary_folder, exist_ok = True)

# Use the only numbers.txt to write an SPS summary file
SPS_input_file = os.path.join(debrecen_folder, "SPS_result_onlynumbers.txt")
Tray_ID = SPS_input_file.split('/')[-3]

SPS_output_file = os.path.join(summary_folder, "SPS summary")

print(f"Writing the SPS summary for {Tray_ID}")

# Open the SPS file with data
with open(SPS_input_file, 'r') as f:
    lines = f.readlines()

# Dictionary for the relevant data
sps_data = {} # {SiPM_index: VBD1, VBD2, Avg_Temp, slope, y-intercept}
for line in lines:
    parts = line.split()

        # Extract and calculate the SiPM ID
    SiPMpos = parts[0].split('_')
    row, col = float(SiPMpos[-2]),float(SiPMpos[-1])
    SIPMindex = int((20 * row) + col)

    spsVBD1 = float(parts[3]) # Raw Breakdown Voltage
    avg_temp = float(parts[4]) # Average Temperature
    temp_uncertainty = float(parts[5]) # Uncertainty of the Temperature
    spsVBD2 = float(parts[6]) # Breakdown Voltage @ 25°C
    y_intercept = float(parts[9]) # y-int for the gain line
    slope = float(parts[10]) # slope for the gain line
    sps_data[SIPMindex] = spsVBD1, spsVBD2, avg_temp, y_intercept, slope


    # Sorts the dictionary in ascending sipm index
    sps_data = {k: sps_data[k] for k in sorted(sps_data)}

with open(SPS_output_file, 'w') as f:
# Title
f.write("=== Summary of SPS Results ===\n")

# Header
f.write("\n[Header]\n")
f.write(f"Tray ID: {Tray_ID}\n")

# Breakdown Voltages from SPS 
f.write("\n[Breakdown Voltage (V)]\n")
f.write("Index \t Raw_VBD \t VBD @ 25°C \t Avg Temp °C\t y-int \t\t slope \n")
f.write("-" * 80 + '\n')

for sipm_index, (vbd1, vbd2, avg_temp, slope, y_int) in sps_data.items():
    # Using string formatting ensures columns stay perfectly aligned 
    # even when index numbers change from 1 to 3 digits (e.g., index 9 vs 399)
    f.write(f"{sipm_index:}\t {vbd1:.4f}\t {vbd2:.4f}\t {avg_temp:.2f}\t\t {slope:.2f}\t {y_int} \n")
print(f"The SPS summary for {Tray_ID} is complete\n")
"""