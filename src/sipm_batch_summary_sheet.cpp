//  *--
//  sipm_batch_summary_sheet.cpp
//  
//  Produces a summary sheet of relevant
//  plots to summarize a tray of SiPMs tested
//  with the Debrecen designed testing setup
//  stationed at Yale.
//
//  Changelog ::
//    - 10/19/25    : Created by Ryan Hamilton
//    - 05/21/2026  : Some functionality moved to compare_robot_cassette on
//  *--

#include "global_vars.hpp"
#include "SiPMDataReader.hpp"
#include "sipm_analysis_helper.hpp"

//========================================================================== Global Variables

// Some helpful label strings
const char string_tempcorr[2][50] = {"#color[2]{#bf{NOT}} Temperature corrected to 25C","Temperature corrected to 25C"};
const char string_tempcorr_short[2][10] = {"","_25C"};

// Global plot objects
TCanvas* gCanvas_solo;
TCanvas* gCanvas_double;
std::vector<std::vector<TPad*> > cpads;

// Static plot limit controls
double voltplot_limits_static[2] = {36.95, 39.3};
double diffplot_limits_static[2] = {-0.48, 0.48};
double darkcurr_limits[2] = {0, 35};

// TODO make plotter class??? Or at least consider it...

//========================================================================== Forward declarations

// V_Breakdown and V_peak distributions
void makeHist_IV_Vpeak(bool flag_run_at_25_celcius = true);
void makeHist_SPS_Vbreakdown(bool flag_run_at_25_celcius = true);

// Indexed plots to display test data for each individial tray
void makeIndexSeries(bool flag_run_at_25_celcius = true);
void makeIndexDifference(bool flag_run_at_25_celcius = true);

// Indexed plots to display test data over all available trays together
void makeIndexedTray(bool flag_run_at_25_celcius = true);
void makeIndexedOutliers(bool flag_run_at_25_celcius = true);

// Heat maps of test results for SiPM tray, cassette test location
void makeTrayMapVpeak(bool flag_run_at_25_celcius = true);
void makeTrayMapVbreakdown(bool flag_run_at_25_celcius = true);
void makeTestMapVpeak(bool flag_run_at_25_celcius = true);
void makeTestMapVbreakdown(bool flag_run_at_25_celcius = true);

// Dark current analysis
void makeHist_DarkCurrent();


//========================================================================== Macro Main

// Main macro method: generate SiPM data
void sipm_batch_summary_sheet() {
  SiPMDataReader* reader = new SiPMDataReader();
  gErrorIgnoreLevel = kWarning;
  
  // Read in trays to treat as current batch
  reader->ReadFile("../batch_data.txt");
  
  // Read IV and SPS data
  reader->ReadDataIV();
  reader->ReadDataSPS();
  
  // Initialize canvases
  gCanvas_solo = new TCanvas();
  gCanvas_double = new TCanvas();
  gStyle->SetOptStat(0);
  

  // Test averaging methods
//  int tray_index = 0;
//  std::cout << "Average V_peak for tray " << gReader->GetTrayStrings()->at(tray_index) << " \t\t:: " << getAvgVpeak(tray_index, false) << std::endl;
//  std::cout << "Average V_peak (25C) for tray " << gReader->GetTrayStrings()->at(tray_index) << " \t:: " << getAvgVpeak(tray_index, true) << std::endl;
//  std::cout << "Average V_bd for tray " << gReader->GetTrayStrings()->at(tray_index) << " \t\t:: " << getAvgVbreakdown(tray_index, false) << std::endl;
//  std::cout << "Average V_bd (25C) for tray " << gReader->GetTrayStrings()->at(tray_index) << " \t:: " << getAvgVbreakdown(tray_index, true);
//  std::cout << " (" << t_red << countOutliersVbreakdown(tray_index, true) << t_def << " Outliers beyond tray avg +/-" << declare_Vbd_outlier_range << "V)" << std::endl;
//  
//  std::cout << "For all trays ::" << std::endl;
//  std::cout << "Average V_peak for all trays \t\t\t:: " << getAvgVpeakAllTrays(false) << std::endl;
//  std::cout << "Average V_peak for all trays (25C) \t\t:: " << getAvgVpeakAllTrays(true) << std::endl;
//  std::cout << "Average V_bd for all trays \t\t\t:: " << getAvgVbreakdownAllTrays(false) << std::endl;
//  std::cout << "Average V_bd for all trays (25C) \t\t:: " << getAvgVbreakdownAllTrays(true) << std::endl;
  
  int n_trays = gReader->GetTrayStrings()->size();
  for (int i_tray = 0; i_tray < n_trays; ++i_tray) {
    std::cout << "Average V_bd (25C) for tray " << gReader->GetTrayStrings()->at(i_tray) << " \t:: " << getAvgVbreakdown(i_tray, true);
    std::cout << " (" << t_red << countOutliersVbreakdown(i_tray, true) << t_def << " Outliers beyond tray avg +/-" << declare_Vbd_outlier_range << "V)" << std::endl;
  }
  
  // Make series at ambient temp and 25C corrected
  makeIndexSeries(true);
  makeIndexSeries(false);
  makeIndexDifference(true);
  makeIndexDifference(false);
  
  makeHist_DarkCurrent();
  
  // Make 2D mappings to invesitage strange deviations
  makeTrayMapVpeak();
  makeTrayMapVbreakdown();
  makeTestMapVpeak();
  makeTestMapVbreakdown();
  
  makeTrayMapVpeak(false);
  makeTrayMapVbreakdown(false);
  makeTestMapVpeak(false);
  makeTestMapVbreakdown(false);
  
  // Write data in a format easily transferrable to a spreadsheet
  // Negative input: Write for all trays
  gReader->WriteCompressedFile(-1);
  
  // Plots with a summary of all trays to date
  makeIndexedTray(true);
  makeIndexedOutliers(true);
}// End of sipm_batch_summary_sheet::main



//========================================================================== Solo plot generating Macros: Histograms of Result Data

// Construct histograms of IV V_peak for the trays in storage
// Accesses data via global pointers in the header file
//
// TODO return TObjectArray for summary sheet
void makeHist_IV_Vpeak(bool flag_run_at_25_celcius) {
  
  // Set up canvas
  gCanvas_solo->Clear();
  gCanvas_solo->SetCanvasSize(800, 600);
  gPad->SetLeftMargin(0.1);
  gPad->SetRightMargin(0.03);
  gPad->SetTicks(1,1);
  gPad->SetTopMargin(0.1);
  
  // Iterate over all available data
  const int n_trays = gReader->GetIV()->size();
  for (int i_tray = 0; i_tray < n_trays; ++i_tray) {
    std::string c_tray_string = gReader->GetTrayStrings()->at(i_tray);
    
    // Initialize histograms
//    TH1D* hist_IV = new TH1D(Form("hist_IV_%s",c_tray_string.c_str()),
//                             ";V_{bd} [V];Count of SiPMs",
//                             )
    
    
    
    
    // plot histograms
    
    //save histograms
    
    
  }// End of loop over trays
  
  return;
}

// Construct histograms of IV V_breakdown for the trays in storage
// Accesses data via global pointers in the header file
//
// TODO return TObjectArray for summary sheet
void makeHist_IV_Vbreakdown(bool flag_run_at_25_celcius) {
  
  // Make histograms
  
  // plot histograms
  
  //save histograms
  
  return;
}

// Construct histograms of Dark Current for the trays in storage
// Accesses data via global pointers in the header file
//
// TODO return TObjectArray for summary sheet
void makeHist_DarkCurrent() {
  
  
  // TODO currently runs over all trays but would be good to check it for single trays
  
  gCanvas_solo->Clear();
  gCanvas_solo->SetCanvasSize(800, 600);
  gPad->SetLogy();
  gPad->SetLeftMargin(0.1);
  gPad->SetRightMargin(0.03);
  gPad->SetTicks(1,1);
  gPad->SetTopMargin(0.1);
  
  
  // Make histograms
  TH1D* hist_dark_current_undervoltage = new TH1D("hist_dark_current_undervoltage",
                                                  ";Dark Current I_{dark} [nA];Count of SiPMs",
                                                  2*darkcurr_limits[1], darkcurr_limits[0], darkcurr_limits[1]);
  TH1D* hist_dark_current_overvoltage = new TH1D("hist_dark_current_overvoltage",
                                                 ";Dark Current I_{dark} [nA];Count of SiPMs",
                                                 2*darkcurr_limits[1], darkcurr_limits[0], darkcurr_limits[1]);
  
  // Gather all tray data
  for (int i_tray = 0; i_tray < gReader->GetTrayStrings()->size(); ++i_tray) {
    int IV_size = gReader->GetIV()->at(i_tray)->IV_Vpeak->size();
    for (int i_IV = 1; i_IV <= IV_size; ++i_IV) {
      hist_dark_current_undervoltage->Fill(gReader->GetIV()->at(i_tray)->Idark_3below->at(i_IV - 1));
      hist_dark_current_overvoltage->Fill(gReader->GetIV()->at(i_tray)->Idark_4above->at(i_IV - 1));
    }
  }
  
  // plot histograms
  hist_dark_current_undervoltage->SetLineColor(plot_colors[0]);
  hist_dark_current_undervoltage->SetFillColorAlpha(plot_colors[0], 0.4);
  hist_dark_current_overvoltage->SetLineColor(plot_colors[1]);
  hist_dark_current_overvoltage->SetFillColorAlpha(plot_colors[1], 0.4);
  
  
  double range_Idark_plot[2] = {0.5, hist_dark_current_undervoltage->GetMaximum() * 3.75};
  hist_dark_current_undervoltage->GetYaxis()->SetRangeUser(range_Idark_plot[0], range_Idark_plot[1]);
  hist_dark_current_undervoltage->GetXaxis()->SetTitleOffset(1.2);
  
  hist_dark_current_undervoltage->Draw("hist");
  hist_dark_current_overvoltage->Draw("hist same");
  
  // Draw lines representing spec sheet limits
  TLine* contract_line = new TLine();
  contract_line->SetLineColor(kBlack);
  contract_line->DrawLine(Hamamatsu_spec_max_Idark, range_Idark_plot[0],
                          Hamamatsu_spec_max_Idark, range_Idark_plot[1]);
  
  float total_margin_horizontal = gPad->GetRightMargin() + gPad->GetLeftMargin();
  float specmax_position = gPad->GetLeftMargin() + (1-total_margin_horizontal)*(Hamamatsu_spec_max_Idark / darkcurr_limits[1]) + 0.005;
  drawText("Hamamatsu Spec Maximum at V_{^{op}}", specmax_position, 0.85, false, kBlack, 0.03)->SetTextAngle(270);
  
  // Legend for the two histograms
  TLegend* dark_current_legend = new TLegend(0.2, 0.68, 0.5, 0.85);
  dark_current_legend->SetLineWidth(0);
  dark_current_legend->AddEntry(hist_dark_current_undervoltage, "I_{dark} at V = (V_{br} #minus 3)", "f");
  dark_current_legend->AddEntry(hist_dark_current_overvoltage, "I_{dark} at V = (V_{br} + 4)", "f");
  dark_current_legend->Draw();
  
  // Add a note for which SiPM trays are included in the data
  float hamamatsu_tray_xpos = specmax_position + 0.06;
  float hamamatsu_tray_ypos = 1 - gPad->GetTopMargin() - 0.06;
  drawText(Form("Hamamatsu #bf{%s}",Hamamatsu_SiPM_Code), 0.97, hamamatsu_tray_ypos+0.075, true, kBlack, 0.035);
  drawText("Data SiPM Tray IDs:", hamamatsu_tray_xpos-0.02, hamamatsu_tray_ypos, false, kBlack, 0.034);
  for (int i_tray = 1; i_tray <= gReader->GetTrayStrings()->size(); ++i_tray) {
    if (i_tray % 2 == 0) {
      drawText(Form("%s", gReader->GetTrayStrings()->at(i_tray-1).c_str()), hamamatsu_tray_xpos + 0.14, hamamatsu_tray_ypos - 0.03*std::floor((i_tray+1)/2), false, kBlack, 0.03);
    } else {
      drawText(Form("%s", gReader->GetTrayStrings()->at(i_tray-1).c_str()), hamamatsu_tray_xpos, hamamatsu_tray_ypos - 0.03*std::floor((i_tray+1)/2), false, kBlack, 0.03);
    }
  }
  
  // Mark note counts over spec maxiumum
  float overint_ypos = hamamatsu_tray_ypos - 0.03*std::floor((gReader->GetTrayStrings()->size()+1)/2) - 0.08;
  drawText("SiPMs over spec max", hamamatsu_tray_xpos-0.02, overint_ypos, false, kBlack, 0.035);
  drawText(Form("(%.1f nA at V_{br} + 4): ",Hamamatsu_spec_max_Idark), hamamatsu_tray_xpos-0.02, overint_ypos-0.04, false, kBlack, 0.035);
  int count_overmax = countDarkCurrentOverLimitAllTrays(Hamamatsu_spec_max_Idark);
  int count_total = countSiPMsAllTrays();
  float percent_overmax = 100. * count_overmax / count_total;
  drawText(Form("#color[2]{#bf{%i}} of %i (#color[2]{#bf{%.1f}}%%)",count_overmax, count_total, percent_overmax),
           hamamatsu_tray_xpos-0.02, overint_ypos - 0.1, false, kBlack, 0.035);
  
  
  // Draw some text giving info on the setup
  drawText("#bf{ePIC} Test Stand", 0.1, 0.957, false, kBlack, 0.04);
  drawText("#bf{Debrecen} SiPM Test Setup @ #bf{Yale}", 0.1, 0.915, false, kBlack, 0.04);
  
  //save histograms
  gCanvas_solo->SaveAs("../plots/single_plots/IV_scan/dark_current_histograms.pdf");
  
  return;
}// End of sipm_batch_summary_sheet::makeHist_DarkCurrent

