import os
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import numpy as np


# -----------------------------------------------------------------------------------------------------
# This code analyzes the repeated measurements of tray 250821-1302
# It generates a histogram of each method by subtracting the mean across five runs with individual runs
# -----------------------------------------------------------------------------------------------------

# The folder with the repeated measurements
test_folders = [
    "/Users/mekail/Documents/Wright Lab/Michael's Code/RepeatedTrays/250821-1302",
    "/Users/mekail/Documents/Wright Lab/Michael's Code/RepeatedTrays/250821-1302-test2",
    "/Users/mekail/Documents/Wright Lab/Michael's Code/RepeatedTrays/250821-1302-test3",
    "/Users/mekail/Documents/Wright Lab/Michael's Code/RepeatedTrays/250821-1302-test4",
    "/Users/mekail/Documents/Wright Lab/Michael's Code/RepeatedTrays/250821-1302-test5"
]

Debrecen_results = "/Users/mekail/Documents/Wright Lab/Michael's Code/RepeatedTrays/250821-1302-test4/debrecen/IV_result.txt"

# Dictionary to group measurements by SiPM ID
# Structure: {sipm_num: [val_test1, val_test2, ..., val_test5]}
sipm_RD = {}
sipm_Inv = {}
sipm_Sec = {}
sipm_Para = {}
sipm_Tan = {}


# Loop through each of the folders
for folder_path in test_folders:
    summary_file  = os.path.join(folder_path, "summary", "All Methods")
    debrecen_file = os.path.join(folder_path, "debrecen", "IV_result.txt")

    # My method
    with open(summary_file, 'r') as f:
        lines = f.readlines()

    for line in lines:
        parts = line.split()
        if len(parts) == 7:
            try:
                sipm_id = float(parts[0])
                VBD_RD  = float(parts[1])
                VBD_Inv = float(parts[2])
                VBD_Sec = float(parts[3])
                VBD_Tan = float(parts[4])
                VBD_Para = float(parts[5])
                avg_temp = float(parts[6])

                # Temperature Corrections
                temp_diff = 25 - avg_temp
                VBD_RD = VBD_RD + temp_diff * 0.037
                VBD_Inv = VBD_Inv + temp_diff * 0.036
                VBD_Sec = VBD_Sec + temp_diff * 0.036
                VBD_Tan = VBD_Tan + temp_diff * 0.036
                VBD_Para = VBD_Para + temp_diff * 0.036

                for target_dict in [sipm_RD, sipm_Inv, sipm_Sec, sipm_Tan, sipm_Para]:
                    if sipm_id not in target_dict:
                        target_dict[sipm_id] = []
                
                # Append the 5 values to a dictionary
                sipm_RD[sipm_id].append(VBD_RD)
                sipm_Inv[sipm_id].append(VBD_Inv)
                sipm_Sec[sipm_id].append(VBD_Sec)
                sipm_Tan[sipm_id].append(VBD_Tan)
                sipm_Para[sipm_id].append(VBD_Para)

    
            except ValueError:
                continue

# Gridspec so that the bottom two plots are centered
fig = plt.figure(figsize=(15,9))
gs = gridspec.GridSpec(2, 6, figure=fig, hspace = 0.3, wspace = 0.7)

ax0 = fig.add_subplot(gs[0, 0:2])
ax1 = fig.add_subplot(gs[0, 2:4])
ax2 = fig.add_subplot(gs[0, 4:6])
ax3 = fig.add_subplot(gs[1, 1:3])
ax4 = fig.add_subplot(gs[1, 3:5])
axes_list = [ax0, ax1, ax2, ax3, ax4]


method_config = [
    {"name": "Relative Derivative", "dict": sipm_RD},
    {"name": "Inverse Derivative", "dict": sipm_Inv},
    {"name": "Second Derivative", "dict": sipm_Sec},
    {"name": "Tangent", "dict": sipm_Tan},
    {"name": "Parabolic", "dict": sipm_Para}
    ]
colors = ['blue', 'orange', 'green', 'red', 'purple']
# Loop to calculate deviations for each sipm for each method
# Plots in a 2 x 3 grid with the bottom two plots being centered
for i, method in enumerate(method_config):
    method_name = method["name"]
    target_dict = method["dict"]
    
    if not target_dict:
        continue
    
    all_deviations = []

    # Loop to call the dictionaries for the methods
    for sipm_id, values in target_dict.items():
        if len(values) > 0:
            mean_vbd = np.mean(values)
            deviations = np.array(values) - mean_vbd
            all_deviations.extend(deviations)
    all_deviations = np.array(all_deviations) * 1000
    stdDev = np.std(all_deviations)

    ax = axes_list[i]
    
    # Plotting how the method compares systematically
    ax.hist(all_deviations, bins = 20, edgecolor='black', color = colors[i])
    ax.set_xlabel("Differences (mV)")
    ax.set_ylabel("counts")
    ax.set_title(f"{method_name}", fontweight = 'bold')
    ax.set_xlim(-15, 15)
    ax.grid(axis='y', linestyle='--', alpha=0.4)
    ax.set_ylim(0, 500)

    # Create a textbox with standard deviation values
    box_text = f"$\sigma$ = {stdDev:.1f} mV"
    
   
    box_style = dict(
        boxstyle='round,pad=0.5',
        facecolor='white',
        edgecolor='gray',
        alpha=0.85
    )
    
    ax.text(
        0.05, 0.95,               
        box_text, 
        transform=ax.transAxes,   
        fontsize=10, 
        verticalalignment='top', 
        horizontalalignment='left',
        bbox=box_style)

    

