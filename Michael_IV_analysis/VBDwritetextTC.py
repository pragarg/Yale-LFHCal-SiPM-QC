import numpy as np
import os
from scipy.optimize import curve_fit
import glob
from scipy.optimize import brentq
from scipy.signal import savgol_filter
from scipy.interpolate import interp1d
import time

# ----------------------------------------------------------------------
# Writes the temperature corrected values for all methods in a text file
# ----------------------------------------------------------------------

start_time = time.time()

# Determines the SiPM and tray ID
def getSiPMnum(filepath):
    with open(filepath, "r") as f:
        lines = f.readlines()

    SiPM_num = lines[6].split(':')[1].strip() # Example SiPM number line: "SiPM number      : 419"
    return SiPM_num

def temperature(filepath):

    with open (filepath, 'r') as f:
        lines = f.readlines()

        
        temp_lines = lines[18:20]
        
        temp_array = []
        for line in temp_lines:
            parts2 = line.split()
            temps = float(parts2[2])
            temp_array.append(temps)
        
        temp_array = np.array(temp_array)
        avg_temp = np.mean(temp_array)
    return avg_temp

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
                
    # Current and voltage are appended to arrays 
    voltage_array = np.array(voltage_list[5:])
    current_array = np.array(current_list[5:])

    current_array = current_array * 1E6
    return voltage_array, current_array

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
    range = 0.1
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

    ln_current = np.log(cur)
    dln_dV = savgol_filter(np.gradient(ln_current), window_length = 11, polyorder = 3)
    Invdln_dV = 1 / dln_dV

    # Boundaries for the fit
    lower_boundary = RD + 0.15
    upper_boundary = RD + 0.9

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
    range = 0.15
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
    left_window = (vol >= RD - 1) & (vol <= RD - 0.65)

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
    left_mask = (vol >= RD - 1) & (vol <= RD - 0.65)
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
    para_mask = (v_shifted >= 0.05) & (v_shifted <= 0.8)

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

# Put the path to the folder with all your tray information
folder_with_trays = "/Users/mekail/Documents/Wright Lab/Michael's Code/production/robot_production"