//========================================================================== Solo plot generating Macros: Indexed series



// Construct a scatter plot of V_peak and V_bd vs SiPM index for each SiPM tray
// This enables one to clearly see systematic trends/compare outliers over testing time
//
// TODO return TObjectArray for summary sheet
void makeIndexSeries(bool flag_run_at_25_celcius) {
  
  // Set up canvas
  gCanvas_solo->Clear();
  gCanvas_solo->SetCanvasSize(1500, 600);
  gCanvas_solo->cd();
  gPad->SetTicks(1,1);
  gPad->SetRightMargin(0.02);
  gPad->SetLeftMargin(0.06);
  gPad->SetTopMargin(0.11);
  
  // Iterate over all available data
  for (int i_tray = 0; i_tray < gReader->GetTrayStrings()->size(); ++i_tray) {
    
    int IV_size = gReader->GetIV()->at(i_tray)->IV_Vpeak->size();
    int SPS_size = gReader->GetSPS()->at(i_tray)->SPS_Vbd->size();
    if (IV_size != SPS_size) {
      std::cout << "Warning in <sipm_batch_summary_sheet::makeIndexSeries>: SPS and IV arrays are unequal size!" << std::endl;
      std::cout << "All data will be plotted and indices will be assumed regularly correlated, take caution that this is handled correctly." << std::endl;
    }
    
    // Make histograms
    TH1F* hist_indexed_Vpeak = new TH1F("hist_indexed_Vpeak", 
                                        ";SiPM index [flattened];V_{br} [V]",
                                        IV_size, 0, IV_size);
    TH1F* hist_indexed_Vbreakdown = new TH1F("hist_indexed_Vbreakdown",
                                            ";SiPM index [flattened];V_{br} [V]",
                                            SPS_size, 0, SPS_size);
    
    // Append all data
    if (flag_run_at_25_celcius) {
      for (int i_IV = 1; i_IV <= IV_size; ++i_IV) {
//        if (gReader->GetIV()->at(i_tray)->IV_Vpeak_25C->at(i_IV) == 0) continue;
//        hist_indexed_Vpeak->SetBinContent(i_IV, gReader->GetIV()->at(i_tray)->avg_temp->at(i_IV - 1)); // Testing temperature, delete this!!!
        hist_indexed_Vpeak->SetBinContent(i_IV, gReader->GetIV()->at(i_tray)->IV_Vpeak_25C->at(i_IV - 1));
      }
      for (int i_SPS = 1; i_SPS <= SPS_size; ++i_SPS) {
//        if (gReader->GetSPS()->at(i_tray)->SPS_Vbd_25C->at(i_SPS - 1) == 0) continue;
//        hist_indexed_Vbreakdown->SetBinContent(i_SPS, gReader->GetSPS()->at(i_tray)->avg_temp->at(i_SPS - 1)); // Testing temperature, delete this!!!
        hist_indexed_Vbreakdown->SetBinContent(i_SPS, gReader->GetSPS()->at(i_tray)->SPS_Vbd_25C->at(i_SPS - 1));
      }
    } else {
      for (int i_IV = 1; i_IV <= IV_size; ++i_IV) {
//        if (gReader->GetIV()->at(i_tray)->IV_Vpeak->at(i_IV - 1) == 0) continue;
        hist_indexed_Vpeak->SetBinContent(i_IV, gReader->GetIV()->at(i_tray)->IV_Vpeak->at(i_IV - 1));
      }
      for (int i_SPS = 1; i_SPS <= SPS_size; ++i_SPS) {
//        if (gReader->GetSPS()->at(i_tray)->SPS_Vbd->at(i_SPS - 1) == 0) continue;
        hist_indexed_Vbreakdown->SetBinContent(i_SPS, gReader->GetSPS()->at(i_tray)->SPS_Vbd->at(i_SPS - 1));
      }
    }
    
    // Gather average V_bd for the tray
    double avg_voltages[2];
    if (flag_use_all_trays_for_averages) {
      avg_voltages[0] = getAvgVpeakAllTrays(flag_run_at_25_celcius); //IV
      avg_voltages[1] = getAvgVbreakdownAllTrays(flag_run_at_25_celcius); //SPS
    } else {
      avg_voltages[0] = getAvgVpeak(i_tray, flag_run_at_25_celcius); //IV
      avg_voltages[1] = getAvgVbreakdown(i_tray, flag_run_at_25_celcius); //SPS
    }
    
    // Set plot range--dynamically based on the results of the testing
    double aspect_separation = (avg_voltages[0] - avg_voltages[1])/0.55;
    double voltplot_limits[2] = {
      avg_voltages[1] - 0.2 *aspect_separation,
      avg_voltages[0] + 0.25*aspect_separation
    };
    
    //Debug
//    std::cout << "Average Vbr :: IV = " << avg_voltages[0] << ", SPS = " << avg_voltages[1] << "]." << std::endl;
//    std::cout << "Voltplot limits :: [" << voltplot_limits[0] << ',' << voltplot_limits[1] << "]." << std::endl;
    
    if (voltplot_limits[1] - voltplot_limits[0] < 0.25) {
      voltplot_limits[0] = voltplot_limits_static[0];
      voltplot_limits[1] = voltplot_limits_static[1];
    }
    
    // *-- plot histograms to represent the indexed SiPM test results
    
    // Set up the canvas/draw established hists
    gCanvas_solo->cd();
    hist_indexed_Vpeak->GetYaxis()->SetRangeUser(voltplot_limits[0], voltplot_limits[1]); // This line to comment out when swapping to temperature
    hist_indexed_Vpeak->GetYaxis()->SetTitleOffset(0.85);
    hist_indexed_Vpeak->SetMarkerColor(plot_colors[0]);
    hist_indexed_Vpeak->SetMarkerStyle(20);
    hist_indexed_Vpeak->Draw("hist p");
    
    hist_indexed_Vbreakdown->SetMarkerColor(plot_colors[1]);
    hist_indexed_Vbreakdown->SetMarkerStyle(21);
    
    // Draw reference averaged +/- 50 MV lines
    TLine* avg_line = new TLine();
    
    // Average line: V_peak (IV)
    avg_line->SetLineColor(kBlack);
    avg_line->DrawLine(0, avg_voltages[0], IV_size, avg_voltages[0]);
    avg_line->SetLineColor(kGray+2);
    avg_line->SetLineStyle(7);
    avg_line->DrawLine(0, avg_voltages[0]+0.05, IV_size, avg_voltages[0]+0.05);
    avg_line->DrawLine(0, avg_voltages[0]-0.05, IV_size, avg_voltages[0]-0.05);
    
    // Average line: V_breakdown (SPS)
    avg_line->SetLineStyle(1);
    avg_line->SetLineColor(kBlack);
    avg_line->DrawLine(0, avg_voltages[1], IV_size, avg_voltages[1]);
    avg_line->SetLineColor(kGray+2);
    avg_line->SetLineStyle(7);
    avg_line->DrawLine(0, avg_voltages[1]+0.05, IV_size, avg_voltages[1]+0.05);
    avg_line->DrawLine(0, avg_voltages[1]-0.05, IV_size, avg_voltages[1]-0.05);
    
    // Cassette test lines
    TLine* cassette_line = new TLine();
    cassette_line->SetLineColor(kGray+1);
    cassette_line->SetLineStyle(6);
    for (int i = 1; i <= 14; ++i) cassette_line->DrawLine(32*i, voltplot_limits[0], 32*i, voltplot_limits[1]);
    
    // assure points sit on top of lines
    hist_indexed_Vpeak->Draw("hist p same");
    hist_indexed_Vbreakdown->Draw("hist p same");
    
    // Legend for labeling the two V_breakdown measurement types
    float leg_extra_space = 0;
    if (flag_run_at_25_celcius) leg_extra_space = 0.04;
    TLegend* vbd_legend = new TLegend(0.635, 0.36 + leg_extra_space, 0.90, 0.51 + leg_extra_space);
    vbd_legend->SetLineWidth(0);
    vbd_legend->AddEntry(hist_indexed_Vpeak,       Form("IV V_{bd} #kern[0.3]{(#color[2]{%i} outliers)}",
                                                        countOutliersVpeak(i_tray, flag_run_at_25_celcius)), "p");
    vbd_legend->AddEntry(hist_indexed_Vbreakdown,  Form("SPS V_{bd} #kern[0.1]{(#color[2]{%i} outliers)}",
                                                        countOutliersVbreakdown(i_tray, flag_run_at_25_celcius)), "p");
    vbd_legend->Draw();
    
    // Legend for the lines marking tray average, test sets
    TLegend* line_legend = new TLegend(0.15, 0.315 + leg_extra_space, 0.45, 0.55 + leg_extra_space);
    line_legend->SetLineWidth(0);
    hist_indexed_Vpeak->SetLineColor(kBlack);
    if (flag_use_all_trays_for_averages) line_legend->AddEntry(hist_indexed_Vpeak, "Average over all trays", "l");
    else                                 line_legend->AddEntry(hist_indexed_Vpeak, "Average over tray", "l");
    line_legend->AddEntry(avg_line, "Average #pm 50mV", "l");
    line_legend->AddEntry(cassette_line, "Test Runs (32 SiPM per test)", "l");
    line_legend->Draw();
    
    
    // Draw some text giving info on the setup
    drawText("#bf{Debrecen} SiPM Test Setup @ #bf{Yale}", 0.06, 0.91, false, kBlack, 0.045);
    drawText("#bf{ePIC} Test Stand", 0.06, 0.955, false, kBlack, 0.045);
    drawText(Form("Hamamatsu #bf{%s} Tray #%s", Hamamatsu_SiPM_Code, gReader->GetTrayStrings()->at(i_tray).c_str()), 0.98, 0.95, true, kBlack, 0.05);
    drawText(Form("%s", string_tempcorr[flag_run_at_25_celcius]), 0.98, 0.905, true, kBlack, 0.04);
    
    
    //save histograms
    gCanvas_solo->SaveAs(Form("../plots/single_plots/indexed%s/%s_indexed_Vbd%s.pdf",
                              string_tempcorr_short[flag_run_at_25_celcius],
                              gReader->GetTrayStrings()->at(i_tray).c_str(),
                              string_tempcorr_short[flag_run_at_25_celcius]));
    
    delete hist_indexed_Vpeak;
    delete hist_indexed_Vbreakdown;
  }
  
  return;
}// End of sipm_batch_summary_sheet::makeIndexSeries