# fig.suptitle("Method Stability Across Repeated Measurements", fontweight = 'bold')
# fig.text(0.5, 0.92, "Tray 250821-1302\n Hamamatsu SP14160-1315PS", horizontalalignment = 'center', style = 'italic')
#plt.tight_layout(rect=[0, 0, 1, 0.92])
plt.tight_layout()
plt.show()







# All of this is old code
"""
sipm_means = {}
sipm_deviations = {}

methods = ""
# My method
for sipm_id, VBD_RD in sipm_history.items():

    values = sipm_history[sipm_id]
  
    #print(sipm_history[sipm_id])

    if len(values) > 0:
        mean_vbd = np.mean(values)
        #print(f"The average breakdown voltage for {SiPMDnum} is {mean_vbd:.4f}")
        #sipm_means[sipm_id] = mean_vbd
        # print(f"{np.array(values[0])} - {mean_vbd}) = {np.array(values[0]) - mean_vbd}")
        # print(f"{np.array(values[1])} - {mean_vbd}) = {np.array(values[1]) - mean_vbd}")
        # print(f"{np.array(values[2])} - {mean_vbd}) = {np.array(values[2]) - mean_vbd}")
        #print(f"{np.array(values[3])} - {mean_vbd}) = {np.array(values[3]) - mean_vbd}")
        #print(f"{np.array(values[4])} - {mean_vbd}) = {np.array(values[4]) - mean_vbd}")
              
        #print(f" {sipm_history[sipm_id]} - {mean_vbd} ")
        sipm_deviations[sipm_id] = (np.array(values) - mean_vbd)
    print(sipm_deviations[sipm_id])
"""
"""

#print(sipm_deviations)
        #print(f"The difference between the mean of {sipm_id} and its value is {sipm_deviations}")

all_deviations = []
for deviations in sipm_deviations.values():
    all_deviations.extend(deviations)
stdDev = np.std(all_deviations)
print("standard deviation:", np.std(all_deviations))

#print(all_deviations)

plt.figure(figsize=(8, 5))
plt.text(0.05, 0.95, f"std = {stdDev}", transform=plt.gca().transAxes)
plt.hist(all_deviations, bins=40, color= 'blue', edgecolor='black', alpha=0.7)
plt.title("SiPM Breakdown Voltage Stability Across 5 Repeated Runs")
plt.xlabel("Individual Run VBD - SiPM Average VBD (V)")
plt.ylabel("Counts")
plt.grid(True, linestyle='--', alpha=0.5)
plt.xlim(-0.010, 0.010)
plt.show()

"""


"""
# Debrecen 
for SiPMDnum, VBDDeb in sipm_history2.items():

    values = sipm_history2[SiPMDnum]

    if len(values) > 0:
        mean_vbd = np.mean(values)
        #print(f"The average breakdown voltage for {SiPMDnum} is {mean_vbd:.4f}")
        #sipm_means[sipm_id] = mean_vbd
        print(f"{np.array(values[0])} - {mean_vbd}) = {np.array(values[0]) - mean_vbd}")
        print(f"{np.array(values[1])} - {mean_vbd}) = {np.array(values[1]) - mean_vbd}")
        print(f"{np.array(values[2])} - {mean_vbd}) = {np.array(values[2]) - mean_vbd}")
        #print(f"{np.array(values[3])} - {mean_vbd}) = {np.array(values[3]) - mean_vbd}")
        #print(f"{np.array(values[4])} - {mean_vbd}) = {np.array(values[4]) - mean_vbd}")
              
        #print(f" {sipm_history[sipm_id]} - {mean_vbd} ")
        sipm_deviations[SiPMDnum] = (np.array(values) - mean_vbd)
#     print(sipm_deviations[SiPMDnum])
"""

"""
    # Debrecen method
    with open(debrecen_file, 'r') as f:
        lines = f.readlines()

    for line in lines[1:]:
        parts = line.split()
        if len(parts) < 5:
            continue
        try:
            SiPMpos  = parts[1].split('_')
            row      = float(SiPMpos[-2])
            col      = float(SiPMpos[-1])
            SiPMDnum = int((20 * row) + col)

            VBDDeb   = float(parts[4])
            avg_temp = float(parts[2])

            # Temperature correction -- single line works for both + and - diff
            temp_diff = 25 - avg_temp
            VBDDeb    = VBDDeb + temp_diff * 0.0347

            if SiPMDnum not in sipm_history2:
                sipm_history2[SiPMDnum] = []
            sipm_history2[SiPMDnum].append(VBDDeb)

        except (ValueError, IndexError):
            continue
"""