# Look inside the folder and filter out only the directories (tray folders)
trays = [
    f for f in os.listdir(folder_with_trays) 
    if os.path.isdir(os.path.join(folder_with_trays, f))
]
print(trays)
for tray in trays:

    # Tray folder 
    tray_folder = os.path.join(folder_with_trays, tray)
    Tray_ID = os.path.basename(tray_folder)
    print(f"Processing tray {Tray_ID}")

    # Make a folder called summaries if it doesn't already exist
    summary_folder = os.path.join(tray_folder, 'summary')
    os.makedirs(summary_folder, exist_ok = True)

    # Make a folder called TempCorr
    TempCorr_folder = os.path.join(summary_folder, 'temperature corrected')
    os.makedirs(TempCorr_folder, exist_ok = True)

    # Text_files is the folder directory containing all the parsed text files
    text_files = glob.glob(os.path.join(tray_folder, "parsed_txt", "*txt"))

    print(f"Found {len(text_files)} files in {Tray_ID} folder \n")

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
    RelDerVBDlist = []
    InvDerVBDlist = []
    SecDerVBDlist = []
    TangentVBDlist = []
    ParabolicVBDlist = []
    Temperaturelist = []

    # Computes the breakdown voltage in numerical order of SiPM
    for SiPM in sorted(SiPM_file_map):
        voltage, current = detIV(SiPM_file_map[SiPM])
        temp_avg = temperature(SiPM_file_map[SiPM])

        VBD_RD = RelDer(voltage, current)
        VBD_Inv = InvDer(voltage, current, VBD_RD)
        VBD_Sec = SecDer(voltage, current)
        VBD_Tan = Tangent(voltage, current, VBD_RD)
        VBD_Para = Parabolic(voltage, current, VBD_RD)

        # Temperature Corrections
        temp_diff = 25 - temp_avg
        VBD_RD = (VBD_RD  + temp_diff * 0.037)
        VBD_Inv = (VBD_Inv + temp_diff * 0.036)
        VBD_Sec = (VBD_Sec  + temp_diff * 0.036)
        VBD_Tan = (VBD_Tan  + temp_diff * 0.036)
        VBD_Para = (VBD_Para + temp_diff * 0.036)

        SiPMnumlist.append(SiPM)
        Temperaturelist.append(temp_avg)
        RelDerVBDlist.append(VBD_RD)
        InvDerVBDlist.append(VBD_Inv)
        SecDerVBDlist.append(VBD_Sec)
        TangentVBDlist.append(VBD_Tan)
        ParabolicVBDlist.append(VBD_Para)

    SiPMnum_array = np.array(SiPMnumlist)
    RelDerVBD_array = np.array(RelDerVBDlist)
    InvDerVBD_array = np.array(InvDerVBDlist)
    SecDerVBD_array = np.array(SecDerVBDlist)
    TangentVBD_array = np.array(TangentVBDlist)
    ParabolicVBD_array = np.array(ParabolicVBDlist)
    Temperature_array = np.array(Temperaturelist)

    RelDer_file = os.path.join(TempCorr_folder, "Relative Derivative")
    InvDer_file = os.path.join(TempCorr_folder, "Inverse Dervative")
    SecDer_file = os.path.join(TempCorr_folder, "Second Derivative")
    Tangent_file = os.path.join(TempCorr_folder, "Tangent")
    Parabolic_file = os.path.join(TempCorr_folder, "Parabolic")

    AllMethods_file = os.path.join(TempCorr_folder, "All Methods")

    with open(AllMethods_file, 'w') as f:
        print("Writing the summary for all methods")

        # Title
        f.write("=== All Methods Summary ===\n")

        # Header
        f.write("\n[Header]\n")
        f.write(f"Tray ID: {Tray_ID}\n")

        # All Breakdown voltages 
        f.write("\n[Breakdown Voltage (V)]\n")
        f.write("SiPM \t Relative \t Inverse \t Second \t Tangent \t Parabolic\t AvgTemp \n")
        f.write('-' * 96 + '\n')

        for i, sipm in enumerate(SiPMnum_array):

            entry1 = SiPMnum_array[i]
            entry2 = RelDerVBD_array[i]
            entry3 = InvDerVBD_array[i]
            entry4 = SecDerVBD_array[i]
            entry5 = TangentVBD_array[i]
            entry6 = ParabolicVBD_array[i]
            entry7 = Temperature_array[i]  

            f.write(f"{entry1} \t {entry2:.4f} \t {entry3:.4f} \t {entry4:.4f} \t {entry5:.4f} \t {entry6:.4f} \t {entry7:.3f} \n")
        print(f"The summary for all methods for tray {Tray_ID} is finished")
        print()

    # Writes the file for the relative derivative breakdown voltage
    with open(RelDer_file, 'w') as f:
        print("Writing the summary for the relative derivative")

        # Title
        f.write("=== Relative Derivative Summary ===\n")

        # Header
        f.write("\n[Header]\n")
        f.write(f"Tray ID: {Tray_ID}\n")

        # Relative derivative Values
        f.write("\n[Relative Derivative Values]\n")
        f.write("SiPM \t Breakdown Voltage (V)\t Avg Temp °C\n")
        f.write('-' * 44 + '\n')
        
        for i, sipm in enumerate(SiPMnum_array):

            entry1 = SiPMnum_array[i]
            entry2 = RelDerVBD_array[i]
            entry3 = Temperature_array[i]

            f.write(f"{entry1} \t {entry2:.4f} \t\t {entry3:.3f}\n")
        print(f"The relative derivative summary for tray {Tray_ID} is finished")
        print()
        
    # Writes the file for the inverse derivative breakdown voltage values
    with open(InvDer_file, 'w') as f:
        print("Writing the summary for the inverse derivative")
        # Title
        f.write("=== Inverse Derivative Summary ===\n")

        # Header
        f.write("\n[Header]\n")
        f.write(f"Tray ID: {Tray_ID}\n")

        # Inverse derivative values
        f.write("\n[Inverse Derivative]\n")
        f.write("SiPM \t Breakdown Voltage (V)\t Avg Temp °C\n")
        f.write('-' * 44 + '\n')
        
        for i, sipm in enumerate(SiPMnum_array):

            entry1 = SiPMnum_array[i]
            entry2 = InvDerVBD_array[i]
            entry3 = Temperature_array[i]

            f.write(f"{entry1} \t {entry2:.4f} \t\t {entry3:.3f}\n")
        print(f"The inverse derivative summary for tray {Tray_ID} is finished")
        print()

    with open(SecDer_file, 'w') as f:
        print("Writing the summary for the second derivative")

        # Title
        f.write("=== Second Derivative Summary ===\n")

        # Header
        f.write("\n[Header]\n")
        f.write(f"Tray ID: {Tray_ID}\n")

        # Second Derivative Values
        f.write("\n[Second Derivative Values]\n")
        f.write("SiPM \t Breakdown Voltage (V)\t Avg Temp °C\n")
        f.write('-' * 44 + '\n')
        
        for i, sipm in enumerate(SiPMnum_array):

            entry1 = SiPMnum_array[i]
            entry2 = SecDerVBD_array[i]
            entry3 = Temperature_array[i]

            f.write(f"{entry1} \t {entry2:.4f} \t\t {entry3:.3f} \n")
        print(f"The second derivative summary for tray {Tray_ID} is finished")
        print()

    with open(Tangent_file, 'w') as f:
        print("Writing the summary for the tangent method")

        # Title
        f.write("=== Tangent Method Summary ===\n")

        # Header
        f.write("\n[Header]\n")
        f.write(f"Tray ID: {Tray_ID}\n")

        # Tangent method
        f.write("\n[Tangent]]\n")
        f.write("SiPM \t Breakdown Voltage (V)\t Avg Temp °C\n")
        f.write('-' * 44 + '\n')
        
        for i, sipm in enumerate(SiPMnum_array):

            entry1 = SiPMnum_array[i]
            entry2 = TangentVBD_array[i]
            entry3 = Temperature_array[i]

            f.write(f"{entry1} \t {entry2:.4f} \t\t {entry3:.3f}\n")
        print(f"The Tangent summary for tray {Tray_ID} is finished")
        print()

    with open(Parabolic_file, 'w') as f:
        print("Writing the summary for the parabolic method")
        # Title
        f.write("=== Parabolic Method Summary ===\n")

        # Header
        f.write("\n[Header]\n")
        f.write(f"Tray ID: {Tray_ID}\n")

        # Parabolic method
        f.write("\n[Parabolic]\n")
        f.write("SiPM \t Breakdown Voltage (V)\t Avg Temp °C\n")
        f.write('-' * 44 + '\n')
        
        for i, sipm in enumerate(SiPMnum_array):

            entry1 = SiPMnum_array[i]
            entry2 = ParabolicVBD_array[i]
            entry3 = Temperature_array[i]

            f.write(f"{entry1} \t {entry2:.4f} \t\t {entry3:.3f}\n")
        print(f"The parabolic method summary for tray {Tray_ID} is finished")
        print()
    
    
    print(f"All summarys for tray {Tray_ID} have been written\n")
print("All conversions are complete!")

end_time = time.time()
execution_time = end_time - start_time
print(f"Code took {execution_time} seconds to run")