// Construct a scatter plot of V_peak and V_bd vs SiPM index for each SiPM tray
// This enables one to clearly see systematic trends/compare outliers over testing time
//
// TODO return TObjectArray for summary sheet
void makeIndexDifference(bool flag_run_at_25_celcius) {
  
  // Set up canvas
  gCanvas_solo->Clear();
  gCanvas_solo->SetCanvasSize(1500, 450);
  gCanvas_solo->cd();
  gPad->SetTicks(1,1);
  gPad->SetRightMargin(0.02);
  gPad->SetLeftMargin(0.06);
  gPad->SetTopMargin(0.11);
  
  // Iterate over all available data
  for (int i_tray = 0; i_tray < gReader->GetTrayStrings()->size(); ++i_tray) {
    
    int IV_size = gReader->GetIV()->at(i_tray)->IV_Vpeak->size();
    int SPS_size = gReader->GetSPS()->at(i_tray)->SPS_Vbd->size();
    if (IV_size != SPS_size) {
      std::cout << "Warning in <sipm_batch_summary_sheet::makeIndexSeries>: SPS and IV arrays are unequal size!" << std::endl;
      std::cout << "All data will be plotted and indices will be assumed regularly correlated, take caution that this is handled correctly." << std::endl;
    }
    
    // Make histograms
    TH1F* hist_indexed_Vdiff = new TH1F("hist_indexed_Vdiff",
                                        ";SiPM index [flattened];Difference V_{bd}^{IV} - V_{bd}^{SPS} [V]",
                                        IV_size, 0, IV_size);
    
    // Append all data
    if (flag_run_at_25_celcius) {
      for (int i_IV = 1; i_IV <= IV_size; ++i_IV) {
        hist_indexed_Vdiff->SetBinContent(i_IV,
                                          std::fabs(gReader->GetIV()->at(i_tray)->IV_Vpeak_25C->at(i_IV - 1) -
                                                    gReader->GetSPS()->at(i_tray)->SPS_Vbd_25C->at(i_IV - 1) ) );
      }
    } else {
      for (int i_IV = 1; i_IV <= IV_size; ++i_IV) {
        hist_indexed_Vdiff->SetBinContent(i_IV,
                                          std::fabs(gReader->GetIV()->at(i_tray)->IV_Vpeak->at(i_IV - 1) -
                                                    gReader->GetSPS()->at(i_tray)->SPS_Vbd->at(i_IV - 1) ) );
      }
    }// End of bin content setting
    
    // Gather average V_bd for the tray
    double avg_diff;
    if (flag_use_all_trays_for_averages) {
      avg_diff = std::fabs(getAvgVpeakAllTrays(flag_run_at_25_celcius) -
                           getAvgVbreakdownAllTrays(flag_run_at_25_celcius) );
    } else {
      avg_diff = std::fabs(getAvgVpeak(i_tray, flag_run_at_25_celcius) -
                           getAvgVbreakdown(i_tray, flag_run_at_25_celcius) ); //SPS
    }
    
    // Set plot to be used in displaying the data
    double voltplot_limits[2] = {
      0.45, 0.625
    };
    
    // *-- plot histograms to represent the indexed SiPM test results
    
    // Set up the canvas/draw established hists
    gCanvas_solo->cd();
    hist_indexed_Vdiff->GetYaxis()->SetRangeUser(voltplot_limits[0], voltplot_limits[1]);
    hist_indexed_Vdiff->GetYaxis()->SetTitleOffset(0.85);
    hist_indexed_Vdiff->SetMarkerColor(plot_colors[2]);
    hist_indexed_Vdiff->SetMarkerStyle(20);
    hist_indexed_Vdiff->Draw("hist p");
    
    
    // Draw reference averaged difference
    TLine* avg_line = new TLine();
    
    // Average difference line
    avg_line->SetLineColor(kBlack);
    avg_line->DrawLine(0, avg_diff, IV_size, avg_diff);
    
    // Cassette test lines
    TLine* cassette_line = new TLine();
    cassette_line->SetLineColor(kGray+1);
    cassette_line->SetLineStyle(6);
    for (int i = 1; i <= 14; ++i) cassette_line->DrawLine(32*i, voltplot_limits[0], 32*i, voltplot_limits[1]);
    
    // assure points sit on top of lines
    hist_indexed_Vdiff->Draw("hist p same");
    
    // Legend for labeling the two V_breakdown measurement types
//    TLegend* vbd_legend = new TLegend(0.635, 0.15, 0.90, 0.35);
//    vbd_legend->SetLineWidth(0);
//    vbd_legend->AddEntry(hist_indexed_Vdiff, "Difference V_{bd}^{IV} - V_{bd}^{SPS}", "p");
//    vbd_legend->Draw();
    
    // Legend for the lines marking tray average, test sets
    TLegend* line_legend = new TLegend(0.15, 0.15, 0.45, 0.35);
    line_legend->SetLineWidth(0);
    hist_indexed_Vdiff->SetLineColor(kBlack);
//    line_legend->AddEntry(hist_indexed_Vdiff, "Difference V_{bd}^{IV} - V_{bd}^{SPS}", "p");
    if (flag_use_all_trays_for_averages) line_legend->AddEntry(hist_indexed_Vdiff, Form("Average over all trays (#color[2]{%.3f})",avg_diff), "l");
    else                                 line_legend->AddEntry(hist_indexed_Vdiff, Form("Average over tray (#color[2]{%.3f})",avg_diff), "l");
    line_legend->AddEntry(cassette_line, "Test Runs (32 SiPM per test)", "l");
    line_legend->Draw();
    
    
    // Draw some text giving info on the setup
    drawText("#bf{Debrecen} SiPM Test Setup @ #bf{Yale}", 0.06, 0.91, false, kBlack, 0.045);
    drawText("#bf{ePIC} Test Stand", 0.06, 0.955, false, kBlack, 0.045);
    drawText(Form("Hamamatsu #bf{%s} Tray #%s", Hamamatsu_SiPM_Code, gReader->GetTrayStrings()->at(i_tray).c_str()), 0.98, 0.95, true, kBlack, 0.05);
    drawText(Form("%s", string_tempcorr[flag_run_at_25_celcius]), 0.98, 0.905, true, kBlack, 0.04);
    
    
    //save histograms
    gCanvas_solo->SaveAs(Form("../plots/single_plots/indexed_diff%s/%s_indexed_diff_Vbd%s.pdf",
                              string_tempcorr_short[flag_run_at_25_celcius],
                              gReader->GetTrayStrings()->at(i_tray).c_str(),
                              string_tempcorr_short[flag_run_at_25_celcius]));
    
    delete hist_indexed_Vdiff;
  }// End of loop on trays
  
  return;
}// End of sipm_batch_summary_sheet::makeIndexDifference



//========================================================================== Tray indexed plot macros (for all data with each point as a tray avg)

