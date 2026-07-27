import matplotlib.pyplot as plt
import numpy as np
import os
import matplotlib.gridspec as gridspec

# ------------------------------------------------------------------------------------------
# This code generates the IV - SPS for all methods
# It takes the values written from VBDwritetextTC.py and VBDwritetextTUnCorr.py
# There are 3 figures
# Figure 1 plots the differences between IV and SPS across the entire dataset in a histogram
# Figure 2 plots the standard deviation of each method at the tray level
# FIgure 3 plots the mean offset of each method at the tray level
# ------------------------------------------------------------------------------------------

# Insert the folder with all the trays
folder_with_trays = "/Users/mekail/Documents/Wright Lab/Michael's Code/production/robot_production"

# Look inside the folder and filter out only the directories (tray folders)
trays = [
    f for f in os.listdir(folder_with_trays) 
    if os.path.isdir(os.path.join(folder_with_trays, f))
]
print(trays)


# Dictionary for the tray standard deviations between methods and SPS
tray_std = {} # key = tray_ID: std1, std2...
tray_mean = {}
IVSPS_diff = {}
for tray in trays:

    # Tray folder 
    tray_folder = os.path.join(folder_with_trays, tray)
    Tray_ID = os.path.basename(tray_folder)
    print(f"Processing tray {Tray_ID}")

    # IV summary
    summary_folder = os.path.join(tray_folder, "summary")
    TempCorr_folder = os.path.join(summary_folder, "temperature corrected")
    IVsummary_file = os.path.join(TempCorr_folder, "All Methods")

    # SPS file
    SPSfile = os.path.join(summary_folder, 'SPS summary')

    # Dictionary for SPS
    SPS_map = {} # key: SiPM index: SPS value
    SPSindex_list = []
    SPSvbd_list = []

    print(f"Reading SPS summary for tray {Tray_ID}")
    with open(SPSfile, 'r') as f:
        lines = f.readlines()
    
    for line in lines:
        parts = line.split()

        if len(parts) == 6:
            try:
                SIPMindex = int(parts[0])
                SPSraw = float(parts[1])
                SPScor = float(parts[2])
                avg_temp = float(parts[3])
                SPS_map[SIPMindex] = SPScor
            except ValueError:
                continue
            
    # Lists for information
    SiPMnumlist = []
    SPSVBDlist = []
    RelDerVBDlist = []
    InvDerVBDlist = []
    SecDerVBDlist = []
    TangentVBDlist = []
    ParabolicVBDlist = []
    Temperaturelist = []


    print(f"Reading IV breakdown voltage summary for tray {Tray_ID}\n")
    # My method
    with open(IVsummary_file, 'r') as f:
        lines = f.readlines()

    for line in lines:
        parts = line.split()
        if len(parts) == 7:
            try:
                SiPM = int(float((parts[0])))
                VBD_RD  = float(parts[1])
                VBD_Inv = float(parts[2])
                VBD_Sec = float(parts[3])
                VBD_Tan = float(parts[4])
                VBD_Para = float(parts[5])
                avg_temp = float(parts[6])

            except ValueError:
                continue


            # Skip if no matching SPS record
            if SiPM not in SPS_map:
                print(f"    SiPM {SiPM}: no SPS record — skipping")
                continue
            # Temperature Corrections
            # temp_diff = 25 - avg_temp
            # VBD_RD = VBD_RD + temp_diff * 0.037
            # VBD_Inv = VBD_Inv + temp_diff * 0.036
            # VBD_Sec = VBD_Sec + temp_diff * 0.036
            # VBD_Tan = VBD_Tan + temp_diff * 0.036
            # VBD_Para = VBD_Para + temp_diff * 0.036

            SiPMnumlist.append(SiPM)
            SPSVBDlist.append(SPS_map[SiPM])
            RelDerVBDlist.append(VBD_RD)
            InvDerVBDlist.append(VBD_Inv)
            SecDerVBDlist.append(VBD_Sec)
            TangentVBDlist.append(VBD_Tan)
            ParabolicVBDlist.append(VBD_Para)

    SPSvbd_array = np.array(SPSVBDlist)
    SiPMnum_array = np.array(SiPMnumlist)
    RDvbd_array = np.array(RelDerVBDlist)
    INVvbd_array = np.array(InvDerVBDlist)
    SECvbd_array = np.array(SecDerVBDlist)
    TANvbd_array = np.array(TangentVBDlist)
    PARAvbd_array = np.array(ParabolicVBDlist)

    IVSPS_diff[Tray_ID] = {
        "Relative Derivative" : (RDvbd_array - SPSvbd_array),
        "Inverse Derivative" : (INVvbd_array - SPSvbd_array),
        "Second Derivative" : (SECvbd_array - SPSvbd_array),
        "Tangent" : (TANvbd_array - SPSvbd_array), 
        "Parabolic" : (PARAvbd_array - SPSvbd_array)
    }

    tray_std[Tray_ID] = {
        "Relative Derivative" : np.std(RDvbd_array - SPSvbd_array),
        "Inverse Derivative" : np.std(INVvbd_array - SPSvbd_array),
        "Second Derivative" : np.std(SECvbd_array - SPSvbd_array),
        "Tangent" : np.std(TANvbd_array - SPSvbd_array), 
        "Parabolic" : np.std(PARAvbd_array - SPSvbd_array)
    }

    tray_mean[Tray_ID] = {
        "Relative Derivative" : np.mean(RDvbd_array - SPSvbd_array),
        "Inverse Derivative" : np.mean(INVvbd_array - SPSvbd_array),
        "Second Derivative" : np.mean(SECvbd_array - SPSvbd_array),
        "Tangent" : np.mean(TANvbd_array - SPSvbd_array), 
        "Parabolic" : np.mean(PARAvbd_array - SPSvbd_array)
    }

    print(f"Finished reading SPS and IV summaries for tray {Tray_ID}")


