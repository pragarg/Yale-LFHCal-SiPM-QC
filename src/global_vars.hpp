//  *--
//  global_vars
//
//  Header file with certain global parameters that might be used throughout the analysis
//
//  Created by Ryan Hamilton on 10/19/25.
//  *--

#ifndef global_vars_h
#define global_vars_h

#include <stdio.h>
#include <sys/stat.h>
#include "../utils/color.h"
#include "../utils/root_draw_tools.h"

// Terminal output color modifiers
// Example: std::cout << t_red << "ex." << t_def << std::endl;
Color::TextModifier t_red(Color::TEXT_RED);
Color::TextModifier t_mgn(Color::TEXT_MAGENTA);
Color::TextModifier t_grn(Color::TEXT_GREEN);
Color::TextModifier t_yll(Color::TEXT_YELLOW);
Color::TextModifier t_blu(Color::TEXT_BLUE);
Color::TextModifier t_cyn(Color::TEXT_CYAN);
Color::TextModifier t_def(Color::TEXT_DEFAULT);

// Tags and Identifiers
const char Hamamatsu_SiPM_Code[20] = "S14160-1315PS";

// Variables for allowed Q/A Ranges
const double declare_Vbd_outlier_range = 0.050; // +/- 50mV range around average
const bool use_quadrature_sum_for_syst_error = true;
const double contract_outlier_margin_percent = 5; // 5% outliers allowed by contract
const float Hamamatsu_spec_max_Idark = 20.;

// Fixed array info variables
const int NROW = 20;
const int NCOL = 23;

// Information about SiPM locations in physical space
const float temp_sensor_separation_cm = 0.27;
const float sipm_cassette_separation_cm = 1.0; // TODO measure actual board, these are placeholders from what I roughly remember.
// Temp corr looks weird, seems like factor of 2 could make them align??? I don't think the SiPMs are further than the sensors, that wouldn't make sense....

// Variables for I/O handling
const char batch_data_file[20] = "../batch_data.txt";

// flags to control some options in analysis
bool flag_use_all_trays_for_averages = false;       // Use all available trays' data to compute averages (Recommended ONLY when all trays are similar)

// Variables to control histogram/plot ranges
const int nbin_temp_grad = 19;
//const int nbin_temp_grad = 39;
const double range_temp_grad[2] = {-0.4, 0.4};

// TODO refine color palette
Int_t TH2_palette = kBird;
Int_t plot_colors[3] = {
  kViolet+1, kOrange+1, kRed+2
};

Int_t plot_colors_alt[3] = {
  kViolet+3, kOrange+2, kRed+2
};


// Plot settings

// Tray display mode for dark current histogram:
//  0 - No tray numbers are displayed
//  1 - All tray numbers are displayed
//  2 - Tray numbers are displayed in condensed notation
const int tray_display_mode = 0;


#endif /* global_vars_h */