// Make brief indexed tray V_breakdown measurement summary
// This will show the average V_brakdown from IV and SPS measurements
// From each tray with errors representing the spread of data for each tray
//
// TODO return TObjectArray for summary sheet
void makeIndexedTray(bool flag_run_at_25_celcius) {
  const bool debug_tray_index = false;
  const int n_trays = gReader->GetIV()->size();
  const int lim_trays = 8; // threshold below which to reformat the plot for few trays
  
  // Set up canvas dynamically based on the number of trays
  gCanvas_double->cd();
  gCanvas_double->Clear();
  gCanvas_double->SetCanvasSize(300+50*n_trays, 750);
  cpads.clear();
  cpads.push_back(std::vector<TPad*>());
  cpads[0].push_back(buildPad("index_tray_0", 0, 1./3, 1, 1));
  cpads[0].push_back(buildPad("index_tray_1", 0, 0, 1, 1./3));
  
  // Set up main pad: 300+50*n x 500
  cpads[0][0]->cd();
  const float aspect_ratio = static_cast<float>(300+50*n_trays)/500.;
  gPad->SetTicks(1,1);
  gPad->SetRightMargin(0.01+0.01*aspect_ratio);
  gPad->SetLeftMargin(0.16-0.03*aspect_ratio);
  gPad->SetTopMargin(0.11);
  gPad->SetBottomMargin(0.005);
  double plot_window_size_x = 1.0 - gPad->GetRightMargin() - gPad->GetLeftMargin();
  
  // Set up secondary pad: 300+40*n x 250
  cpads[0][1]->cd();
  gPad->SetTicks(1,1);
  gPad->SetRightMargin(cpads[0][0]->GetRightMargin());
  gPad->SetLeftMargin(cpads[0][0]->GetLeftMargin());
  gPad->SetTopMargin(0.01);
  if (n_trays > lim_trays) gPad->SetBottomMargin(2*0.11);
  else                     gPad->SetBottomMargin(2*0.09);

  // Initialize Histograms
  TH1F* hist_indexed_Vpeak_tray = new TH1F("hist_indexed_Vpeak_tray",
                                           ";Hamamatsu Tray Number;V_{br} [V]",
                                           n_trays, 0, n_trays);
  TH1F* hist_indexed_Vbreakdown_tray = new TH1F("hist_indexed_Vbreakdown",
                                                ";Hamamatsu Tray Number;V_{br} [V]",
                                                n_trays, 0, n_trays);
  TH1F* hist_indexed_Vbreakdown_nominal = new TH1F("hist_indexed_Vbreakdown_nominal",
                                                   ";Hamamatsu Tray Number;V_{Br} [V]",
                                                   n_trays, 0, n_trays);
  TH1F* hist_diffnominal_Vpeak = new TH1F("hist_diffnominal_Vpeak",
                                           ";Hamamatsu Tray Number;V_{br} #minus V_{br, Nominal} [V]",
                                           n_trays, 0, n_trays);
  TH1F* hist_diffnominal_Vbreakdown = new TH1F("hist_diffnominal_Vbreakdown",
                                                ";Hamamatsu Tray Number;V_{br} #minus V_{br, Nominal} [V]",
                                                n_trays, 0, n_trays);
  
  // Alphabetize, separate by batch
  if (debug_tray_index) std::cout << "Sorted list : " << std::endl;
  int tray_reshuffle_index[n_trays];
  for (int i = 0; i < n_trays; ++i) tray_reshuffle_index[i] = i;
  std::string ref_string;
  for (int i_tray = 0; i_tray < n_trays; ++i_tray) {
    ref_string = gReader->GetTrayStrings()->at(tray_reshuffle_index[i_tray]);
    for (int j_tray = i_tray + 1; j_tray < n_trays; ++j_tray) {
      if (ref_string.compare(gReader->GetTrayStrings()->at(tray_reshuffle_index[j_tray])) > 0) {
        ref_string = gReader->GetTrayStrings()->at(tray_reshuffle_index[j_tray]);
        
        // index swap necessary to ensure that the same tray isn't repeated
        int temp = tray_reshuffle_index[i_tray];
        tray_reshuffle_index[i_tray] = tray_reshuffle_index[j_tray];
        tray_reshuffle_index[j_tray] = temp;
      }
    }// End of next element finding
    
    // Assign to reshuffled list
    if (debug_tray_index) std::cout << gReader->GetTrayStrings()->at(tray_reshuffle_index[i_tray]) << std::endl;
    ref_string = gReader->GetTrayStrings()->at(tray_reshuffle_index[i_tray]);
  }// End of alphabetize/tray sort
  
  // Gather avg data for tray measurements and add to histogram
  for (int i_tray = 0; i_tray < n_trays; ++i_tray) {
    int i_fill = tray_reshuffle_index[i_tray];
    hist_indexed_Vpeak_tray->GetXaxis()->SetBinLabel(i_tray + 1, gReader->GetTrayStrings()->at(i_fill).c_str());
    hist_diffnominal_Vpeak->GetXaxis()->SetBinLabel(i_tray + 1, gReader->GetTrayStrings()->at(i_fill).c_str());
    
    hist_indexed_Vpeak_tray->SetBinContent(i_tray + 1, getAvgVpeak(i_fill, flag_run_at_25_celcius));
    hist_indexed_Vpeak_tray->SetBinError(i_tray + 1, getStdevVpeak(i_fill, flag_run_at_25_celcius));

    hist_indexed_Vbreakdown_tray->SetBinContent(i_tray + 1, getAvgVbreakdown(i_fill, flag_run_at_25_celcius));
    hist_indexed_Vbreakdown_tray->SetBinError(i_tray + 1, getStdevVbreakdown(i_fill, flag_run_at_25_celcius));
  }// End of hist filling
  
  // Gather nominal data reported by Hamamatsu, stored in separate file
  std::ifstream nominal_file("../tray_nominal_data.txt");
  std::string line;
  while (getline(nominal_file, line)) {
    std::stringstream linestream(line);
    std::string entry;
    std::string reldata[2] = {"",""};
    while (getline(linestream, entry, ' ')) {
      if (entry.size() == 0) continue; // Repeated spaces
      if (entry[0] == '#') continue; // Comments
      
      // Gather relevant data
      if (reldata[0].compare("") == 0) reldata[0] = entry;
      else if (reldata[1].compare("") == 0) reldata[1] = entry;
      else break;
    }
    
    // search for matching trays among the gathered data
    for (int i_tray = 0; i_tray < n_trays; ++i_tray) {
      int i_fill = tray_reshuffle_index[i_tray];
      if (reldata[0].compare(gReader->GetTrayStrings()->at(i_fill)) == 0) {
        if (debug_tray_index) {
          std::cout << "Matching nominal found on tray " << t_blu << reldata[0] << t_def << " :: ";
          std::cout << t_red << std::stof(reldata[1]) << t_def << std::endl;
        }
        
        hist_indexed_Vbreakdown_nominal->SetBinContent(i_tray + 1, std::stof(reldata[1]) - 4.0);
        hist_indexed_Vbreakdown_nominal->SetBinError(i_tray + 1, 0);
        
        // Set hists for difference against the nominal
        hist_diffnominal_Vpeak->SetBinContent(i_tray + 1,       hist_indexed_Vpeak_tray->GetBinContent(i_tray + 1) - std::stof(reldata[1]) + 4.0);
        hist_diffnominal_Vpeak->SetBinError(i_tray + 1,         hist_indexed_Vpeak_tray->GetBinError(i_tray + 1));
        hist_diffnominal_Vbreakdown->SetBinContent(i_tray + 1,  hist_indexed_Vbreakdown_tray->GetBinContent(i_tray + 1) - std::stof(reldata[1]) + 4.0);
        hist_diffnominal_Vbreakdown->SetBinError(i_tray + 1,    hist_indexed_Vbreakdown_tray->GetBinError(i_tray + 1));
      }
    }// End of data matching
  }// End of nominal data gathering
  
  // Format histograms
  cpads[0][0]->cd();
  if (n_trays > lim_trays) hist_indexed_Vpeak_tray->GetXaxis()->SetTitleOffset(1.40);
  hist_indexed_Vpeak_tray->GetYaxis()->SetRangeUser(voltplot_limits_static[0], voltplot_limits_static[1]);
  hist_indexed_Vpeak_tray->GetYaxis()->SetTitleOffset(0.6 + 0.8/aspect_ratio);
  hist_indexed_Vpeak_tray->SetLineColor(plot_colors[0]);
  hist_indexed_Vpeak_tray->SetLineWidth(2);
  hist_indexed_Vpeak_tray->SetFillColorAlpha(plot_colors[0],0);
  hist_indexed_Vpeak_tray->SetMarkerColor(plot_colors[0]);
  hist_indexed_Vpeak_tray->SetMarkerStyle(20);
  hist_indexed_Vpeak_tray->SetMarkerSize(1.0 + 1.0/aspect_ratio);
  hist_indexed_Vpeak_tray->Draw("b p e1 x0");
  
  hist_indexed_Vbreakdown_tray->SetLineColor(plot_colors[1]);
  hist_indexed_Vbreakdown_tray->SetLineWidth(2);
  hist_indexed_Vbreakdown_tray->SetFillColorAlpha(plot_colors[1],0);
  hist_indexed_Vbreakdown_tray->SetMarkerColor(plot_colors[1]);
  hist_indexed_Vbreakdown_tray->SetMarkerStyle(21);
  hist_indexed_Vbreakdown_tray->SetMarkerSize(1.0 + 1.0/aspect_ratio);
  
  hist_indexed_Vbreakdown_nominal->SetLineColor(kBlack);
  hist_indexed_Vbreakdown_nominal->SetLineWidth(2);
  hist_indexed_Vbreakdown_nominal->SetFillColorAlpha(kBlack,0);
  hist_indexed_Vbreakdown_nominal->SetMarkerColor(kBlack);
  hist_indexed_Vbreakdown_nominal->SetMarkerStyle(53);
  hist_indexed_Vbreakdown_nominal->SetMarkerSize(1.0 + 1.0/aspect_ratio);
  
  cpads[0][1]->cd();
  if (n_trays > lim_trays) hist_diffnominal_Vpeak->GetXaxis()->SetTitleOffset(1.40);
  hist_diffnominal_Vpeak->GetXaxis()->SetTickSize(2.0 * hist_diffnominal_Vpeak->GetXaxis()->GetTickLength());
  hist_diffnominal_Vpeak->GetXaxis()->SetTitleSize(2.0 * hist_diffnominal_Vpeak->GetXaxis()->GetTitleSize());
  hist_diffnominal_Vpeak->GetXaxis()->SetLabelSize(2.0 * hist_diffnominal_Vpeak->GetXaxis()->GetLabelSize());
  hist_diffnominal_Vpeak->GetXaxis()->SetLabelOffset(2.0 * hist_diffnominal_Vpeak->GetXaxis()->GetLabelOffset());
  hist_diffnominal_Vpeak->GetYaxis()->SetNdivisions(505);
  hist_diffnominal_Vpeak->GetYaxis()->SetLabelSize(2.0 * hist_diffnominal_Vpeak->GetYaxis()->GetLabelSize());
  hist_diffnominal_Vpeak->GetYaxis()->SetTitleSize(2.0 * hist_diffnominal_Vpeak->GetYaxis()->GetTitleSize());
  hist_diffnominal_Vpeak->GetYaxis()->SetRangeUser(diffplot_limits_static[0], diffplot_limits_static[1]);
  hist_diffnominal_Vpeak->GetYaxis()->SetTitleOffset(0.5*(0.6 + 0.8/aspect_ratio));
  hist_diffnominal_Vpeak->SetLineColor(plot_colors[0]);
  hist_diffnominal_Vpeak->SetLineWidth(hist_indexed_Vpeak_tray->GetLineWidth());
  hist_diffnominal_Vpeak->SetFillColorAlpha(plot_colors[0], 0);
  hist_diffnominal_Vpeak->SetMarkerColor(plot_colors[0]);
  hist_diffnominal_Vpeak->SetMarkerStyle(hist_indexed_Vpeak_tray->GetMarkerStyle());
  hist_diffnominal_Vpeak->SetMarkerSize(hist_indexed_Vpeak_tray->GetMarkerSize());
  hist_diffnominal_Vpeak->Draw("b p e1 x0");
  
  hist_diffnominal_Vbreakdown->SetLineColor(plot_colors[1]);
  hist_diffnominal_Vbreakdown->SetLineWidth(hist_indexed_Vbreakdown_tray->GetLineWidth());
  hist_diffnominal_Vbreakdown->SetFillColorAlpha(plot_colors[1], 0);
  hist_diffnominal_Vbreakdown->SetMarkerColor(plot_colors[1]);
  hist_diffnominal_Vbreakdown->SetMarkerStyle(hist_indexed_Vbreakdown_tray->GetMarkerStyle());
  hist_diffnominal_Vbreakdown->SetMarkerSize(hist_indexed_Vbreakdown_tray->GetMarkerSize());
  
  // Draw reference averaged +/- 50 MV lines, average over all trays
  double avg_voltages[2];
  avg_voltages[0] = getAvgVpeakAllTrays(flag_run_at_25_celcius); //IV
  avg_voltages[1] = getAvgVbreakdownAllTrays(flag_run_at_25_celcius); //SPS
  
  // TObjects for drawing
  TLine* avg_line = new TLine();
  TLine* dev_line = new TLine();
  
  // Unity line for deviation plot
  cpads[0][1]->cd();
  dev_line->SetLineColor(kGray + 1);
  dev_line->DrawLine(0, 0, n_trays, 0);
  
  // Average line: V_peak (IV)
  cpads[0][0]->cd();
  avg_line->SetLineColor(kBlack);
  avg_line->DrawLine(0, avg_voltages[0], n_trays, avg_voltages[0]);
  dev_line->SetLineColor(kGray+2);
  dev_line->SetLineStyle(7);
  dev_line->DrawLine(0, avg_voltages[0]+0.05, n_trays, avg_voltages[0]+0.05);
  dev_line->DrawLine(0, avg_voltages[0]-0.05, n_trays, avg_voltages[0]-0.05);
  
  // Average line: V_breakdown (SPS)
  avg_line->DrawLine(0, avg_voltages[1], n_trays, avg_voltages[1]);
  dev_line->DrawLine(0, avg_voltages[1]+0.05, n_trays, avg_voltages[1]+0.05);
  dev_line->DrawLine(0, avg_voltages[1]-0.05, n_trays, avg_voltages[1]-0.05);
  
  
  
  // SiPM batch delimeter lines, dynamic to the input data
  TLine* batch_line = new TLine();
  batch_line->SetLineColor(kGray+1);
  batch_line->SetLineStyle(6);
  std::string last_batch;
  float start_of_batch = 0.5;
  for (int i_tray = 0; i_tray < n_trays; ++i_tray) {
    std::stringstream traystream(gReader->GetTrayStrings()->at(tray_reshuffle_index[i_tray]));
    if (i_tray == 0) {getline(traystream, last_batch, '-'); continue;}
    
    std::string next_batch;
    getline(traystream, next_batch, '-');
    if (last_batch.compare(next_batch) != 0 || i_tray == n_trays - 1) {
      // draw new batch delimiter
      if (last_batch.compare(next_batch) != 0) {
        cpads[0][1]->cd();
        batch_line->DrawLine(i_tray, diffplot_limits_static[0], i_tray, diffplot_limits_static[1]);
        cpads[0][0]->cd();
        batch_line->DrawLine(i_tray, voltplot_limits_static[0], i_tray, voltplot_limits_static[1]);
      }
      
      // Label the batch on the plot
      double batch_text_x = gPad->GetLeftMargin() + plot_window_size_x * static_cast<float>(start_of_batch)/n_trays + 0.01;
      drawText(Form("Batch %s", last_batch.c_str()), batch_text_x, 0.81, false, kBlack, 0.0375);
      if (last_batch.compare("250717") == 0) drawText("(ORNL)", batch_text_x, 0.77, false, kBlack, 0.0375);
      
      // continue to next batch
      last_batch = next_batch;
      start_of_batch = i_tray;
    }
  }// End of batch delimiter lines
  
  
  
  // assure points sit on top of lines
  cpads[0][0]->cd();
  hist_indexed_Vpeak_tray->Draw("b p e1 x0 same");
  hist_indexed_Vbreakdown_tray->Draw("b p e1 x0 same");
  hist_indexed_Vbreakdown_nominal->Draw("b p e1 x0 same");
  
  // Legend for labeling the two V_breakdown measurement types
  double first_x_margin = gPad->GetLeftMargin() + plot_window_size_x * 5.0/n_trays;
  TLegend* vbd_legend = new TLegend(0.15, 0.05, first_x_margin - 0.02, 0.27);
  vbd_legend->SetLineWidth(0);
  vbd_legend->AddEntry(hist_indexed_Vbreakdown_nominal, "Hamamatsu Nominal", "p");
  vbd_legend->AddEntry(hist_indexed_Vpeak_tray, "IV V_{bd} (also called V_{peak})", "p");
  vbd_legend->AddEntry(hist_indexed_Vbreakdown_tray, "SPS V_{bd}", "p");
  vbd_legend->Draw();
  
  // Draw some text giving info on the setup
  double right_text_margin = gPad->GetRightMargin() - (n_trays <= lim_trays)*0.01;
  double left_text_margin = gPad->GetLeftMargin() - (n_trays <= lim_trays)*0.05;
  drawText("#bf{Debrecen} SiPM Test Setup @ #bf{Yale}",           left_text_margin, 0.91, false, kBlack, 0.04);
  drawText("#bf{ePIC} Test Stand",                                left_text_margin, 0.955, false, kBlack, 0.045);
  drawText(Form("Hamamatsu #bf{%s}", Hamamatsu_SiPM_Code),        1.0-right_text_margin, 0.95, true, kBlack, 0.045);
  drawText(Form("%s", string_tempcorr[flag_run_at_25_celcius]),   1.0-right_text_margin, 0.905, true, kBlack, 0.035);
  
  
  // Legend for the lines marking tray average, test sets
  TLegend* line_legend = new TLegend(first_x_margin + 0.02, 0.05, first_x_margin + plot_window_size_x * 5.0/n_trays - 0.02, 0.27);
  line_legend->SetLineWidth(0);
  line_legend->AddEntry(avg_line, Form("Average all trays #left[#splitline{IV      %.2f}{SPS  %.2f}#right]",avg_voltages[0],avg_voltages[1]), "l");
  line_legend->AddEntry(dev_line, "Average #pm 50mV", "l");
  line_legend->AddEntry(batch_line, "Batch Delimeter", "l");
  line_legend->Draw();
  
  // Second panel -- difference against nominal
  cpads[0][1]->cd();
  hist_diffnominal_Vpeak->Draw("b p e1 x0 same");
  hist_diffnominal_Vbreakdown->Draw("b p e1 x0 same");
  
  gCanvas_double->SaveAs(Form("../plots/batch_plots/batch_Vbr_trayavg%s.pdf",string_tempcorr_short[flag_run_at_25_celcius]));
  
  delete hist_indexed_Vpeak_tray;
  delete hist_indexed_Vbreakdown_tray;
  delete hist_indexed_Vbreakdown_nominal;
  
}// End of sipm_batch_summary_sheet::makeIndexedTray