methods = list(list(tray_std.values())[0].keys())
tray_names = list(tray_std.keys())
colors = ['blue', 'orange', 'green', 'red', 'purple']
"""
# This block generates histograms for the differences between IV and SPS across the entire dataset
fig = plt.figure(figsize=(15, 10))
gs = gridspec.GridSpec(2, 6)

positions = [
    gs[0, 0:2], gs[0, 2:4], gs[0, 4:6], 
    gs[1, 1:3], gs[1, 3:5] # Centered: columns 1-3 and 3-5
]

for i, method in enumerate(methods):
    ax = fig.add_subplot(positions[i])
    
    # Differences between IV method and SPS for all sipms across trays
    all_diffs_for_method = np.concatenate([IVSPS_diff[tray][method] for tray in tray_names])

    global_mean = np.mean(all_diffs_for_method)
    global_std = np.std(all_diffs_for_method)


    ax.hist(all_diffs_for_method, bins=40, color = colors[i], alpha=0.7, edgecolor='black')

    # Create a textbox with std and mean of all the differences between sipms
    box_text = f"Mean: {global_mean:.3f} V\n$\sigma$: {global_std:.4f} V"
    
    box_style = dict(
        boxstyle='round,pad=0.5',
        facecolor='white',
        edgecolor='black',
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
    
    # Formatting
    ax.set_title(method, fontsize=12, weight = 'bold')
    # ax.set_xlabel("Difference (IV - SPS) [V]")
    # ax.set_ylabel("Frequency")
    ax.grid(axis='y', linestyle='--', alpha=0.4)
    ax.tick_params(axis='both', which='major', labelsize=14)

plt.tight_layout(rect=[0, 0.03, 1, 0.95])
plt.show()
"""


