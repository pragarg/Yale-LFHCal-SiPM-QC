import matplotlib.pyplot as plt
import numpy as np
import os

# ------------------------------------------------------------------------------------------
# This code looks at how gain varies at the nominal operating voltage across trays and sipms
# ------------------------------------------------------------------------------------------

tray_vop = {
    "250717-1301": 42.00, "250717-1302": 42.00, "250717-1303": 42.00, "250717-1304": 42.00, "250717-1305": 42.00,
    "250821-1301": 42.00, "250821-1302": 42.00, "250821-1303": 42.00, "250821-1304": 42.00, "250821-1305": 42.00,
    "250911-0801": 41.50, "250911-0802": 41.50, "250911-1506": 42.20, "250911-1606": 42.30, "250911-1607": 42.30,
    "251016-0901": 41.60, "251016-1206": 41.90, "251016-1207": 41.90, "251016-1806": 42.50,
    "251113-1201": 41.90, "251113-1202": 41.90, "251113-1906": 42.60, "251113-2101": 42.80,
    "251211-1101": 41.80, "251211-1511": 42.20, "251211-1512": 42.20, "251211-2101": 42.80, "251211-2102": 42.80,
    "260115-1001": 41.70, "260115-1002": 41.70, "260115-1806": 42.50, "260115-1807": 42.50,
    "260212-1316": 42.00, "260212-1317": 42.00, "260212-1318": 42.00, "260212-1506": 42.20,
    "260312-1101": 41.80, "260312-1102": 41.80, "260312-1901": 42.60, "260312-1902": 42.60,
    "260409-1701": 42.40, "260409-1702": 42.40, "260409-1703": 42.40,
}

def detgain(slope, intercept, Vop):
    gain = slope * Vop + intercept
    return gain 

folder_with_trays = "//Users/mekail/Documents/Wright Lab/Michael's Code/production/robot_production"

# Look inside the folder and filter out only the directories (tray folders)
trays = [
    f for f in os.listdir(folder_with_trays) 
    if os.path.isdir(os.path.join(folder_with_trays, f))
]
print(trays)


gain_dict = {}
slope_array = []
y_int_array = []
for tray in trays: 
    
    tray_folder = os.path.join(folder_with_trays, tray)
    summary_folder = os.path.join(tray_folder, "summary")
    SPS_summary = os.path.join(summary_folder, "SPS summary")

    plot_folder = os.path.join(tray_folder, "plots")
    plot_folder2 = "/Users/mekail/Documents/Wright Lab/Michael's Code/Plots/GainPlots/Robot Trays"
    os.makedirs(plot_folder, exist_ok = True)

    tray_ID = os.path.basename(tray_folder)

    Vop = tray_vop[tray_ID]

    gain_array = []
    with open(SPS_summary, 'r') as f:
        lines = f.readlines()

    for line in lines:
        sections = line.split()
        if len(sections) == 6:
            try: 
                SIPMindex = int(sections[0])
                y_int = float(sections[4])
                slope = float(sections[5])
                gain = detgain(slope, y_int, Vop)
                gain_array.append(gain)
                slope_array.append(slope)
                y_int_array.append(y_int)
                if gain < 0:
                    print(f"SiPM {SIPMindex} for tray {tray_ID} has gain {gain}")
            except ValueError:
                continue

    gain_array = np.array(gain_array)
    gain_mean = np.mean(gain_array)
    gain_std = np.std(gain_array)
    gain_dict[tray_ID] = gain_mean, gain_std

    """
    # box_text = f"Mean: {gain_mean:.2f} \n$\sigma$: {gain_std:.2f} \nVop: {Vop}"

    # box_style = dict(boxstyle='round,pad=0.5', facecolor='white', edgecolor='black', alpha=0.85)
    
    
    # plt.figure(figsize = (6,4))
    # plt.text(0.75, 0.90, box_text, 
    #         transform =  plt.gca().transAxes, 
    #         fontsize=10, verticalalignment='top', 
    #         horizontalalignment='left', bbox=box_style)
    
    # plt.hist(gain_array, bins=20, edgecolor = 'black')
    # plt.title(f"Tray {tray_ID}", weight = 'bold')
    # plt.xlabel("Gain (ADC) [A.U]")
    # plt.ylabel("Count")
    # plt.legend()
    # plt.tight_layout()
    # plt.savefig(os.path.join(plot_folder2, f"Tray {tray_ID}.png"), dpi=300)
    # plt.close()
    """

print(f"mean slope gain: {np.mean(slope_array)}, standard deviation gain: {np.std(slope_array)/len(slope_array)}")
print(f"mean y-int: {np.mean(y_int_array)}, standard deviation y-int: {np.std(y_int_array)/len(y_int_array)}")
tray_ids = list(gain_dict.keys())
mean_gains = list(value[0] for value in gain_dict.values())
std_gains = list(value[1] for value in gain_dict.values())

plt.title("Gain Across Robot Trays", weight = 'bold')
plt.errorbar(tray_ids, mean_gains, yerr = std_gains, fmt = 'o', capsize = 3, zorder = 3)
#plt.ylim(405,425)
plt.xticks(rotation = 30)
plt.xlabel("Tray ID")
plt.ylabel("Mean Gain (ADC) [A.U.]")
plt.tight_layout()
plt.grid(axis='both', linestyle='--', alpha=0.4)
plt.grid(True)
plt.show()