// Make a summary plot of the number of outliers in each tray
// with and without systematic errors on the setup (should be found from
// detailed systematic analysis in systematic_analysis_summary.cc)
void makeIndexedOutliers(bool flag_run_at_25_celcius) {
  
  // Todo: 
  // lines for avg in batch
  // legends with info about sigma_syst
  // Add bin for total outliership of all trays
  
  const int n_trays = gReader->GetIV()->size();
  const int lim_trays = 8; // threshold below which to reformat the plot for few trays
  
  // Input values from systematic analysis (in V)
  float syst_error_results[2][2] = {
    {0.006943, 0.016606}, // Not temperature corrected
    {0.002184, 0.016333}  // Temperature corrected to 25C
  };
  
  // Set up canvas dynamically based on the number of trays
  gCanvas_double->cd();
  gCanvas_double->Clear();
  gCanvas_double->SetCanvasSize(300+50*n_trays, 800);
  cpads.clear();
  cpads.push_back(std::vector<TPad*>());
  cpads[0].push_back(buildPad("index_tray_0", 0, 0.5, 1, 1));
  cpads[0].push_back(buildPad("index_tray_1", 0, 0, 1, 0.5));
  
  // Set up main pad: 300+40*n x 500
  cpads[0][0]->cd();
  const float aspect_ratio = static_cast<float>(300+50*n_trays)/400.;
  gPad->SetTicks(1,1);
  gPad->SetRightMargin(0.01+0.01*aspect_ratio);
  gPad->SetLeftMargin(0.16-0.03*aspect_ratio);
  gPad->SetTopMargin(0.11);
  gPad->SetBottomMargin(0.005);
  double plot_window_size_x = 1.0 - gPad->GetRightMargin() - gPad->GetLeftMargin();
  
  // Set up secondary pad: 300+40*n x 250
  cpads[0][1]->cd();
  gPad->SetTicks(1,1);
  gPad->SetRightMargin(cpads[0][0]->GetRightMargin());
  gPad->SetLeftMargin(cpads[0][0]->GetLeftMargin());
  gPad->SetTopMargin(0.01);
  if (n_trays > lim_trays) gPad->SetBottomMargin(1.5*0.11);
  else                     gPad->SetBottomMargin(1.5*0.09);
  
  // Initialize Histograms
  char plus_types[2][10] = {"+","#oplus"};
  TH1F* hist_outliers_Vpeak = new TH1F("hist_outliers_Vpeak",
                                       ";Hamamatsu Tray Number;Outliership #pm50 mV [%]",
                                       n_trays, 0, n_trays);
  TH1F* hist_outliers_Vbreakdown = new TH1F("hist_outliers_Vbreakdown",
                                            ";Hamamatsu Tray Number;Outliership #pm50 mV [%]",
                                            n_trays, 0, n_trays);
  TH1F* hist_outliers_syst_Vpeak = new TH1F("hist_outliers_syst_Vpeak",
                                            Form(";Hamamatsu Tray Number;Outliership #pm(50 %s #sigma_{syst}) mV [%%]",
                                                 plus_types[use_quadrature_sum_for_syst_error]),
                                            n_trays, 0, n_trays);
  TH1F* hist_outliers_syst_Vbreakdown = new TH1F("hist_outliers_syst_Vbreakdown",
                                                 Form(";Hamamatsu Tray Number;Outliership #pm(50 %s #sigma_{syst}) mV [%%]",
                                                      plus_types[use_quadrature_sum_for_syst_error]),
                                                 n_trays, 0, n_trays);
  
  // Alphabetize, separate by batch
  int tray_reshuffle_index[n_trays];
  for (int i = 0; i < n_trays; ++i) tray_reshuffle_index[i] = i;
  std::string ref_string;
  for (int i_tray = 0; i_tray < n_trays; ++i_tray) {
    ref_string = gReader->GetTrayStrings()->at(tray_reshuffle_index[i_tray]);
    for (int j_tray = i_tray + 1; j_tray < n_trays; ++j_tray) {
      if (ref_string.compare(gReader->GetTrayStrings()->at(tray_reshuffle_index[j_tray])) > 0) {
        ref_string = gReader->GetTrayStrings()->at(tray_reshuffle_index[j_tray]);
        
        // index swap necessary to ensure that the same tray isn't repeated
        int temp = tray_reshuffle_index[i_tray];
        tray_reshuffle_index[i_tray] = tray_reshuffle_index[j_tray];
        tray_reshuffle_index[j_tray] = temp;
      }
    }// End of next element finding
    
    // Assign to reshuffled list
    ref_string = gReader->GetTrayStrings()->at(tray_reshuffle_index[i_tray]);
  }// End of alphabetize/tray sort
  
  // Gather avg data for tray measurements and add to histogram
  for (int i_tray = 0; i_tray < n_trays; ++i_tray) {
    int i_fill = tray_reshuffle_index[i_tray]; //sorted index
    hist_outliers_Vpeak->GetXaxis()->SetBinLabel(i_tray + 1, gReader->GetTrayStrings()->at(i_fill).c_str());
    hist_outliers_syst_Vpeak->GetXaxis()->SetBinLabel(i_tray + 1, gReader->GetTrayStrings()->at(i_fill).c_str());
    
    // could handle errors more dynamically... Maybe use SDOM on mean to find stat error by looking at means to different outliers?
    
    hist_outliers_Vpeak->SetBinContent(i_tray + 1, (static_cast<double>(countOutliersVpeak(i_fill, flag_run_at_25_celcius)) /
                                                    countValidSiPMs(i_fill) )*100);
    hist_outliers_Vpeak->SetBinError(i_tray + 1, 0.1);
    hist_outliers_Vbreakdown->SetBinContent(i_tray + 1, (static_cast<double>(countOutliersVbreakdown(i_fill, flag_run_at_25_celcius)) /
                                                         countValidSiPMs(i_fill) )*100);
    hist_outliers_Vbreakdown->SetBinError(i_tray + 1, 0.1);
    
    // + extra tolerance for systematic errors (defined above in this method)
    hist_outliers_syst_Vpeak->SetBinContent(i_tray + 1, (static_cast<double>(countOutliersVpeak(i_fill, flag_run_at_25_celcius,
                                                                                                syst_error_results[flag_run_at_25_celcius][0])) /
                                                         countValidSiPMs(i_fill) )*100);
    hist_outliers_syst_Vpeak->SetBinError(i_tray + 1, 0.1);
    hist_outliers_syst_Vbreakdown->SetBinContent(i_tray + 1, (static_cast<double>(countOutliersVbreakdown(i_fill, flag_run_at_25_celcius,
                                                                                                          syst_error_results[flag_run_at_25_celcius][1])) /
                                                              countValidSiPMs(i_fill) )*100);
    hist_outliers_syst_Vbreakdown->SetBinError(i_tray + 1, 0.1);
  }// End of hist filling
  
  // Format histograms
  cpads[0][0]->cd();
  double plotlim_outliers[2] = {0, 19};
  if (n_trays > lim_trays) hist_outliers_Vpeak->GetXaxis()->SetTitleOffset(1.40);
  hist_outliers_Vpeak->GetYaxis()->SetRangeUser(plotlim_outliers[0], plotlim_outliers[1]);
  hist_outliers_Vpeak->GetYaxis()->SetTitleOffset(0.6 + 0.8/aspect_ratio);
  hist_outliers_Vpeak->GetYaxis()->SetTitleOffset((0.6 + 0.8/aspect_ratio)/1.5);
  hist_outliers_Vpeak->GetYaxis()->SetTitleSize(1.25*hist_outliers_Vpeak->GetYaxis()->GetTitleSize());
  hist_outliers_Vpeak->GetYaxis()->SetLabelSize(1.25*hist_outliers_Vpeak->GetYaxis()->GetLabelSize());
  hist_outliers_Vpeak->SetLineColor(plot_colors[0]);
  hist_outliers_Vpeak->SetLineWidth(2);
  hist_outliers_Vpeak->SetFillColorAlpha(plot_colors[0],.1);
  hist_outliers_Vpeak->SetMarkerColor(plot_colors[0]);
  hist_outliers_Vpeak->SetMarkerStyle(20);
  hist_outliers_Vpeak->SetMarkerSize(1.0 + 1.0/aspect_ratio);
  hist_outliers_Vpeak->SetBarWidth(0.4);
  hist_outliers_Vpeak->SetBarOffset(0.1);
  hist_outliers_Vpeak->Draw("hist b p0 e x0");
  
  hist_outliers_Vbreakdown->SetLineColor(plot_colors[1]);
  hist_outliers_Vbreakdown->SetLineWidth(2);
  hist_outliers_Vbreakdown->SetFillColorAlpha(plot_colors[1],.1);
  hist_outliers_Vbreakdown->SetMarkerColor(plot_colors[1]);
  hist_outliers_Vbreakdown->SetMarkerStyle(21);
  hist_outliers_Vbreakdown->SetMarkerSize(1.0 + 1.0/aspect_ratio);
  hist_outliers_Vbreakdown->SetBarWidth(0.4);
  hist_outliers_Vbreakdown->SetBarOffset(0.5);
  
  cpads[0][1]->cd();
  if (n_trays > lim_trays) hist_outliers_syst_Vpeak->GetXaxis()->SetTitleOffset(1.40);
  hist_outliers_syst_Vpeak->GetYaxis()->SetRangeUser(plotlim_outliers[0], plotlim_outliers[1]);
  hist_outliers_syst_Vpeak->GetYaxis()->SetTitleOffset((0.6 + 0.8/aspect_ratio)/1.5);
  hist_outliers_syst_Vpeak->GetXaxis()->SetTitleSize(1.5*hist_outliers_syst_Vpeak->GetXaxis()->GetTitleSize());
  hist_outliers_syst_Vpeak->GetXaxis()->SetLabelSize(1.5*hist_outliers_syst_Vpeak->GetXaxis()->GetLabelSize());
  hist_outliers_syst_Vpeak->GetYaxis()->SetTitleSize(1.25*hist_outliers_syst_Vpeak->GetYaxis()->GetTitleSize());
  hist_outliers_syst_Vpeak->GetYaxis()->SetLabelSize(1.25*hist_outliers_syst_Vpeak->GetYaxis()->GetLabelSize());
  hist_outliers_syst_Vpeak->SetLineColor(plot_colors_alt[0]);
  hist_outliers_syst_Vpeak->SetLineWidth(2);
  hist_outliers_syst_Vpeak->SetFillColorAlpha(plot_colors_alt[0],.1);
  hist_outliers_syst_Vpeak->SetMarkerColor(plot_colors_alt[0]);
  hist_outliers_syst_Vpeak->SetMarkerStyle(53);
  hist_outliers_syst_Vpeak->SetMarkerSize(1.0 + 1.0/aspect_ratio);
  hist_outliers_syst_Vpeak->SetBarWidth(0.4);
  hist_outliers_syst_Vpeak->SetBarOffset(0.1);
  hist_outliers_syst_Vpeak->Draw("hist b p0 e x0");
  
  hist_outliers_syst_Vbreakdown->SetLineColor(plot_colors_alt[1]);
  hist_outliers_syst_Vbreakdown->SetLineWidth(2);
  hist_outliers_syst_Vbreakdown->SetFillColorAlpha(plot_colors_alt[1],.1);
  hist_outliers_syst_Vbreakdown->SetMarkerColor(plot_colors_alt[1]);
  hist_outliers_syst_Vbreakdown->SetMarkerStyle(54);
  hist_outliers_syst_Vbreakdown->SetMarkerSize(1.0 + 1.0/aspect_ratio);
  hist_outliers_syst_Vbreakdown->SetBarWidth(0.4);
  hist_outliers_syst_Vbreakdown->SetBarOffset(0.5);
  
  // Draw reference averaged +/- 50 MV lines, average over all trays
  double avg_voltages[2];
  avg_voltages[0] = getAvgVpeakAllTrays(flag_run_at_25_celcius); //IV
  avg_voltages[1] = getAvgVbreakdownAllTrays(flag_run_at_25_celcius); //SPS
  
  // TObjects for drawing
  TLine* margin_line = new TLine();
  TLine* batch_avg_line_IV = new TLine();
  TLine* batch_avg_line_SPS = new TLine();
  TLine* batch_avg_line_IV_corr = new TLine();
  TLine* batch_avg_line_SPS_corr = new TLine();
  
  // Line formatting
  margin_line->SetLineColor(kBlack);
  batch_avg_line_IV->SetLineColor(plot_colors[0]);
  batch_avg_line_SPS->SetLineColor(plot_colors[1]);
  batch_avg_line_IV->SetLineStyle(7);
  batch_avg_line_SPS->SetLineStyle(3);
  batch_avg_line_IV_corr->SetLineColor(plot_colors_alt[0]);
  batch_avg_line_SPS_corr->SetLineColor(plot_colors_alt[1]);
  batch_avg_line_IV_corr->SetLineStyle(7);
  batch_avg_line_SPS_corr->SetLineStyle(3);
  
  // Allowed Margin/threshold for acceptance
  cpads[0][0]->cd();
  margin_line->DrawLine(0, contract_outlier_margin_percent, n_trays, contract_outlier_margin_percent);
  cpads[0][1]->cd();
  margin_line->DrawLine(0, contract_outlier_margin_percent, n_trays, contract_outlier_margin_percent);
  
  // SiPM batch delimeter lines, dynamic to the input data
  TLine* batch_line = new TLine();
  batch_line->SetLineColor(kGray+1);
  batch_line->SetLineStyle(6);
  std::string last_batch, next_batch;
  std::stringstream traystream;
  float start_of_batch = 0.5;
  for (int i_tray = 0; i_tray <= n_trays; ++i_tray) {
     if (i_tray == n_trays) goto lastbatch;  // catch last batch and draw text, skipping check for strings
    
    // Check if next string denotes a new batch
    traystream = std::stringstream(gReader->GetTrayStrings()->at(tray_reshuffle_index[i_tray]));
    if (i_tray == 0) {getline(traystream, last_batch, '-'); continue;}
    getline(traystream, next_batch, '-');
    
  lastbatch:
    if (last_batch.compare(next_batch) != 0 || i_tray >= n_trays - 1) {
      
      // compute batch average
      double avg_IV_this_batch = (static_cast<double>(countOutliersVpeakBatch(last_batch, flag_run_at_25_celcius, 0))
                                  / countValidSiPMsBatch(last_batch) )*100;
      double avg_PS_this_batch = (static_cast<double>(countOutliersVbreakdownBatch(last_batch, flag_run_at_25_celcius, 0))
                                  / countValidSiPMsBatch(last_batch) )*100;
      double avg_IV_corr_batch = (static_cast<double>(countOutliersVpeakBatch(last_batch, flag_run_at_25_celcius, 
                                                                              syst_error_results[flag_run_at_25_celcius][0]))
                                  / countValidSiPMsBatch(last_batch) )*100;
      double avg_PS_corr_batch = (static_cast<double>(countOutliersVbreakdownBatch(last_batch, flag_run_at_25_celcius, 
                                                                                   syst_error_results[flag_run_at_25_celcius][1]))
                                  / countValidSiPMsBatch(last_batch) )*100;
      
      // draw new batch delimiters/average lines
      cpads[0][1]->cd();
      if (last_batch.compare(next_batch) != 0) batch_line->DrawLine(i_tray, plotlim_outliers[0], i_tray, plotlim_outliers[1]);
      batch_avg_line_IV_corr->DrawLine(std::floor(start_of_batch), avg_IV_corr_batch, i_tray, avg_IV_corr_batch);
      batch_avg_line_SPS_corr->DrawLine(std::floor(start_of_batch), avg_PS_corr_batch, i_tray, avg_PS_corr_batch);
      
      cpads[0][0]->cd();
      if (last_batch.compare(next_batch) != 0) batch_line->DrawLine(i_tray, plotlim_outliers[0], i_tray, plotlim_outliers[1]);
      batch_avg_line_IV->DrawLine(std::floor(start_of_batch), avg_IV_this_batch, i_tray, avg_IV_this_batch);
      batch_avg_line_SPS->DrawLine(std::floor(start_of_batch), avg_PS_this_batch, i_tray, avg_PS_this_batch);
      
      // Label the batch on the plot
      if (i_tray - start_of_batch != 1) {
        double batch_text_x = gPad->GetLeftMargin() + plot_window_size_x * static_cast<float>(start_of_batch)/n_trays + 0.01;
        drawText(Form("Batch %s", last_batch.c_str()), batch_text_x, 0.81, false, kBlack, 0.0375);
        if (last_batch.compare("250717") == 0) drawText("(ORNL)", batch_text_x, 0.77, false, kBlack, 0.0375);
      }
      
      // continue to next batch
      last_batch = next_batch;
      start_of_batch = i_tray;
    }// End of check for new batch
  }// End of batch delimiter lines
  
  // Finish first panel -- raw outliers against margin 50mV
  // assure points sit on top of lines
  cpads[0][0]->cd();
  hist_outliers_Vpeak->Draw("b p e x0 same");
  hist_outliers_Vbreakdown->Draw("hist b p0 e x0 same");
  
  // Legend for labeling the two V_breakdown measurement types
  cpads[0][1]->cd();
  double first_x_margin = gPad->GetLeftMargin() + plot_window_size_x * 5.0/n_trays;
  TLegend* vbd_legend = new TLegend(0.125, 0.685, first_x_margin - 0.06, 0.95);
  vbd_legend->SetLineWidth(0);
  vbd_legend->AddEntry(hist_outliers_Vpeak, "IV V_{bd} Uncorrected", "p");
  vbd_legend->AddEntry(hist_outliers_Vbreakdown, "SPS V_{bd} Uncorrected", "p");
  vbd_legend->AddEntry(hist_outliers_syst_Vpeak, "IV V_{bd} Corrected", "p");
  vbd_legend->AddEntry(hist_outliers_syst_Vbreakdown, "SPS V_{bd} Corrected", "p");
  vbd_legend->Draw();
  
  
  // Legend for the lines marking tray average, test sets
  TLegend* line_legend = new TLegend(first_x_margin + 0.02, 0.56, first_x_margin + plot_window_size_x * 5.0/n_trays - 0.02, 0.95);
  line_legend->SetLineWidth(0);
  double avg_IV_all = (static_cast<double>(countOutliersVpeakBatch("-", flag_run_at_25_celcius, 0))
                       / countSiPMsAllTrays() )*100;
  double avg_PS_all = (static_cast<double>(countOutliersVbreakdownBatch("-", flag_run_at_25_celcius, 0))
                       / countSiPMsAllTrays() )*100;
  double avg_IV_corr = (static_cast<double>(countOutliersVpeakBatch("-", flag_run_at_25_celcius,
                                                                    syst_error_results[flag_run_at_25_celcius][0]))
                        / countSiPMsAllTrays() )*100;
  double avg_PS_corr = (static_cast<double>(countOutliersVbreakdownBatch("-", flag_run_at_25_celcius,
                                                                         syst_error_results[flag_run_at_25_celcius][1]))
                       / countSiPMsAllTrays() )*100;
  line_legend->AddEntry(margin_line, Form("Contract Margin (%.1f%%)",contract_outlier_margin_percent), "l");
  line_legend->AddEntry(batch_avg_line_IV, Form("IV Outliers (Total: #color[2]{%.2f}%%)",avg_IV_all), "l");
  line_legend->AddEntry(batch_avg_line_SPS, Form("SPS Outliers (Total: #color[2]{%.2f}%%)",avg_PS_all), "l");
  line_legend->AddEntry(batch_avg_line_IV_corr, Form("IV Corrected (Total: #color[2]{%.2f}%%)",avg_IV_corr), "l");
  line_legend->AddEntry(batch_avg_line_SPS_corr, Form("SPS Corrected (Total: #color[2]{%.2f}%%)",avg_PS_corr), "l");
  line_legend->AddEntry(batch_line, "Batch Delimeter", "l");
  line_legend->Draw();
  
  // Second panel -- outliers including extra tolerance for systematic errors
  cpads[0][1]->cd();
//  hist_outliers_syst_Vpeak->Draw("b p e x0 same");
  hist_outliers_syst_Vbreakdown->Draw("hist b p0 e x0 same");
  
  
  // Draw some text giving info on the setup
  cpads[0][0]->cd();
  double right_text_margin = gPad->GetRightMargin() - (n_trays <= lim_trays)*0.01;
  double left_text_margin = gPad->GetLeftMargin() - (n_trays <= lim_trays)*0.05;
  drawText("#bf{Debrecen} SiPM Test Setup @ #bf{Yale}",           left_text_margin, 0.91, false, kBlack, 0.04);
  drawText("#bf{ePIC} Test Stand",                                left_text_margin, 0.955, false, kBlack, 0.045);
  drawText(Form("Hamamatsu #bf{%s}", Hamamatsu_SiPM_Code),        1.0-right_text_margin, 0.95, true, kBlack, 0.045);
  drawText(Form("%s", string_tempcorr[flag_run_at_25_celcius]),   1.0-right_text_margin, 0.905, true, kBlack, 0.035);
  
  // Save to file
  char string_quadsum_short[2][10] = {"_dirsum","_quadsum"};
  gCanvas_double->SaveAs(Form("../plots/batch_plots/batch_Vbr_outliers%s%s.pdf",
                              string_tempcorr_short[flag_run_at_25_celcius],
                              string_quadsum_short[use_quadrature_sum_for_syst_error]));
  
  delete hist_outliers_Vpeak;
  delete hist_outliers_Vbreakdown;
  delete hist_outliers_syst_Vpeak;
  delete hist_outliers_syst_Vbreakdown;
}// End of sipm_batch_summary_sheet::makeIndexedOutliers