"""
# Figure 1 plots the standard deviation of each method at the tray level
# FIgure 2 plots the mean offset of each method at the tray level

# Define a distinct marker style for each method to keep them distinct
markers = ['o', 's', '^', 'D', 'v']
colors = ['blue', 'orange', 'green', 'red', 'purple']
fig1, ax1 = plt.subplots(figsize=(14, 8))

for idx, method in enumerate(methods):
    y_values = [tray_std[tray_name][method] for tray_name in tray_names]
    ax1.scatter(tray_names, y_values, label=method,
                marker=markers[idx], s=80, alpha=0.85)

ax1.set_xlabel('Tray ID', fontsize=12)
ax1.set_ylabel('Std Dev of (IV - SPS) [V]', fontsize=12)
ax1.set_xticklabels(tray_names, rotation=30, ha='right')
# ax1.tick_params(axis='x', which='major', labelsize=14)
# ax1.tick_params(axis = 'y', which = 'major', labelsize = '18')
ax1.grid(True, linestyle='--', alpha=0.4)
ax1.legend(
    title="Analysis Methods", 
    loc='best', 
    fontsize=14,          # Sets the size of the legend item labels
    title_fontsize=14    # Sets the size of the legend title ("Analysis Methods")
    #markerscale=1.5       # Scales up the lines/dots in the legend box (1.5x bigger)
)
plt.tight_layout()
plt.show()

# Plot 2: Mean difference (accuracy) 
fig2, ax2 = plt.subplots(figsize=(14, 8))

for idx, method in enumerate(methods):
    y_values = [tray_mean[tray_name][method] for tray_name in tray_names]
    ax2.scatter(tray_names, y_values, label=method,
                marker=markers[idx], s=80, alpha=0.85)

ax2.axhline(y=0, color='black', linestyle='-', linewidth=1, alpha=0.5)
ax2.set_xlabel('Tray ID', fontsize=12)
ax2.set_ylabel('Mean of (IV - SPS) [V]', fontsize=12)
#ax2.set_title('Method Accuracy Across Production Trays', fontsize=14)
# ax2.set_xticks(range(len(tray_names)))
ax2.set_xticklabels(tray_names, rotation=30, ha='right')
# ax2.tick_params(axis='x', which='major', labelsize=14)
# ax2.tick_params(axis = 'y', which = 'major', labelsize = 18)
ax2.grid(True, linestyle='--', alpha=0.4)
ax2.legend(
    title="Analysis Methods", 
    loc='best', 
    fontsize=14,         
)

plt.tight_layout()
plt.show()


# Summary table printed to terminal 
print(f"\n{'Method':<25} {'Mean across trays':>18} {'Std across trays':>18}")
print("-" * 65)
for method in methods:
    all_means = [tray_mean[t][method] for t in tray_names]
    all_stds  = [tray_std[t][method]  for t in tray_names]
    print(f"{method:<25} {np.mean(all_means):>18.4f} V {np.mean(all_stds):>16.4f} V")
"""