//========================================================================== Solo plot generating Macros: SiPM Tray/Test Mappings



// Map the IV V_peak results to the tray poisitons in a 2D grid
// Helpful for checking if strange trends are manufacturing flaws or
// systematic/statistical errors in the testing procedure
void makeTrayMapVpeak(bool flag_run_at_25_celcius = true) {
  
  gStyle->SetPalette(kSunset);
  gCanvas_solo->Clear();
  gCanvas_solo->SetCanvasSize(750, 600);
  gCanvas_solo->cd();
  gPad->SetTicks(1,1);
  gPad->SetLogy(0);
  gPad->SetRightMargin(0.17);
  gPad->SetLeftMargin(0.085);
  gPad->SetTopMargin(0.11);
  gPad->SetBottomMargin(0.095);
  
  // Iterate over all available data
  for (int i_tray = 0; i_tray < gReader->GetTrayStrings()->size(); ++i_tray) {
    
    // Get averages for z axis range
    double avg_voltage = getAvgVpeak(i_tray, flag_run_at_25_celcius);
    
    // Make map histogram
    int IV_size = gReader->GetIV()->at(i_tray)->IV_Vpeak->size();
    TH2F* map_tray_Vpeak = new TH2F("map_tray_Vpeak",
                                    ";SiPM Tray Column;SiPM Tray Row;Deviation from Tray Avg. #color[2]{#bf{IV}} V_{br} [V]",
                                    NCOL, 0, NCOL, NROW, 0, NROW);
    for (int i_IV = 0; i_IV < IV_size; ++i_IV) {
      if (flag_run_at_25_celcius) {
        map_tray_Vpeak->SetBinContent(1 + gReader->GetIV()->at(i_tray)->col->at(i_IV),
                                      1 + gReader->GetIV()->at(i_tray)->row->at(i_IV),
                                      gReader->GetIV()->at(i_tray)->IV_Vpeak_25C->at(i_IV) - avg_voltage);
      } else {
        map_tray_Vpeak->SetBinContent(1 + gReader->GetIV()->at(i_tray)->col->at(i_IV),
                                      1 + gReader->GetIV()->at(i_tray)->row->at(i_IV),
                                      gReader->GetIV()->at(i_tray)->IV_Vpeak->at(i_IV) - avg_voltage);
      }
    }
    
    // Plot the map
    map_tray_Vpeak->GetZaxis()->SetRangeUser(-0.16, 0.16);
    map_tray_Vpeak->GetZaxis()->SetTitleOffset(1.7);
    map_tray_Vpeak->GetYaxis()->SetTitleOffset(0.86);
    map_tray_Vpeak->Draw("colz");
    
    // Draw some text giving info on the setup
    drawText("#bf{ePIC} Test Stand", gPad->GetLeftMargin(), 0.95, false, kBlack, 0.035);
    drawText("#bf{Debrecen} SiPM Test Setup @ #bf{Yale}", gPad->GetLeftMargin(), 0.903, false, kBlack, 0.035);
    drawText(Form("Hamamatsu #bf{%s} Tray #%s", Hamamatsu_SiPM_Code, gReader->GetTrayStrings()->at(i_tray).c_str()), 0.99, 0.955, true, kBlack, 0.035);
    drawText(Form("%s", string_tempcorr[flag_run_at_25_celcius]), 0.99, 0.915, true, kBlack, 0.03);
    
    // Save the map
    gCanvas_solo->SaveAs(Form("../plots/single_plots/mapped_tray%s/%s_traymap_IV_Vbr%s.pdf",
                              string_tempcorr_short[flag_run_at_25_celcius],
                              gReader->GetTrayStrings()->at(i_tray).c_str(),
                              string_tempcorr_short[flag_run_at_25_celcius]));
    
    delete map_tray_Vpeak;
  }
  return;
}// End of sipm_batch_summary_sheet::makeTrayMapVpeak



// Map the SPS V_breakdown results to the tray poisitons in a 2D grid
// Helpful for checking if strange trends are manufacturing flaws or
// systematic/statistical errors in the testing procedure
void makeTrayMapVbreakdown(bool flag_run_at_25_celcius = true) {
  
  gStyle->SetPalette(kSunset);
  gCanvas_solo->Clear();
  gCanvas_solo->SetCanvasSize(750, 600);
  gCanvas_solo->cd();
  gPad->SetTicks(1,1);
  gPad->SetLogy(0);
  gPad->SetRightMargin(0.17);
  gPad->SetLeftMargin(0.085);
  gPad->SetTopMargin(0.11);
  gPad->SetBottomMargin(0.095);
  
  // Iterate over all available data
  for (int i_tray = 0; i_tray < gReader->GetTrayStrings()->size(); ++i_tray) {
    
    // Get averages for z axis range
    double avg_voltage = getAvgVbreakdown(i_tray, flag_run_at_25_celcius);
    
    // Make map histogram
    int SPS_size = gReader->GetSPS()->at(i_tray)->SPS_Vbd->size();
    TH2F* map_tray_Vbreakdown = new TH2F("map_tray_Vbreakdown",
                                         ";SiPM Tray Column;SiPM Tray Row;Deviation from Tray Avg. #color[2]{#bf{SPS}} V_{br} [V]",
                                         NCOL, 0, NCOL, NROW, 0, NROW);
    for (int i_SPS = 0; i_SPS < SPS_size; ++i_SPS) {
      if (flag_run_at_25_celcius) {
        map_tray_Vbreakdown->SetBinContent(1 + gReader->GetSPS()->at(i_tray)->col->at(i_SPS),
                                           1 + gReader->GetSPS()->at(i_tray)->row->at(i_SPS),
                                           gReader->GetSPS()->at(i_tray)->SPS_Vbd_25C->at(i_SPS) - avg_voltage);
      } else {
        map_tray_Vbreakdown->SetBinContent(1 + gReader->GetSPS()->at(i_tray)->col->at(i_SPS),
                                           1 + gReader->GetSPS()->at(i_tray)->row->at(i_SPS),
                                           gReader->GetSPS()->at(i_tray)->SPS_Vbd->at(i_SPS) - avg_voltage);
      }
    }
    
    // Plot the map
    map_tray_Vbreakdown->GetZaxis()->SetRangeUser(-0.16, 0.16);
    map_tray_Vbreakdown->GetZaxis()->SetTitleOffset(1.7);
    map_tray_Vbreakdown->GetYaxis()->SetTitleOffset(0.86);
    map_tray_Vbreakdown->Draw("colz");
    
    // Draw some text giving info on the setup
    drawText("#bf{ePIC} Test Stand", gPad->GetLeftMargin(), 0.95, false, kBlack, 0.035);
    drawText("#bf{Debrecen} SiPM Test Setup @ #bf{Yale}", gPad->GetLeftMargin(), 0.903, false, kBlack, 0.035);
    drawText(Form("Hamamatsu #bf{%s} Tray #%s", Hamamatsu_SiPM_Code, gReader->GetTrayStrings()->at(i_tray).c_str()), 0.99, 0.955, true, kBlack, 0.035);
    drawText(Form("%s", string_tempcorr[flag_run_at_25_celcius]), 0.99, 0.915, true, kBlack, 0.03);
    
    // Save the map
    gCanvas_solo->SaveAs(Form("../plots/single_plots/mapped_tray%s/%s_traymap_SPS_Vbr%s.pdf",
                              string_tempcorr_short[flag_run_at_25_celcius],
                              gReader->GetTrayStrings()->at(i_tray).c_str(),
                              string_tempcorr_short[flag_run_at_25_celcius]));
    
    delete map_tray_Vbreakdown;
  }
  return;
  
}// End of sipm_batch_summary_sheet::makeTrayMapVbreakdown