"""
# This is old code which calculated the breakdown voltage of the SiPMs.
# 
# Determines the SiPM and tray ID
def getSiPMnum(filepath):
    with open(filepath, "r") as f:
        lines = f.readlines()

    SiPM_num = lines[6].split(':')[1].strip() # Example SiPM number line: "SiPM number      : 419"
    return SiPM_num

# Outputs the current and voltage within that file as an array
def detIV(file):
    voltage_list = []
    current_list = []
    # Read the file
    
    textfile = file
    with open(textfile, "r") as f:
        lines = f.readlines()

    # Example
    #num        SMU Voltage (V)   Voltage (V)     Current (A)
    #-----------------------------------------------------
    #0              37.804497     37.803391     0.000000365
    #1              37.824498     37.823380     0.000000324
    
    # Current and voltage are aligned in this way
    # So identify the lines with only 4 entries
    for line in lines: 
        parts = line.split()

        if len(parts) == 4:
            try: 
                index = float(parts[0]) #Number
                smu_v = float(parts[1]) #SMU voltage
                dmm_v = float(parts[2]) #DMM voltage
                c = float(parts[3]) #Current 
                voltage_list.append(dmm_v)
                current_list.append(c)
            except ValueError:
                continue
    
    sipm_index = lines[6].split(':')
    sipm_index = int(sipm_index[1].strip())
                
    # Current and voltage are appended to arrays 
    voltage_array = np.array(voltage_list[5:])
    current_array = np.array(current_list[5:])

    current_array = current_array * 1E6
    return voltage_array, current_array# sipm_index

# Determines the breakdown voltage using the relative derivative method
def RelDer(vol, cur):

    # Calculate the first derivative of the natural log
    # Use Savgol filter to smooth the data
    dln_dV = np.gradient(np.log(cur), vol)
    #dln_dV = savgol_filter(dln_dV, window_length = 11, polyorder = 3)

    # A gaussian is fit to the peak of the derivative
    def gaussian(x, amplitude, mean, sigma):
        return amplitude * np.exp(-0.5 * ((x - mean) / sigma) ** 2)
    
    # Use the maximum value of the arrays as a reference point for the Gaussian fit
    peak_index = np.argmax(dln_dV)
    peak_voltage = vol[peak_index]

    # Define a range around the peak for the Gaussian fit
    range = 0.08
    lower = peak_voltage - range
    upper = peak_voltage + range

    # Fit window
    # Cuts the data using a boolean mask to only include the data within the defined range
    mask = (vol >= lower) & (vol <= upper)
    vol_cut    = vol[mask]
    dln_dV_cut = dln_dV[mask]

    # Fits a Gaussian to the fit window
    p0 = [max(dln_dV_cut), peak_voltage, range]
    popt, _ = curve_fit(gaussian, vol_cut, dln_dV_cut, p0=p0)

    # The mean of the fitted Gaussian is taken as the breakdown voltage
    VBD = popt[1] 

    return VBD

# Determines the breakdown voltage using the inverse derivative method
def InvDer(vol, cur, RD):

    # Compute the reciporical of the first derivative
    dln_dV = np.gradient(np.log(cur), vol)
    Invdln_dV = 1 / np.gradient(np.log(cur), vol)
    Invdln_dV = savgol_filter(Invdln_dV, window_length = 11, polyorder = 3)

    # Boundaries for the fit
    lower_boundary = RD + 0.3
    upper_boundary = RD + 0.8

    # Cut out only the data in the fit window
    mask         = (vol >= lower_boundary) & (vol <= upper_boundary)
    v_fit        = vol[mask]
    InvDerv_fit  = Invdln_dV[mask]

    # Fit a straight line to the rising region
    slope, intercept = np.polyfit(v_fit, InvDerv_fit, 1)

    # Breakdown voltage is where the line crosses y = 0
    VBD = -intercept / slope

    return VBD

# Determines the breakdown voltage using the second derivative method
def SecDer(vol, cur): 
    ln_current = np.log(cur)

    dln_dV = savgol_filter(np.gradient(ln_current, vol), window_length = 11, polyorder = 3)
    d2ln_dV = savgol_filter(np.gradient(dln_dV, vol), window_length = 11, polyorder = 3)

    # Fits a gaussian to the peak 
    def gaussian(x, amplitude, mean, sigma):
        return amplitude * np.exp(-0.5 * ((x - mean) / sigma) ** 2)

    # Uses the maximum value as a initial guess for the 'mean' of the Gaussian
    peak_index = np.argmax(d2ln_dV)
    peak_voltage = voltage[peak_index]

    # Range of the window 
    range = 0.1
    lower = peak_voltage - range
    upper = peak_voltage + range

    # Boolean mask to restrict voltage values within the window
    mask = (vol >= lower) & (vol <= upper)
    vol_cut    = voltage[mask]
    d2ln_dV_cut = d2ln_dV[mask]

    # Fit Gaussian to ONLY the data from the window
    p0 = [max(d2ln_dV_cut), peak_voltage, range]
    popt, _ = curve_fit(gaussian, vol_cut, d2ln_dV_cut, p0=p0)
    
    VBD = popt[1] # The center of the gaussian is the brekadown voltage
    return VBD

# Determines the breakdown voltage using the tangent method
def Tangent(vol, cur, RD): 
    ln_current = np.log(cur)
    # Creates a fit window for the linear line fit to the left of the breakdown voltage
    left_window = (vol >= RD - 0.7) & (vol <= RD - 0.55)

    # Performs a linear fit for the baseline using the left_window
    m_baseline, b_baseline = np.polyfit(vol[left_window], ln_current[left_window], 1)

    # Takes the derivative of the natural log of the current to find the inflection point for the tangent line fit
    dln_dV = np.gradient(ln_current, vol)

    # Find the voltage in array closest to RD
    RD_idx = np.argmin(np.abs(vol - RD))

    # Recall the inflection point is just the breakdown voltage from RD
    x_inflection = vol[RD_idx]
    y_inflection = ln_current[RD_idx]

    # Perform a linear fit on just the right window region
    m_tangent = dln_dV[RD_idx] # The slope at the inflection is just dln_dV evaluated at RD
    b_tangent = y_inflection - m_tangent * x_inflection 

    # Breakdown voltage is the x-value where the tangent line intersects the baseline
    # Set the two lines equal to each other and solve for x
    VBD = (b_baseline - b_tangent) / (m_tangent - m_baseline)
    return VBD

# Determines the breakdown voltage using the Parabolic method
def Parabolic(vol, cur, RD):
    ln_current = np.log(cur)
    
    # Uses a boolean loop to create a window for the baseline fit
    left_mask = (vol >= RD - 0.7) & (vol <= RD - 0.55)
    voltage_left = vol[left_mask]
    current_left = ln_current[left_mask]

    # Calculates the slope and intercept for the baseline fit line
    m_baseline, b_baseline = np.polyfit(voltage_left, current_left, 1)

    # Adds more data points to brentq can give random values 
    vol_fine = np.linspace(vol.min(), vol.max(), 300)
    interp_funct = interp1d(vol, cur, kind = 'cubic')
    cur_fine = interp_funct(vol_fine)

    # Shifts voltage to make fitting more stable
    v_shifted = vol_fine - RD
   
    # Window for the Parabolic fit
    para_mask = (v_shifted >= 0.1) & (v_shifted <= voltage.max())

    # Determining the parabolic fit
    a, b, c = np.polyfit(v_shifted[para_mask], cur_fine[para_mask], 2)
 
    def objective(v):
        # vs is the shifted voltage
        vs = v - RD
        parabolic_val = a * vs ** 2 + b * vs+ c # Evaluates the parabolic equation at vs
        baseline_val = np.exp(m_baseline*v + b_baseline) # Evaluates the baseline at vs
        
        # brentq will use parabolic_val - baseline_val to find the intersection of the two functions
        # Notably, when parabolic - baseline = 0 lies the intersection or VBD given by the parabolic method
        return parabolic_val - baseline_val
    
    # Find intersection using brentq
    try:
        f_start = objective(vol_fine.min())
        f_end = objective(vol_fine.max())

        if f_start * f_end > 0:
            return None
        
        # brentq uses the objective function to find the intersection point
        # This is the breakdown voltage extrapolated by the parabolic method 
        VBD = brentq(objective, vol_fine.min(), vol_fine.max(), xtol = 1e-6)
        return VBD
    
    except Exception as e:
        return (None)

folder_with_trays = "/Users/mekail/Documents/Wright Lab/Michael's Code/production/robot_production"

# Look inside the folder and filter out only the directories (tray folders)
trays = [
    f for f in os.listdir(folder_with_trays) 
    if os.path.isdir(os.path.join(folder_with_trays, f))
]
print(trays)

# Dictionary for the tray standard deviations between methods and SPS
tray_results = {} # key = tray_ID: std1, std2...
for tray in trays:

    # Tray folder 
    tray_folder = os.path.join(folder_with_trays, tray)
    Tray_ID = os.path.basename(tray_folder)
    print(f"Processing tray {Tray_ID}")

    # Summary folder
    summary_folder = os.path.join(tray_folder, 'summary')

    # Text_files is the folder directory containing all the parsed text files
    text_files = glob.glob(os.path.join(tray_folder, "parsed_txt", "*txt"))

    print(f"Found {len(text_files)} files in {Tray_ID} folder \n")

    # SPS file
    SPSfile = os.path.join(summary_folder, 'SPS summary')

    # Dictionary for SPS
    SPS_map = {} # key: SiPM index: SPS value
    SPSindex_list = []
    SPSvbd_list = []
    with open(SPSfile, 'r') as f:
        lines = f.readlines()

    for line in lines:
        parts = line.split()

        if len(parts) == 4:
            try:
                SIPMindex = int(parts[0])
                SPSraw = float(parts[1])
                SPScor = float(parts[2])
                avg_temp = float(parts[3])
                SPS_map[SIPMindex] = SPSraw
                # SPSindex_list.append(SIPMindex)
                # SPSvbd_list.append(SPScor)
            except ValueError:
                continue
    # SPSindex_array = np.array(SPSindex_list)
    # SPSvbd_array = np.array(SPSvbd_list)

    # Creates a map to analyze the SiPMs in ascending SiPM order
    SiPM_file_map = {}
    # Orders the SiPM by numerical order
    for SiPM_file in text_files:
        filename = os.path.basename(SiPM_file)
        
        # Split the txt file name into chunks to get the SiPM number
        
        chunks = filename.split('_')
        row = int(chunks[2])
        column = int(chunks[3]) # If the filename ends in .txt, split off the extension
        SiPM_num = (row * 20) + column

        SiPM_file_map[SiPM_num] = SiPM_file
    

    SiPMnumlist = []
    SPSVBDlist = []
    RelDerVBDlist = []
    InvDerVBDlist = []
    SecDerVBDlist = []
    TangentVBDlist = []
    ParabolicVBDlist = []
    Temperaturelist = []


    all_channels = sorted(list(set(SiPM_file_map.keys()).union(SPS_map.keys())))

    # Computes the breakdown voltage in numerical order of SiPM
    for SiPM in all_channels:
        if (SiPM not in SiPM_file_map) or (SiPM not in SPS_map):
            print(f"  -> Skipping channel {SiPM}: Missing data or SPS record.")
            continue

        voltage, current = detIV(SiPM_file_map[SiPM])

        VBD_RD = RelDer(voltage, current)
        VBD_Inv = InvDer(voltage, current)
        VBD_Sec = SecDer(voltage, current)
        VBD_Tangent = Tangent(voltage, current, VBD_RD)
        VBD_Parabolic = Parabolic(voltage, current, VBD_RD)
    
        SiPMnumlist.append(SiPM)
        SPSVBDlist.append(SPS_map[SiPM])
        RelDerVBDlist.append(VBD_RD)
        InvDerVBDlist.append(VBD_Inv)
        SecDerVBDlist.append(VBD_Sec)
        TangentVBDlist.append(VBD_Tangent)
        ParabolicVBDlist.append(VBD_Parabolic)

    SiPMnum_array = np.array(SiPMnumlist)
    SPSvbd_array = np.array(SPSVBDlist)
    RDvbd_array = np.array(RelDerVBDlist)
    INVvbd_array = np.array(InvDerVBDlist)
    SECvbd_array = np.array(SecDerVBDlist)
    TANvbd_array = np.array(TangentVBDlist)
    PARAvbd_array = np.array(ParabolicVBDlist)

    tray_results[Tray_ID] = {
        "Relative Derivative" : np.std(RDvbd_array - SPSvbd_array),
        "Inverse Derivative" : np.std(INVvbd_array - SPSvbd_array),
        "Second Derivative" : np.std(SECvbd_array - SPSvbd_array),
        "Tangent" : np.std(TANvbd_array - SPSvbd_array), 
        "Parabolic" : np.std(PARAvbd_array - SPSvbd_array)
    }

methods = list(list(tray_results.values())[0].keys())
tray_names = list(tray_results.keys())

fig, ax = plt.subplots(figsize=(10, 6))

# Define a distinct marker style for each method to keep them distinct
markers = ['o', 's', '^', 'D', 'v']

for idx, method in enumerate(methods):
    # Pull the standard deviations for this specific method across all trays
    y_values = [tray_results[tray_name][method] for tray_name in tray_names]
    
    # Plot as a scatter plot series
    ax.scatter(tray_names, y_values, label=method, marker=markers[idx % len(markers)], s=80, alpha=0.85)

ax.set_xlabel('Tray ID', fontsize=12)
ax.set_ylabel('Std Dev of (IV - SPS) VBD [V]', fontsize=12)
ax.set_title('Method Precision Performance Across Production Trays', fontsize=14)
ax.set_xticklabels(tray_names, rotation=30, ha='right')

ax.grid(True, linestyle='--', alpha=0.4)
ax.legend(title="Analysis Methods", loc='best')

plt.tight_layout()
plt.show()
"""