// Map the IV V_peak results to the cassette test poisitons in a 2D grid
// Helpful for checking if strange trends are manufacturing flaws or
// systematic/statistical errors in the testing procedure
void makeTestMapVpeak(bool flag_run_at_25_celcius = true) {
  
  gStyle->SetPalette(kSunset);
  gCanvas_solo->Clear();
  gCanvas_solo->SetCanvasSize(1200, 600);
  gCanvas_solo->cd();
  gPad->SetTicks(1,1);
  gPad->SetLogy(0);
  gPad->SetRightMargin(0.13);
  gPad->SetLeftMargin(0.05);
  gPad->SetBottomMargin(0.08);
  gPad->SetTopMargin(0.11);
  
  // Iterate over all available data
  for (int i_tray = 0; i_tray < gReader->GetTrayStrings()->size(); ++i_tray) {
    
    // Get averages for z axis range
    double avg_voltage = getAvgVpeak(i_tray, flag_run_at_25_celcius);
    
    // Make map histogram
    int IV_size = gReader->GetIV()->at(i_tray)->IV_Vpeak->size();
    TH2F* map_test_Vpeak = new TH2F("map_test_Vpeak",
                                    ";Cassette Index;IV Test Set;Deviation from Tray Avg. #color[2]{#bf{IV}} V_{br} [V]",
                                    32, 0, 32, 15, 0, 15);
    for (int i_IV = 0; i_IV < IV_size; ++i_IV) {
      if (flag_run_at_25_celcius) {
        map_test_Vpeak->Fill(i_IV % 32, i_IV / 32,
                             gReader->GetIV()->at(i_tray)->IV_Vpeak_25C->at(i_IV) - avg_voltage);
      } else {
        map_test_Vpeak->Fill(i_IV % 32, i_IV / 32,
                             gReader->GetIV()->at(i_tray)->IV_Vpeak->at(i_IV) - avg_voltage);
      }
    }for (int i_fill = IV_size; i_fill < 32*15; ++i_fill)
      map_test_Vpeak->SetBinContent(i_fill % 32 + 1, i_fill / 32 + 1, -1);
    
    // Plot the map
    map_test_Vpeak->GetZaxis()->SetRangeUser(-0.16, 0.16);
    map_test_Vpeak->GetZaxis()->SetTitleOffset(1.1);
    map_test_Vpeak->GetYaxis()->SetTitleOffset(0.6);
    map_test_Vpeak->Draw("colz");
    
    // Draw some text giving info on the setup
    drawText("#bf{ePIC} Test Stand", gPad->GetLeftMargin(), 0.95, false, kBlack, 0.045);
    drawText("#bf{Debrecen} SiPM Test Setup @ #bf{Yale}", gPad->GetLeftMargin(), 0.903, false, kBlack, 0.045);
    drawText(Form("Hamamatsu #bf{%s} Tray #%s", Hamamatsu_SiPM_Code, gReader->GetTrayStrings()->at(i_tray).c_str()), 1-gPad->GetRightMargin(), 0.955, true, kBlack, 0.045);
    drawText(Form("%s", string_tempcorr[flag_run_at_25_celcius]), 1-gPad->GetRightMargin(), 0.907, true, kBlack, 0.04);
    
    // Save the map
    gCanvas_solo->SaveAs(Form("../plots/single_plots/mapped_test%s/%s_testmap_IV_Vbr%s.pdf",
                              string_tempcorr_short[flag_run_at_25_celcius],
                              gReader->GetTrayStrings()->at(i_tray).c_str(),
                              string_tempcorr_short[flag_run_at_25_celcius]));
    
    delete map_test_Vpeak;
  }
  return;
}// End of sipm_batch_summary_sheet::makeTestMapVpeak



// Map the SPS V_breakdown results to the tray poisitons in a 2D grid
// Helpful for checking if strange trends are manufacturing flaws or
// systematic/statistical errors in the testing procedure
void makeTestMapVbreakdown(bool flag_run_at_25_celcius = true) {
  
  gStyle->SetPalette(kSunset);
  gCanvas_solo->Clear();
  gCanvas_solo->SetCanvasSize(1200, 600);
  gCanvas_solo->cd();
  gPad->SetTicks(1,1);
  gPad->SetLogy(0);
  gPad->SetRightMargin(0.13);
  gPad->SetLeftMargin(0.05);
  gPad->SetBottomMargin(0.08);
  gPad->SetTopMargin(0.11);
  
  // Iterate over all available data
  for (int i_tray = 0; i_tray < gReader->GetTrayStrings()->size(); ++i_tray) {
    
    // Get averages for z axis range
    double avg_voltage = getAvgVbreakdown(i_tray, flag_run_at_25_celcius);
    
    // Make map histogram
    int SPS_size = gReader->GetSPS()->at(i_tray)->SPS_Vbd->size();
    TH2F* map_test_Vbreakdown = new TH2F("map_test_Vbreakdown",
                                         ";Cassette Index;SPS Test Set;Deviation from Tray Avg. #color[2]{#bf{SPS}} V_{br} [V]",
                                         32, 0, 32, 15, 0, 15);
    for (int i_SPS = 0; i_SPS < SPS_size; ++i_SPS) {
      if (flag_run_at_25_celcius) {
        map_test_Vbreakdown->Fill(i_SPS % 32, i_SPS / 32,
                                  gReader->GetSPS()->at(i_tray)->SPS_Vbd_25C->at(i_SPS) - avg_voltage);
      } else {
        map_test_Vbreakdown->Fill(i_SPS % 32, i_SPS / 32,
                                  gReader->GetSPS()->at(i_tray)->SPS_Vbd->at(i_SPS) - avg_voltage);
      }
    } for (int i_fill = SPS_size; i_fill < 32*15; ++i_fill)
      map_test_Vbreakdown->SetBinContent(i_fill % 32 + 1, i_fill / 32 + 1, -1);
    
    // Plot the map
    map_test_Vbreakdown->GetZaxis()->SetRangeUser(-0.16, 0.16);
    map_test_Vbreakdown->GetZaxis()->SetTitleOffset(1.1);
    map_test_Vbreakdown->GetYaxis()->SetTitleOffset(0.6);
    map_test_Vbreakdown->Draw("colz");
    
    // Draw some text giving info on the setup
    drawText("#bf{ePIC} Test Stand", gPad->GetLeftMargin(), 0.95, false, kBlack, 0.045);
    drawText("#bf{Debrecen} SiPM Test Setup @ #bf{Yale}", gPad->GetLeftMargin(), 0.903, false, kBlack, 0.045);
    drawText(Form("Hamamatsu #bf{%s} Tray #%s", Hamamatsu_SiPM_Code, gReader->GetTrayStrings()->at(i_tray).c_str()), 1-gPad->GetRightMargin(), 0.955, true, kBlack, 0.045);
    drawText(Form("%s", string_tempcorr[flag_run_at_25_celcius]), 1-gPad->GetRightMargin(), 0.907, true, kBlack, 0.04);
    
    // Save the map
    gCanvas_solo->SaveAs(Form("../plots/single_plots/mapped_test%s/%s_testmap_SPS_Vbr%s.pdf",
                              string_tempcorr_short[flag_run_at_25_celcius],
                              gReader->GetTrayStrings()->at(i_tray).c_str(),
                              string_tempcorr_short[flag_run_at_25_celcius]));
    
    delete map_test_Vbreakdown;
  }
  return;
  
}// End of sipm_batch_summary_sheet::makeTestMapVbreakdown

