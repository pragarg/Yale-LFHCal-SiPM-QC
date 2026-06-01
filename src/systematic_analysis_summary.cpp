//  *--
//  systematic_analysis_summary.cpp
//
//  Produces some plots that aim to analyze
//  systematic dependences of the test stand
//  setup by Debrecen at Yale.
//
//  Created by Ryan Hamilton on 11/1/25.
//  *--

#include "global_vars.hpp"
#include "SiPMDataReader.hpp"
#include "sipm_analysis_helper.hpp"

//========================================================================== Global Variables

// flags to control some options in analysis
bool global_flag_run_at_25_celcius = false;
// TODO make class variable, I was silly and didn't think I would use this as much as I do...
bool global_flag_adjust_IV_tempcorr = false;
bool global_flag_find_cycle_temp_gradient = true;
bool reproducibility_skip_SPS = false;


// Some helpful label strings
const char string_tempcorr[2][50] = {"#color[2]{#bf{NOT}} Temperature corrected to 25C","Temperature corrected to 25C"};
const char string_tempcorr_short[2][10] = {"","_25C"};


// Global plot objects
TCanvas* gCanvas_solo;

TCanvas* gCanvas_cassetteplot;
TPad* cassette_pad;
std::vector<std::vector<TPad*> > cassette_pads;

TCanvas* gCanvas_surfacecorr;
TPad* surfacecorr_pad;
std::vector<std::vector<TPad*> > surfacecorr_pads;



// Global data collectors
const int nbins_residualhist = 21;
const int nbins_stdevhist = 10;
TH1D* gHist_rep_residual[2];        // Residuals for reproducibility comparisons among SiPMs
TH1D* gHist_rep_stdev[2];           // Stdev of SiPM repeated test distributions in a histogram
TH1D* gData_cycletest_sipm_pair_difference_IV;    // Differences between IV test of same SiPM from adjacent cassette locations after temperature correction to 25C
TH1D* gData_cycletest_sipm_pair_difference_SPS;   // Differences bewteen SPS test of same SiPM from adjacent cassette locations after temperature correction to 25C
double avg_sipm_pair_difference[2] = {0,0};
int count_sipm_pair_differences = 0;

// Error estimators
const double error_confidence = 0.9; // TODO frequentist confidence interval
double gRepError_IV[2] = {0,0};     // Mean error from IV reproducibility, useful for other systematics/plots
double gRepError_SPS[2] = {0,0};    // Mean error from SPS reproducibility, useful for other measurements
double gTempcorr_IV = 0.0369;        // Hamamatsu spec temperature correction: 34 mV / Kelvin. From our fits, maybe more like 36.5 mV/K or so

// Plot limit controls
double voltplot_limits[2] = {37.6, 38.6};
double volthist_range[2] = {-0.06, 0.06};
double darkcurr_limits[2] = {0, 35};



// TODO make plotter class??? Or at least consider it...

//========================================================================== Forward declarations

//Reproducability
void initializeGlobalReproducabilityHists();
void makeReproducabilityHist(std::string base_tray_id, bool drawplot = true);
void drawGlobalReproducabilityHists(std::string modifier = "batch");

// Temperature
void makeTemperatureScan();
void makeTemperatureGradientHist();

// Cycle/Cassette Location/Reshuffle
void makeCycleScan();

// Operating Voltage V_op
void makeOperatingVoltageScan();

// Surface Imperfections
void makeSurfaceImperfectionCorrelation();

//========================================================================== Macro Main

// Main macro method: generate SiPM data
void systematic_analysis_summary() {
  
  // *-- Analysis setup
  
  // Initialize SiPMDataReader
  SiPMDataReader* reader = new SiPMDataReader();
  
  // Initialize canvases
  gStyle->SetOptStat(0);
  gStyle->SetPalette(TH2_palette);
  gErrorIgnoreLevel = kWarning;
  
  gCanvas_solo = new TCanvas();
  gCanvas_surfacecorr = new TCanvas();
  
  // *-- Analysis tasks: Reproducibility
  
  // Read IV and SPS data for reproducibility tests
  reader->SetSubDirectory("repsyst");
  reader->ReadFile("../data/syst_traylist_repsyst.txt");
  reader->ReadDataIV();
  reader->ReadDataSPS();
  
  // Run for both temperature correction states
  for (int i_tempcorr = 0; i_tempcorr < 2; ++i_tempcorr) {
    // Initialize padded canvas
    gCanvas_cassetteplot = new TCanvas();
    gCanvas_cassetteplot->SetCanvasSize(1500,830);
    cassette_pad = buildPad("cassette_pad", 0, 0, 1, 0.75/0.83);
    cassette_pad->cd();
    cassette_pads = divideFlush(gPad, 8, 4, 0.025, 0.005, 0.05, 0.01);
    
    global_flag_run_at_25_celcius = i_tempcorr;
    
    // Initialize global hists
    initializeGlobalReproducabilityHists();
    
    // Make plots from reproducibility tests
    makeReproducabilityHist("250821-1302");
    makeReproducabilityHist("250821-1303");
    
    // Make composite plots with data from all repeated tests
    drawGlobalReproducabilityHists();
    
    cassette_pads.clear();
  }
  
  
  // *-- Analysis tasks: Operating Voltage
  
  // Read IV and SPS data for vop scan
  reader->SetFlatTrayString(); // Don't require parent directories end in "-results"
  reader->SetSubDirectory("vopscan");
  reader->ReadFile("../data/syst_traylist_vopscan.txt");
  reader->ReadDataIV();
  reader->ReadDataSPS();
  
  makeOperatingVoltageScan();
  
  // *-- Analysis tasks: Temperature
  reader->SetFlatTrayString(); // Don't require parent directories end in "-results"
  reader->SetSubDirectory("tempscan");
  reader->ReadFile("../data/syst_traylist_tempscan.txt");
  reader->ReadDataIV();
  reader->ReadDataSPS();
  
  makeTemperatureScan();
  
  // *-- Analysis tasks: Cycle scan
  reader->SetFlatTrayString(); // Don't require parent directories end in "-results"
  reader->SetSubDirectory("cyclescan");
  reader->ReadFile("../data/syst_traylist_cyclescan.txt");
  reader->ReadDataIV();
  reader->ReadDataSPS();
  
  makeCycleScan();
  
  
  
  // Make a temperature difference hist with all available data
  // (to search for a potential temperature gradient in the test box)
  makeTemperatureGradientHist();
  
  // *-- Analysis tasks: Surface Imperfection study
  // Do SiPMs with obstructed surfaces behave more poorly?
  global_flag_run_at_25_celcius = true;
  reader->SetDefTrayString(); // return to requiring "-results" for normal data
  reader->SetSubDirectory("production");
  reader->ReadFile("../data/batch_traylist_production.txt"); // read in only real test results
  reader->ReadDataIV();
  reader->ReadDataSPS();
  makeSurfaceImperfectionCorrelation();
  
  // Check IV reproducibility for "wave-like" correlations
//  reproducibility_skip_SPS = true;
//  initializeGlobalReproducabilityHists();
//  gCanvas_cassetteplot = new TCanvas();
//  gCanvas_cassetteplot->SetCanvasSize(1500,830);
//  cassette_pad = buildPad("cassette_pad", 0, 0, 1, 0.75/0.83);
//  cassette_pad->cd();
//  cassette_pads = divideFlush(gPad, 8, 4, 0.025, 0.005, 0.05, 0.01);
//
//  reader->ReadFile("../batch_data_wavecheck.txt");
//  reader->ReadDataIV();
//  reader->ReadDataSPS();
  makeReproducabilityHist("250911-1607");
  // TODO Fix needing both IV and SPS always
  // TODO Make it so that comments to pass in an empty file string to the reader
  // Currently hobbles over the finish line but needs some cleaning...
  
}// End of systematic_analysis_summary::main

//========================================================================== Reproducibility tests



// Initialize the global histograms for reproducibility tests
// These keep track of residuals and reproducibility test stdev
// throughout all trays and available data when running makeReproducabilityHist()
void initializeGlobalReproducabilityHists() {
  char testtype[2][5] = {"IV","SPS"};
  for (int i_test = 0; i_test < 2; ++i_test) {
    gHist_rep_residual[i_test] = new TH1D(Form("hist_rep_residual_%s",testtype[i_test]),
                                          ";Reproducability Residual V_{br} - V_{br}^{Rep. Avg.} [mV];Count of SiPM Tests",
                                          nbins_residualhist, volthist_range[0]*1000,volthist_range[1]*1000);
    gHist_rep_residual[i_test]->SetLineColor(plot_colors[i_test]);
    gHist_rep_residual[i_test]->SetFillColorAlpha(plot_colors[i_test], 0.25);
    gHist_rep_residual[i_test]->SetMarkerColor(plot_colors[i_test]);
    
    gHist_rep_stdev[i_test] = new TH1D(Form("hist_rep_stdev_%s",testtype[i_test]),
                                       ";Reproducability StDev #sigma [mV];Count of SiPMs",
                                       nbins_stdevhist, 0, volthist_range[1]*1000);
    gHist_rep_stdev[i_test]->SetLineColor(plot_colors[i_test]);
    gHist_rep_stdev[i_test]->SetFillColorAlpha(plot_colors[i_test], 0.25);
    gHist_rep_stdev[i_test]->SetMarkerColor(plot_colors[i_test]);
  }return;
}// End of systematic_analysis_summary::initializeReproducabilityHists



// Draw and save the global histograms for residuals/stdev of reproducibility tests
// Note that these histograms are filled by running makeReproducabilityHist(), and the
// global hist will only contain data that has been run in that method (i.e. not all from gReader).
void drawGlobalReproducabilityHists(std::string modifier) {
  
  // Helpful numbers to add to canvas
  int ntotal_sipms = static_cast<int>(gHist_rep_stdev[0]->GetEntries());
  int tests_per_sipm = static_cast<int>(gHist_rep_residual[0]->GetEntries()/gHist_rep_stdev[0]->GetEntries());
  
  // Reset the canvas
  gCanvas_solo->cd();
  gPad->SetRightMargin(0.04);
  gPad->SetLeftMargin(0.09);
  gPad->SetTicks(1,1);
  
  // Draw residual hists
  gHist_rep_residual[0]->GetXaxis()->SetTitleOffset(1.1);
  gHist_rep_residual[0]->Draw("hist");
  gHist_rep_residual[1]->Draw("hist same");
  
  // Label the plot with some descriptive text
  TLatex* top_tex[6];
  top_tex[0] = drawText("#bf{Debrecen} SiPM Test Setup @ #bf{Yale}",                  gPad->GetLeftMargin(), 0.91, false, kBlack, 0.04);
  top_tex[1] = drawText("#bf{ePIC} Test Stand",                                       gPad->GetLeftMargin(), 0.955, false, kBlack, 0.045);
  top_tex[2] = drawText(Form("Hamamatsu #bf{%s}", Hamamatsu_SiPM_Code),               1.-gPad->GetRightMargin(), 0.95, true, kBlack, 0.045);
  top_tex[3] = drawText(Form("%s", string_tempcorr[global_flag_run_at_25_celcius]),   1.-gPad->GetRightMargin(), 0.91, true, kBlack, 0.035);
  top_tex[4] = drawText(Form("%i total SiPMs",ntotal_sipms),                          0.9, 0.83, true, kBlack, 0.035);
  top_tex[5] = drawText(Form("Each tested %i times",tests_per_sipm),                  0.9, 0.78, true, kBlack, 0.035);
  
  // Legend to label which hists are IV/SPS
  TLegend* vbd_legend = new TLegend(0.14, 0.6, 0.4, 0.85);
  vbd_legend->SetLineWidth(0);
  vbd_legend->AddEntry(gHist_rep_residual[0], "V_{bd} from IV curve", "f");
  vbd_legend->AddEntry(gHist_rep_residual[1], "V_{bd} from SPS", "f");
  vbd_legend->Draw();
  
  // Save residual curve
  gCanvas_solo->SaveAs(Form("../plots/systematic_plots/reproducibility%s/%s_reproducibility_residual.pdf",
                            string_tempcorr_short[global_flag_run_at_25_celcius],
                            modifier.c_str()));
  
  
  // Draw stdev hists
  double max_stdev_plotrange = gHist_rep_stdev[0]->GetBinContent(1) * 1.1;
  gHist_rep_stdev[0]->GetXaxis()->SetTitleOffset(1.1);
//  gHist_rep_stdev[0]->GetYaxis()->SetRangeUser(0, max_stdev_plotrange);
  gHist_rep_stdev[0]->Draw("hist");
  gHist_rep_stdev[1]->Draw("hist same");
  
  // Redraw the descriptive text
  for (int iTex = 0; iTex < 6; ++iTex) top_tex[iTex]->Draw();
  
  // Add lines to mark the average error
  gRepError_IV[global_flag_run_at_25_celcius] /= static_cast<double>(ntotal_sipms);
  gRepError_SPS[global_flag_run_at_25_celcius] /= static_cast<double>(ntotal_sipms);
  std::cout << "SPS error = " << gRepError_SPS[global_flag_run_at_25_celcius] << std::endl;
  
  TLine* avgerr_line_IV = new TLine();
  avgerr_line_IV->SetLineStyle(7);
  avgerr_line_IV->SetLineWidth(2);
  avgerr_line_IV->SetLineColor(plot_colors_alt[0]);
  avgerr_line_IV->DrawLine(1000*gRepError_IV[global_flag_run_at_25_celcius], 0,
                        1000*gRepError_IV[global_flag_run_at_25_celcius],
                        gHist_rep_stdev[0]->GetBinContent(gHist_rep_stdev[0]->FindBin(1000*gRepError_IV[global_flag_run_at_25_celcius])));
  
  TLine* avgerr_line_SPS = new TLine();
  avgerr_line_SPS->SetLineStyle(7);
  avgerr_line_SPS->SetLineWidth(2);
  avgerr_line_SPS->SetLineColor(plot_colors_alt[1]);
  avgerr_line_SPS->DrawLine(1000*gRepError_SPS[global_flag_run_at_25_celcius], 0,
                        1000*gRepError_SPS[global_flag_run_at_25_celcius],
                        gHist_rep_stdev[1]->GetBinContent(gHist_rep_stdev[1]->FindBin(1000*gRepError_SPS[global_flag_run_at_25_celcius])));
  
  // Draw legend + average line
  vbd_legend = new TLegend(0.58, 0.39, 0.9, 0.73);
  vbd_legend->SetLineWidth(0);
  vbd_legend->AddEntry(gHist_rep_residual[0], "V_{bd} from IV curve", "f");
  vbd_legend->AddEntry(gHist_rep_residual[1], "V_{bd} from SPS", "f");
  vbd_legend->AddEntry(avgerr_line_IV, Form("Average #sigma_{Vbd}^{IV} (#color[2]{%.3f} mV)",
                                            1000*gRepError_IV[global_flag_run_at_25_celcius]), "l");
  vbd_legend->AddEntry(avgerr_line_SPS, Form("Average #sigma_{Vbd}^{SPS} (#color[2]{%.3f} mV)",
                                             1000*gRepError_SPS[global_flag_run_at_25_celcius]), "l");
  vbd_legend->Draw();
  
  gCanvas_solo->SaveAs(Form("../plots/systematic_plots/reproducibility%s/batch_reproducibility_stdev.pdf",
                       string_tempcorr_short[global_flag_run_at_25_celcius]));
  
  // Clean hists for another run
  delete gHist_rep_residual[0];
  delete gHist_rep_residual[1];
  delete gHist_rep_stdev[0];
  delete gHist_rep_stdev[1];
}// End of systematic_analysis_summary::drawGlobalReproducabilityHist



// Compose histograms of SiPMs from repeated tests where data is available
// This method will gather the data, filling the global histograms defined above
// and produce an output composite plot with 32 histograms (one for each cassette
// slot) for any test sets which are repeated in the available data from gReader.
void makeReproducabilityHist(std::string base_tray_id, bool drawplot) {
  
  // Find which test sets have repeated measurements
  std::vector<std::string>* tray_strings = gReader->GetTrayStrings();
  std::vector<std::string>::iterator iterator_main_test = tray_strings->end();
  int count_index = 0;
  for (std::vector<std::string>::iterator it = tray_strings->begin(); it != tray_strings->end(); ++it) {
    if (it->compare(base_tray_id) == 0) {iterator_main_test = it; break;}
    ++count_index;
  } if (iterator_main_test == tray_strings->end()) {
    std::cout << "Base tray not found in read data. Check input or add data to ../batch_data.txt" << std::endl;
    return;
  }
  
  
  // iterate through next ones while the substring is still the same.
  // Determine which sets of measurements are repeated among all repetitions
  // Should be the same but good to check
  std::vector<int> tray_indices;
  std::vector<bool> repeated;
  std::cout << "Base tray " << t_blu << base_tray_id << t_def << " found! Appending repeated tests..." << std::endl;
  for (std::vector<std::string>::iterator it = iterator_main_test; it != tray_strings->end(); ++it) {
    if (it->substr(0,base_tray_id.size()).compare(base_tray_id) == 0) {
      std::cout << t_blu << *it << t_def << " (index " << count_index << ")." << std::endl;
      tray_indices.push_back(count_index);
      
      if (tray_indices.size() == 1) {++count_index; continue;} // first should have all tests
      
      // Find which sets are repeated
      std::cout << "Repeated sets: { " << t_mgn;
      if (repeated.size() == 0) { // set repetitions
        for (int i = 0; i < 15; ++i) {
          if (gReader->HasSet(count_index, i)) repeated.push_back(1); // data available
          else                                 repeated.push_back(0); // no data
          if (repeated.back()) std::cout << i << ' ';
        }std::cout << t_def << "}" << std::endl;
      } else {// check that all subsequent tests have the same repeated measurements
        for (int i = 0; i < 15; ++i) if (repeated[i] == 1) {
          if ((repeated[i] && !gReader->HasSet(count_index, i)) ||
              (!repeated[i] && gReader->HasSet(count_index, i))) {
            std::cout << "Bad repeated sets on " << tray_strings->at(count_index) << "!" << std::endl;
            ++count_index;
            tray_indices.back() = -1;
            continue;
          }
        }// made it with all repetitions aligning--good tray.
        std::cout << t_grn << "same! " << t_def << '}' << std::endl;
        
      }
    }
    ++count_index;
  }
  
  if (tray_indices.size() < 2) {
    std::cout << "No further tests found for this tray, results will not be statistically meaningful. Terminating..." << std::endl;
    return;
  }
  
  // Repetition analysis loop -- one for each repeated test set
  double avg_this_tray_IV = getAvgVpeak(tray_indices[0], global_flag_run_at_25_celcius);
  double avg_this_tray_SPS = getAvgVbreakdown(tray_indices[0], global_flag_run_at_25_celcius);
  bool flag_padded = true;
  for (int r = 0; r < 15; ++r) {
    
    // Begin performing repetition analysis: gather data
    bool has_failed_measurements_IV[8][4];
    bool has_failed_measurements_SPS[8][4];
    for (int s = 0; s < 32; ++s) {
      has_failed_measurements_IV[s/4][s%4] = false;
      has_failed_measurements_SPS[s/4][s%4] = false;
    }
    TH1D* repetition_hists_IV[8][4];
    TH1D* repetition_hists_SPS[8][4];
    
    if (!repeated[r]) continue;
    float ylim = 0;
    int total_trays = 0;
    for (int s = 0; s < 32; ++s) {
      // Note that the 14th set only has 12 SiPMs
      if (r == 14 && s == 12) break;
      
      // find the average for these repeated measurements
      double avg_this_sipm_IV = 0;
      double avg_this_sipm_SPS = 0;
      
      for (int i = 0; i < tray_indices.size(); ++i) {
        if (tray_indices[i] == -1) continue; // flag for bad tray, in case I need it later
        
        // Check for failed measurements and tally for later (IV)
        if (gReader->GetVbdTestIndexIV(tray_indices[i], r, s, global_flag_run_at_25_celcius) == -999) {
          has_failed_measurements_IV[s/4][s%4] = true;
          std::pair<int,int> index = gReader->GetTrayIndexFromTestIndex(r, s);
          std::cout << t_red << "Bad IV measurement" << t_def << " in tray index " << tray_indices[i] << ", with SiPM (";
          std::cout << index.first << ',' << index.second << ")." << std::endl;
          continue;
        }avg_this_sipm_IV += gReader->GetVbdTestIndexIV(tray_indices[i], r, s, global_flag_run_at_25_celcius);
        
        if (reproducibility_skip_SPS) goto nextloop;
        
        // Check for failed measurements and tally for later (SPS)
        if (gReader->GetVbdTestIndexSPS(tray_indices[i], r, s, global_flag_run_at_25_celcius) == -999) {
          has_failed_measurements_SPS[s/4][s%4] = true;
          std::pair<int,int> index = gReader->GetTrayIndexFromTestIndex(r, s);
          std::cout << t_red << "Bad SPS measurement" << t_def << " in tray " << gReader->GetTrayStrings()->at(tray_indices[i]);
          std::cout << ", with SiPM (" << index.first << ',' << index.second << ")." << std::endl;
          continue;
        }avg_this_sipm_SPS += gReader->GetVbdTestIndexSPS(tray_indices[i], r, s, global_flag_run_at_25_celcius);
        
      nextloop:
        // Tally total good SiPM tests
        if (s == 0) total_trays += 1;
      }// End of tray loop
      avg_this_sipm_IV /= total_trays;
      avg_this_sipm_SPS /= total_trays;
      ylim = total_trays + 1.5;
      // IV histogram
      repetition_hists_IV[s/4][s%4] = new TH1D(Form("hist_IV_Vbr_set%i_(%i,%i)",r,(r*32 + s)/23,(r*32 + s)%23),
                                               ";V_{br} [V];Counts", 12,
                                               avg_this_tray_IV + volthist_range[0],
                                               avg_this_tray_IV + volthist_range[1]);
      int color_to_use_IV = plot_colors[0];
      if (has_failed_measurements_IV[s/4][s%4]) color_to_use_IV = plot_colors[2];
      repetition_hists_IV[s/4][s%4]->SetLineColor(color_to_use_IV);
      repetition_hists_IV[s/4][s%4]->SetFillColorAlpha(color_to_use_IV, 0.25);
      repetition_hists_IV[s/4][s%4]->SetMarkerColor(color_to_use_IV);
      repetition_hists_IV[s/4][s%4]->GetXaxis()->SetNdivisions(203);
      repetition_hists_IV[s/4][s%4]->GetYaxis()->SetNdivisions(204);
      repetition_hists_IV[s/4][s%4]->GetYaxis()->SetRangeUser(0, ylim);
      
      // SPS histogram
      if (!reproducibility_skip_SPS) {
        repetition_hists_SPS[s/4][s%4] = new TH1D(Form("hist_SPS_Vbr_set%i_(%i,%i)",r,(r*32 + s)/23,(r*32 + s)%23),
                                                  ";V_{br} [V];Counts", 12,
                                                  avg_this_tray_SPS + volthist_range[0],
                                                  avg_this_tray_SPS + volthist_range[1]);
        int color_to_use_SPS = plot_colors[1];
        if (has_failed_measurements_SPS[s/4][s%4]) color_to_use_SPS = plot_colors[2];
        repetition_hists_SPS[s/4][s%4]->SetLineColor(color_to_use_SPS);
        repetition_hists_SPS[s/4][s%4]->SetFillColorAlpha(color_to_use_SPS, 0.25);
        repetition_hists_SPS[s/4][s%4]->SetMarkerColor(color_to_use_SPS);
        repetition_hists_SPS[s/4][s%4]->GetXaxis()->SetNdivisions(203);
        repetition_hists_SPS[s/4][s%4]->GetYaxis()->SetNdivisions(204);
        repetition_hists_SPS[s/4][s%4]->GetYaxis()->SetRangeUser(0, ylim);
      }
      
      // fill the single test histograms
      for (int i = 0; i < tray_indices.size(); ++i) {
        if (tray_indices[i] == -1) continue; // flag for bad tray, in case I need it later
        repetition_hists_IV[s/4][s%4]->Fill(gReader->GetVbdTestIndexIV(tray_indices[i], r, s, global_flag_run_at_25_celcius));
        if (reproducibility_skip_SPS) continue;
        repetition_hists_SPS[s/4][s%4]->Fill(gReader->GetVbdTestIndexSPS(tray_indices[i], r, s, global_flag_run_at_25_celcius));
      }
      
      // Fill the global histograms with residual/stdev data
      double stdev[2] = {0,0};
      // only use SiPMs with all ok measurements for consistency
      if (!has_failed_measurements_IV[s/4][s%4] &&
          (!reproducibility_skip_SPS && !has_failed_measurements_SPS[s/4][s%4])) {
        for (int i = 0; i < tray_indices.size(); ++i) {
          if (tray_indices[i] == -1) continue; // flag for bad tray, in case I need it later
          // Fill global residual histograms
          double dev_this_meas_IV = gReader->GetVbdTestIndexIV(tray_indices[i], r, s, global_flag_run_at_25_celcius) - avg_this_sipm_IV;
          gHist_rep_residual[0]->Fill(dev_this_meas_IV*1000);
          stdev[0] += dev_this_meas_IV*dev_this_meas_IV;
          
          // Add option to skip SPS if only IV data is of interest
          if (reproducibility_skip_SPS) continue;
          double dev_this_meas_SPS = gReader->GetVbdTestIndexSPS(tray_indices[i], r, s, global_flag_run_at_25_celcius) - avg_this_sipm_SPS;
          gHist_rep_residual[1]->Fill(dev_this_meas_SPS*1000);
          stdev[1] += dev_this_meas_SPS*dev_this_meas_SPS;
        }// End of Histogram fill
        
        // Fill global stdev histogram
        gHist_rep_stdev[0]->Fill(std::sqrt(stdev[0])*1000); // IV
        if (!reproducibility_skip_SPS)
          gHist_rep_stdev[1]->Fill(std::sqrt(stdev[1])*1000); // SPS
//        std::cout << "SPS error in cassette index " << s << " after computing : " << std::sqrt(stdev[1])*1000 << " mV" << std::endl;
        
        // Add stdev to error counter--for taking average later
        gRepError_IV[global_flag_run_at_25_celcius] += std::sqrt(stdev[0]); // IV
        if (!reproducibility_skip_SPS)
          gRepError_SPS[global_flag_run_at_25_celcius] += std::sqrt(stdev[1]); // SPS
        
        // Fitting to gaussian with LogLikelihood (best for low count hists)
        
        // TODO
      }// End of check for good SiPM tests
    }// End of cassette loop
    
    // Only draw plots if desired
    if (drawplot) {
      // *------- Visual plot elements
      
      
      
      TLine* avg_line = new TLine();
      avg_line->SetLineColorAlpha(kBlack, 0.5);
      
      TLine* dev_line = new TLine();
      dev_line->SetLineColorAlpha(kGray+1, 1);
      dev_line->SetLineStyle(7);
      
      TBox* forbidden_range_box = new TBox();
      forbidden_range_box->SetFillColorAlpha(kRed+2, 0.25);
      
      
      // *------- IV Plots
      
      for (int s = 0; s < 32; ++s) {
        cassette_pads[3-s%4][7-s/4]->cd();
        gPad->SetTicks(1,1);
        //      std::cout << t_red << "debug!" << t_def << std::endl;
        
        // Add extra padding to the canvases to split them from flush if desired
        if (flag_padded) {
          double extra_padding = 0.0185;
          gPad->SetLeftMargin(gPad->GetLeftMargin() + extra_padding);
          gPad->SetTopMargin(gPad->GetTopMargin() + extra_padding);
          gPad->SetRightMargin(gPad->GetRightMargin() + extra_padding);
          gPad->SetBottomMargin(gPad->GetBottomMargin() + extra_padding);
          if (s == 31) flag_padded = false;
        }
        
        // Ensure all pads have the same tick/text sizes
        double aspect_vert = (1 - gPad->GetTopMargin() - gPad->GetBottomMargin());
        double aspect_horiz = (1 - gPad->GetLeftMargin() - gPad->GetRightMargin());
        double aspect_ratio = aspect_vert / aspect_horiz;
        repetition_hists_IV[s/4][s%4]->GetXaxis()->SetTickLength(0.06 * aspect_ratio);
        repetition_hists_IV[s/4][s%4]->GetYaxis()->SetTickLength(0.06 / aspect_ratio);
        repetition_hists_IV[s/4][s%4]->GetXaxis()->SetLabelSize(0.08 * aspect_horiz);
        repetition_hists_IV[s/4][s%4]->GetXaxis()->SetLabelOffset(0.02 / aspect_horiz/aspect_horiz);
        repetition_hists_IV[s/4][s%4]->GetXaxis()->SetTitleSize(0.09 * aspect_horiz);
        repetition_hists_IV[s/4][s%4]->GetXaxis()->SetTitleOffset(1 / aspect_horiz);
        repetition_hists_IV[s/4][s%4]->GetYaxis()->SetLabelSize(0.08 * aspect_vert);
        repetition_hists_IV[s/4][s%4]->GetYaxis()->SetLabelOffset(0.02 / aspect_vert);
        repetition_hists_IV[s/4][s%4]->GetYaxis()->SetLabelOffset(0.02 / aspect_vert);
        repetition_hists_IV[s/4][s%4]->GetYaxis()->SetTitleSize(0.09 * aspect_vert);
        
        // Draw the hist and helpful visual features
        repetition_hists_IV[s/4][s%4]->Draw("hist");
        avg_line->DrawLine(avg_this_tray_IV, 0, avg_this_tray_IV, ylim);
        
        forbidden_range_box->DrawBox(avg_this_tray_IV + volthist_range[0], 0, avg_this_tray_IV - 0.05, ylim);
        forbidden_range_box->DrawBox(avg_this_tray_IV + 0.05, 0, avg_this_tray_IV + volthist_range[1], ylim);
        
        dev_line->DrawLine(avg_this_tray_IV+0.05, 0, avg_this_tray_IV+0.05, ylim);
        dev_line->DrawLine(avg_this_tray_IV-0.05, 0, avg_this_tray_IV-0.05,  ylim);
        
        // Label this SiPM
        std::pair<int, int> sipm_tray_index = gReader->GetTrayIndexFromTestIndex(r, s);
        drawText(Form("(%i,%i)",sipm_tray_index.first, sipm_tray_index.second),
                 gPad->GetLeftMargin() + 0.2*aspect_horiz, gPad->GetBottomMargin() + 0.86*aspect_vert,
                 false, kBlack, 0.1*std::sqrt(aspect_horiz*aspect_vert));
      }
      
      // Draw some text giving info on the setup
      gCanvas_cassetteplot->cd();
      TLatex* top_tex[6];
      top_tex[0] = drawText("#bf{Debrecen} SiPM Test Setup @ #bf{Yale}",                 0.025, 0.91, false, kBlack, 0.04);
      top_tex[1] = drawText("#bf{ePIC} Test Stand",                                      0.025, 0.955, false, kBlack, 0.045);
      top_tex[2] = drawText(Form("Hamamatsu #bf{%s}", Hamamatsu_SiPM_Code),              0.995, 0.95, true, kBlack, 0.045);
      top_tex[3] = drawText(Form("%s", string_tempcorr[global_flag_run_at_25_celcius]),  0.995, 0.905, true, kBlack, 0.035);
      top_tex[4] = drawText("IV Reproducibility",                                        0.42, 0.95, false, kBlack, 0.045);
      top_tex[5] = drawText(Form("Tray #bf{%s}: (Set %i)#times#color[2]{%i}",
                                 base_tray_id.c_str(), r,total_trays),                   0.40, 0.905, false, kBlack, 0.035);
      
      
      gCanvas_cassetteplot->SaveAs(Form("../plots/systematic_plots/reproducibility%s/%s-set%i-rep%lu-IV.pdf",
                                        string_tempcorr_short[global_flag_run_at_25_celcius],base_tray_id.c_str(), r, tray_indices.size()));
      
      // *------- SPS Plots
      
      for (int s = 0; s < 32; ++s) {
        cassette_pads[3-s%4][7-s/4]->cd();
        gPad->SetTicks(1,1);
        // Ensure all pads have the same tick sizes
        double aspect_vert = (1 - gPad->GetTopMargin() - gPad->GetBottomMargin());
        double aspect_horiz = (1 - gPad->GetLeftMargin() - gPad->GetRightMargin());
        double aspect_ratio = aspect_vert / aspect_horiz;
        repetition_hists_SPS[s/4][s%4]->GetXaxis()->SetTickLength(0.06 * aspect_ratio);
        repetition_hists_SPS[s/4][s%4]->GetYaxis()->SetTickLength(0.06 / aspect_ratio);
        repetition_hists_SPS[s/4][s%4]->GetXaxis()->SetLabelSize(0.08 * aspect_horiz);
        repetition_hists_SPS[s/4][s%4]->GetXaxis()->SetLabelOffset(0.02 / aspect_horiz/aspect_horiz);
        repetition_hists_SPS[s/4][s%4]->GetXaxis()->SetTitleSize(0.09 * aspect_horiz);
        repetition_hists_SPS[s/4][s%4]->GetXaxis()->SetTitleOffset(1 / aspect_horiz);
        repetition_hists_SPS[s/4][s%4]->GetYaxis()->SetLabelSize(0.08 * aspect_vert);
        repetition_hists_SPS[s/4][s%4]->GetYaxis()->SetLabelOffset(0.02 / aspect_vert);
        repetition_hists_SPS[s/4][s%4]->GetYaxis()->SetLabelOffset(0.02 / aspect_vert);
        repetition_hists_SPS[s/4][s%4]->GetYaxis()->SetTitleSize(0.09 * aspect_vert);
        repetition_hists_SPS[s/4][s%4]->Draw("hist");
        avg_line->DrawLine(avg_this_tray_SPS, 0, avg_this_tray_SPS, ylim);
        
        forbidden_range_box->DrawBox(avg_this_tray_SPS + volthist_range[0], 0, avg_this_tray_SPS - 0.05, ylim);
        forbidden_range_box->DrawBox(avg_this_tray_SPS + 0.05, 0, avg_this_tray_SPS + volthist_range[1], ylim);
        
        dev_line->DrawLine(avg_this_tray_SPS+0.05, 0, avg_this_tray_SPS+0.05, ylim);
        dev_line->DrawLine(avg_this_tray_SPS-0.05, 0, avg_this_tray_SPS-0.05,  ylim);
        
        std::pair<int, int> sipm_tray_index = gReader->GetTrayIndexFromTestIndex(r, s);
        drawText(Form("(%i,%i)",sipm_tray_index.first, sipm_tray_index.second),
                 gPad->GetLeftMargin() + 0.2*aspect_horiz, gPad->GetBottomMargin() + 0.86*aspect_vert,
                 false, kBlack, 0.1*std::sqrt(aspect_horiz*aspect_vert));
      }
      
      // Correct the IV text to SPS
      gCanvas_cassetteplot->cd();
      top_tex[4]->Clear();
      top_tex[4] = drawText("SPS Reproducibility", 0.415, 0.95, false, kBlack, 0.045);
      
      gCanvas_cassetteplot->SaveAs(Form("../plots/systematic_plots/reproducibility%s/%s-set%i-rep%lu-SPS.pdf",
                                        string_tempcorr_short[global_flag_run_at_25_celcius],base_tray_id.c_str(), r, tray_indices.size()));
      
      // Clear latex for next run
      for (int iTex = 0; iTex < 6; ++iTex) top_tex[iTex]->Clear();
      
      // Clear repetition hists
      for (int s = 0; s < 32; ++s) {
        delete repetition_hists_IV[s/4][s%4];
        delete repetition_hists_SPS[s/4][s%4];
      }
    } // End of plot drawing
  }// End of repetition/measurement set loop
  return;
}// End of systematic_analysis_summary::makeReproducabilityHist

//========================================================================== Temperature Systematics



// Analyse the data from the special temperature scan systematic
// This was a special one time test from when the lab was overheated
// Several measurements of the same 4 SiPMs were taken as the lab cooled
// enabling a scan over temperature which would not otherwise be possible
// in our setup. 
//
// This method assumes that the contiguous string "tempscan"
// is in the run notes/batch strings and only includes such data.
void makeTemperatureScan() {
  bool temp_debug = false;
  
  // Find strings with tempscan
  std::vector<int> tempscan_tray_indices;
  std::vector<std::string> tempscan_tray_strings;
  for (int i_tray = 0; i_tray < gReader->GetTrayStrings()->size(); ++i_tray) {
    if (gReader->GetTrayStrings()->at(i_tray).find("tempscan") != -1) {
      std::string swapstring = gReader->GetTrayStrings()->at(i_tray);
      tempscan_tray_indices.push_back(i_tray);
      tempscan_tray_strings.push_back(swapstring);
      
      std::cout << "Good Temperature scan tray found at index " << t_blu << i_tray << t_def;
      std::cout << " (" << t_grn << gReader->GetTrayStrings()->at(i_tray) << t_def << ")" << std::endl;
    }
  }
  
  // Check that vopscan values were found, return if not
  if (tempscan_tray_indices.size() == 0) {
    std::cout << "Warning in systematic_analysis_summary::makeTemperatureScan: No trays with \"tempscan\" found in dataset." << std::endl;
    std::cout << "Check input batch file to verify tempscan data are available." << std::endl;
    return;
  }
  
  // Initialize data arrays
  
  // SiPM identifer data
  std::vector<int> sipm_row;
  std::vector<int> sipm_col;
  
  // V_breakdown Data
  std::vector<std::vector<float> > Vbr_IV;
  std::vector<std::vector<float> > Vbr_25_IV;
  std::vector<std::vector<float> > Vbr_SPS;
  std::vector<std::vector<float> > Vbr_25_SPS;
  // V_breakdown Error (folded in from other systematics)
  std::vector<std::vector<float> > Vbr_IV_err;
  std::vector<std::vector<float> > Vbr_25_IV_err;
  std::vector<std::vector<float> > Vbr_SPS_err;
  std::vector<std::vector<float> > Vbr_25_SPS_err;
  // Temperature and Error
  std::vector<std::vector<float> > temp_IV;
  std::vector<std::vector<float> > temp_SPS;
  std::vector<std::vector<float> > temp_IV_err;
  std::vector<std::vector<float> > temp_SPS_err;
  
  // Gather relevant data from gReader
  for (int i_temp = 0; i_temp < tempscan_tray_indices.size(); ++i_temp) {
    if (temp_debug) std::cout << "current tray :: " << gReader->GetIV()->at(tempscan_tray_indices[i_temp])->tray_note << std::endl;
    
    // access relevant data from reader
    std::vector<int>* current_sipm_row = gReader->GetIV()->at(tempscan_tray_indices[i_temp])->row;
    std::vector<int>* current_sipm_col = gReader->GetIV()->at(tempscan_tray_indices[i_temp])->col;
    
    std::vector<float>* current_Vbr_IV = gReader->GetIV()->at(tempscan_tray_indices[i_temp])->IV_Vpeak;
    std::vector<float>* current_Vbr_25_IV = gReader->GetIV()->at(tempscan_tray_indices[i_temp])->IV_Vpeak_25C;
    std::vector<float>* current_Vbr_SPS = gReader->GetSPS()->at(tempscan_tray_indices[i_temp])->SPS_Vbd;
    std::vector<float>* current_Vbr_25_SPS = gReader->GetSPS()->at(tempscan_tray_indices[i_temp])->SPS_Vbd_25C;
    
    std::vector<float>* current_temp_IV = gReader->GetIV()->at(tempscan_tray_indices[i_temp])->avg_temp;
    std::vector<float>* current_temp_SPS = gReader->GetSPS()->at(tempscan_tray_indices[i_temp])->avg_temp;
    std::vector<float>* current_temp_IV_err = gReader->GetIV()->at(tempscan_tray_indices[i_temp])->stdev_temp;
    std::vector<float>* current_temp_SPS_err = gReader->GetSPS()->at(tempscan_tray_indices[i_temp])->stdev_temp;
    
    for (int i_sipm = 0; i_sipm < current_Vbr_IV->size(); ++i_sipm) {
      if (current_Vbr_IV->at(i_sipm) == -999) continue;
      if (temp_debug) {
        std::cout << "IV V_br for SiPM " << i_sipm << " = " << current_Vbr_IV->at(i_sipm) << std::endl;
        std::cout << "SPS V_br for SiPM " << i_sipm << " = " << current_Vbr_SPS->at(i_sipm) << std::endl;
      }
      
      // Append data so that each vector is one SiPM--for converting to TGraph later
      if (i_temp == 0) {
        // SiPM indexing
        sipm_row.push_back(current_sipm_row->at(i_sipm));
        sipm_col.push_back(current_sipm_col->at(i_sipm));
        
        // Temperature scan voltage data
        Vbr_IV.push_back(std::vector<float>());
        Vbr_25_IV.push_back(std::vector<float>());
        Vbr_SPS.push_back(std::vector<float>());
        Vbr_25_SPS.push_back(std::vector<float>());
        
        // Temperature scan voltage error -- currently just avg stdev systematic from reproducibility
        Vbr_IV_err.push_back(std::vector<float>());
        Vbr_25_IV_err.push_back(std::vector<float>());
        Vbr_SPS_err.push_back(std::vector<float>());
        Vbr_25_SPS_err.push_back(std::vector<float>());
        
        // Measured temperature and error
        temp_IV.push_back(std::vector<float>());
        temp_SPS.push_back(std::vector<float>());
        temp_IV_err.push_back(std::vector<float>());
        temp_SPS_err.push_back(std::vector<float>());
        
        if (temp_debug) std::cout << "push back new SiPM...(" << sipm_row.back() << ',' << sipm_col.back() << ")." << std::endl;
      }
      
      // Temperature scan voltage data
      Vbr_IV[i_sipm].push_back(current_Vbr_IV->at(i_sipm));
      Vbr_25_IV[i_sipm].push_back(current_Vbr_25_IV->at(i_sipm));
      Vbr_SPS[i_sipm].push_back(current_Vbr_SPS->at(i_sipm));
      Vbr_25_SPS[i_sipm].push_back(current_Vbr_25_SPS->at(i_sipm));
      
      // Temperature scan voltage error -- currently just avg stdev systematic from reproducibility
      Vbr_IV_err[i_sipm].push_back(gRepError_IV[0]);     // IV - not temp corrected
      Vbr_25_IV_err[i_sipm].push_back(gRepError_IV[1]);  // IV - temp corrected
      Vbr_SPS_err[i_sipm].push_back(gRepError_SPS[0]);   // SPS - not temp corrected
      Vbr_25_SPS_err[i_sipm].push_back(gRepError_SPS[1]); // SPS - temp corrected
      
      // Measured temperature and error
      temp_IV[i_sipm].push_back(current_temp_IV->at(i_sipm));
      temp_SPS[i_sipm].push_back(current_temp_SPS->at(i_sipm));
      temp_IV_err[i_sipm].push_back(current_temp_IV_err->at(i_sipm));
      temp_SPS_err[i_sipm].push_back(current_temp_SPS_err->at(i_sipm));
    }// End of SiPM loop
  }// End of Temperature scan file loop
  
  
  // Make TGraphs of data over the V_op scan
  char data_plot_option[5] = "p 2";
  const int ntotal_scan = Vbr_IV[2].size();
  const int ntotal_sipm = Vbr_IV.size();
  std::cout << "ntotal_scan = " << ntotal_scan << std::endl;
  std::cout << "ntotal_sipm = " << ntotal_sipm << std::endl;
  
  // Data graphs
  TGraphErrors* sipm_tempscan_graph_IV[ntotal_sipm];
  TGraphErrors* sipm_tempscan_graph_IV_25[ntotal_sipm];
  TGraphErrors* sipm_tempscan_graph_SPS[ntotal_sipm];
  TGraphErrors* sipm_tempscan_graph_SPS_25[ntotal_sipm];
  TMultiGraph*  sipm_tempscan_multigraph[ntotal_sipm];
  
  // Fit
  TF1* linfit_IV[ntotal_sipm];
  TF1* linfit_IV_25[ntotal_sipm];
  TF1* linfit_SPS[ntotal_sipm];
  TF1* linfit_SPS_25[ntotal_sipm];
  
  // Plot data and store plots
  for (int i_sipm = 0; i_sipm < ntotal_sipm; ++i_sipm) {
    // *-- Prepare the TGraph objects
    sipm_tempscan_multigraph[i_sipm] = new TMultiGraph();
    
    sipm_tempscan_graph_IV[i_sipm] = new TGraphErrors(ntotal_scan,
                                                     temp_IV[i_sipm].data(), Vbr_IV[i_sipm].data(),
                                                     temp_IV_err[i_sipm].data(), Vbr_IV_err[i_sipm].data());
    sipm_tempscan_graph_IV[i_sipm]->SetFillColorAlpha(plot_colors[0], 0.5);
    sipm_tempscan_graph_IV[i_sipm]->SetLineColor(plot_colors[0]);
    sipm_tempscan_graph_IV[i_sipm]->SetLineWidth(2);
    sipm_tempscan_graph_IV[i_sipm]->SetMarkerColor(plot_colors[0]);
    sipm_tempscan_graph_IV[i_sipm]->SetMarkerStyle(53);
    sipm_tempscan_graph_IV[i_sipm]->SetMarkerSize(1.7);
    sipm_tempscan_multigraph[i_sipm]->Add(sipm_tempscan_graph_IV[i_sipm], data_plot_option);
    
    sipm_tempscan_graph_IV_25[i_sipm] = new TGraphErrors(ntotal_scan,
                                                        temp_IV[i_sipm].data(), Vbr_25_IV[i_sipm].data(),
                                                        temp_IV_err[i_sipm].data(), Vbr_25_IV_err[i_sipm].data());
    sipm_tempscan_graph_IV_25[i_sipm]->SetFillColorAlpha(plot_colors_alt[0], 0.5);
    sipm_tempscan_graph_IV_25[i_sipm]->SetLineColor(plot_colors_alt[0]);
    sipm_tempscan_graph_IV_25[i_sipm]->SetMarkerColor(plot_colors_alt[0]);
    sipm_tempscan_graph_IV_25[i_sipm]->SetMarkerStyle(20);
    sipm_tempscan_graph_IV_25[i_sipm]->SetMarkerSize(1.7);
    sipm_tempscan_multigraph[i_sipm]->Add(sipm_tempscan_graph_IV_25[i_sipm], data_plot_option);
    
    sipm_tempscan_graph_SPS[i_sipm] = new TGraphErrors(ntotal_scan,
                                                      temp_SPS[i_sipm].data(), Vbr_SPS[i_sipm].data(),
                                                      temp_SPS_err[i_sipm].data(), Vbr_SPS_err[i_sipm].data());
    sipm_tempscan_graph_SPS[i_sipm]->SetFillColorAlpha(plot_colors[1], 0.5);
    sipm_tempscan_graph_SPS[i_sipm]->SetLineColor(plot_colors[1]);
    sipm_tempscan_graph_SPS[i_sipm]->SetMarkerColor(plot_colors[1]);
    sipm_tempscan_graph_SPS[i_sipm]->SetMarkerStyle(54);
    sipm_tempscan_graph_SPS[i_sipm]->SetMarkerSize(1.7);
    sipm_tempscan_multigraph[i_sipm]->Add(sipm_tempscan_graph_SPS[i_sipm], data_plot_option);
    
    sipm_tempscan_graph_SPS_25[i_sipm] = new TGraphErrors(ntotal_scan,
                                                         temp_SPS[i_sipm].data(), Vbr_25_SPS[i_sipm].data(),
                                                         temp_SPS_err[i_sipm].data(), Vbr_25_SPS_err[i_sipm].data());
    sipm_tempscan_graph_SPS_25[i_sipm]->SetFillColorAlpha(plot_colors_alt[1], 0.5);
    sipm_tempscan_graph_SPS_25[i_sipm]->SetLineColor(plot_colors_alt[1]);
    sipm_tempscan_graph_SPS_25[i_sipm]->SetMarkerColor(plot_colors_alt[1]);
    sipm_tempscan_graph_SPS_25[i_sipm]->SetMarkerStyle(21);
    sipm_tempscan_graph_SPS_25[i_sipm]->SetMarkerSize(1.7);
    sipm_tempscan_multigraph[i_sipm]->Add(sipm_tempscan_graph_SPS_25[i_sipm], data_plot_option);
    
    
    
    // *-- Perform fitting to linear map and estimate flatness from error on the slope
    float rangelim[2] = {0, 50};
    float slopelim[2] = {-0.07, 0.07};
    // Usefil ptions for fitting:
    //    Q - Quiet
    //    W - Ignore errors
    //    F - Use TMinuit for poly
    //    R - use rangelim only for fitting
    //    EX0 - ignore x axis errors
    char fitoption[5] = "Q";
    linfit_IV[i_sipm] = new TF1(Form("linfit_IV_%i_%i", sipm_row[i_sipm],sipm_col[i_sipm]),
                                     "[0] + [1]*(x-25)", rangelim[0], rangelim[1]);
    linfit_IV[i_sipm]->SetLineColor(plot_colors[0]);
    linfit_IV[i_sipm]->SetLineStyle(7);
    linfit_IV[i_sipm]->SetParLimits(1, slopelim[0], slopelim[1]);
    linfit_IV[i_sipm]->SetParameter(getAvgFromVector(Vbr_IV[i_sipm]), 0.034); // From Debrecen temp correction coefficient
    sipm_tempscan_graph_IV[i_sipm]->Fit(linfit_IV[i_sipm], fitoption);

    linfit_IV_25[i_sipm] = new TF1(Form("linfit_IV_25C_%i_%i", sipm_row[i_sipm],sipm_col[i_sipm]),
                                   "[0] + [1]*(x-25)", rangelim[0], rangelim[1]);
    linfit_IV_25[i_sipm]->SetLineColor(plot_colors_alt[0]);
    linfit_IV_25[i_sipm]->SetLineStyle(5);
    linfit_IV_25[i_sipm]->SetParLimits(1, slopelim[0], slopelim[1]);
    linfit_IV_25[i_sipm]->SetParameters(getAvgFromVector(Vbr_25_IV[i_sipm]), 0); // seed null hypothesis as a guess
    sipm_tempscan_graph_IV_25[i_sipm]->Fit(linfit_IV_25[i_sipm], fitoption);

    linfit_SPS[i_sipm] = new TF1(Form("linfit_SPS_%i_%i", sipm_row[i_sipm],sipm_col[i_sipm]),
                                 "[0] + [1]*(x-25)", rangelim[0], rangelim[1]);
    linfit_SPS[i_sipm]->SetLineColor(plot_colors[1]);
    linfit_SPS[i_sipm]->SetLineStyle(7);
    linfit_SPS[i_sipm]->SetParLimits(1, slopelim[0], slopelim[1]);
    linfit_SPS[i_sipm]->SetParameters(getAvgFromVector(Vbr_25_IV[i_sipm]), 0.034); // From Debrecen temp correction coefficient
    sipm_tempscan_graph_SPS[i_sipm]->Fit(linfit_SPS[i_sipm], fitoption);

    linfit_SPS_25[i_sipm] = new TF1(Form("linfit_SPS_25C_%i_%i", sipm_row[i_sipm],sipm_col[i_sipm]),
                                    "[0] + [1]*(x-25)", rangelim[0], rangelim[1]);
    linfit_SPS_25[i_sipm]->SetLineColor(plot_colors_alt[1]);
    linfit_SPS_25[i_sipm]->SetLineStyle(5);
    linfit_SPS_25[i_sipm]->SetParLimits(1, slopelim[0], slopelim[1]);
    linfit_SPS_25[i_sipm]->SetParameters(getAvgFromVector(Vbr_25_SPS[i_sipm]), 0); // seed null hypothesis as a guess
    sipm_tempscan_graph_SPS_25[i_sipm]->Fit(linfit_SPS_25[i_sipm], fitoption);
    
    // Prepare the canvas
    gCanvas_solo->cd();
    gCanvas_solo->Clear();
    gPad->SetTicks(1,1);
    gPad->SetRightMargin(0.015);
    gPad->SetBottomMargin(0.08);
    gPad->SetLeftMargin(0.09);
    
    // Plot the graphs--base layer
    sipm_tempscan_multigraph[i_sipm]->SetTitle(";Average Temperature During Test [#circC];Measured V_{br} [V]");
    sipm_tempscan_multigraph[i_sipm]->GetYaxis()->SetRangeUser(voltplot_limits[0],voltplot_limits[1]);
    sipm_tempscan_multigraph[i_sipm]->GetYaxis()->SetTitleOffset(1.2);
    sipm_tempscan_multigraph[i_sipm]->GetXaxis()->SetTitleOffset(1.0);
    sipm_tempscan_multigraph[i_sipm]->Draw("a");
    
    // Add reference lines
    TLine* typ_line = new TLine();
    typ_line->SetLineStyle(8);
    typ_line->SetLineColor(kGray+1);
    typ_line->DrawLine(42.4, voltplot_limits[0], 42.4, voltplot_limits[1]);
    
    // Add legend--data
    TLegend* leg_data = new TLegend(0.15, 0.35, 0.45, 0.57);
    leg_data->SetLineWidth(0);
    leg_data->AddEntry(sipm_tempscan_graph_IV[i_sipm], "IV (ambient)", data_plot_option);
    leg_data->AddEntry(sipm_tempscan_graph_IV_25[i_sipm], "IV (25#circC)", data_plot_option);
    leg_data->AddEntry(sipm_tempscan_graph_SPS[i_sipm], "SPS (ambient)", data_plot_option);
    leg_data->AddEntry(sipm_tempscan_graph_SPS_25[i_sipm], "SPS (25#circC)", data_plot_option);
    leg_data->Draw();
    
    // Add legend--fitting
    TLegend* leg_fit = new TLegend(0.55, 0.39, 0.95, 0.58);
    leg_fit->SetLineWidth(0);
    leg_fit->SetTextSize(0.035);
    leg_fit->AddEntry(linfit_IV[i_sipm], Form("%.1f #pm %.1f",
                                              1000*linfit_IV[i_sipm]->GetParameter(1),
                                              1000*linfit_IV[i_sipm]->GetParError(1)), "l");
    leg_fit->AddEntry(linfit_IV_25[i_sipm], Form("%.1f #pm %.1f",
                                                 1000*linfit_IV_25[i_sipm]->GetParameter(1),
                                                 1000*linfit_IV_25[i_sipm]->GetParError(1)), "l");
    leg_fit->AddEntry(linfit_SPS[i_sipm], Form("%.1f #pm %.1f",
                                               1000*linfit_SPS[i_sipm]->GetParameter(1),
                                               1000*linfit_SPS[i_sipm]->GetParError(1)), "l");
    leg_fit->AddEntry(linfit_SPS_25[i_sipm], Form("%.1f #pm %.1f",
                                                  1000*linfit_SPS_25[i_sipm]->GetParameter(1),
                                                  1000*linfit_SPS_25[i_sipm]->GetParError(1)), "l");
    leg_fit->Draw();
    
    // Draw the chi2 separately to align them horizontally
    TLatex* tex_chi2[4];
    double base = 0.545;
    double diff = 0.047;
    tex_chi2[0] = drawText(Form("#chi^{2}/NDF = %.3f",
                                linfit_IV[i_sipm]->GetChisquare() /
                                linfit_IV[i_sipm]->GetNDF()),             0.775, base - 0*diff, false, kBlack, 0.035);
    tex_chi2[1] = drawText(Form("#chi^{2}/NDF = %.3f",
                                linfit_IV_25[i_sipm]->GetChisquare() /
                                linfit_IV_25[i_sipm]->GetNDF()),          0.775, base - 1*diff, false, kBlack, 0.035);
    tex_chi2[2] = drawText(Form("#chi^{2}/NDF = %.3f",
                                linfit_SPS[i_sipm]->GetChisquare() /
                                linfit_SPS[i_sipm]->GetNDF()),            0.775, base - 2*diff, false, kBlack, 0.035);
    tex_chi2[3] = drawText(Form("#chi^{2}/NDF = %.3f",
                                linfit_SPS_25[i_sipm]->GetChisquare() /
                                linfit_SPS_25[i_sipm]->GetNDF()),         0.775, base - 3*diff, false, kBlack, 0.035);
    
    // Draw some text about the fitting
    TLatex* tex_fit[3];
    tex_fit[0] = drawText("Fit Slope [mV/#circC]", 0.55, 0.6, false, kBlack, 0.04);
    tex_fit[2] = drawText("Hamamatsu Nominal: #bf{34 mV/#circC}", 0.55, 0.35, false, kBlack, 0.035);
    
    // Draw some informative text about the setup
    TLatex* top_tex[6];
    top_tex[0] = drawText("#bf{Debrecen} SiPM Test Setup @ #bf{Yale}",                 gPad->GetLeftMargin(), 0.91, false, kBlack, 0.04);
    top_tex[1] = drawText("#bf{ePIC} Test Stand",                                      gPad->GetLeftMargin(), 0.955, false, kBlack, 0.045);
    top_tex[2] = drawText(Form("Hamamatsu #bf{%s}", Hamamatsu_SiPM_Code),              1-gPad->GetRightMargin(), 0.95, true, kBlack, 0.045);
    top_tex[3] = drawText(Form("Tray %s SiPM (%i,%i)",
                               gReader->GetIV()->at(tempscan_tray_indices[0])->tray_note.substr(0,11).c_str(),
                               sipm_row[i_sipm],sipm_col[i_sipm]),                     1-gPad->GetRightMargin(), 0.91, true, kBlack, 0.035);
    top_tex[4] = drawText("Test Stand Systematics: Temperature Scan",                  gPad->GetLeftMargin() + 0.05, 0.83, false, kBlack, 0.04);
    top_tex[5] = drawText(Form("%i Total Tests During Cooldown",ntotal_scan),          gPad->GetLeftMargin() + 0.05, 0.78, false, kBlack, 0.04);
    
    // Write fit temperature correction and adjust for later tests
    if (global_flag_adjust_IV_tempcorr) {
      gTempcorr_IV = linfit_IV[i_sipm]->GetParameter(1);
      std::cout << "Adjusting IV temperature correction coefficient to fit result :: ";
      std::cout << t_blu << gTempcorr_IV << t_def << "." << std::endl;
    }
    
    
    
    // Might also be interesting to look at:
    // - Residuals against fit for closer look at potential nonlinearity
    // - Dist of fit coefficients + error relative to Hamamatsu nominal. Do we see consistent IV/SPS behavior?
    
    gCanvas_solo->SaveAs(Form("../plots/systematic_plots/temperature/tempscan_%i_%i_Vbr.pdf",
                              sipm_row[i_sipm],sipm_col[i_sipm]));
  }// End of SiPM plot construction loop
  return;
  
  
}// End of systematic_analysis_summary::makeTemperatureScan



// Check for a possible temperature gradient in the cassette test box
// We do this by constructing a histogram of temperature differences
// from the back temperature sensors to the forward ones in the same row.
void makeTemperatureGradientHist() {
  bool debug_temp_diff = false;
  const int temp_sensor_location[32] = {
    0, 0, 4, 4,
    0, 0, 4, 4,
    1, 1, 5, 5,
    1, 1, 5, 5,
    2, 2, 6, 6,
    2, 2, 6, 6,
    3, 3, 7, 7,
    3, 3, 7, 7
  };
  
  // Make histogram of temperature differences
  TH1D* hist_temp_difference_sensor_IV = new TH1D("hist_temp_difference_sensor_IV",
                                                  ";Front-to-Back Temperature Gradient (T_{front} - T_{back})/d [#circC/cm];Count of Measurement Pairs",
                                                  nbin_temp_grad,range_temp_grad[0],range_temp_grad[1]);
  hist_temp_difference_sensor_IV->SetFillColorAlpha(plot_colors_alt[0], 0.2); // 0.3 on solid fill
//  hist_temp_difference_sensor_IV->SetFillStyle(3017);
  hist_temp_difference_sensor_IV->SetLineColor(plot_colors_alt[0]);
  hist_temp_difference_sensor_IV->SetLineWidth(2);
  hist_temp_difference_sensor_IV->SetLineStyle(3);
  hist_temp_difference_sensor_IV->SetMarkerColor(plot_colors_alt[0]);
  
  TH1D* hist_temp_difference_sensor_SPS = new TH1D("hist_temp_difference_sensor_SPS",
                                                   ";Front-to-Back Temperature Gradient (T_{front} - T_{back})/d [#circC/cm];Count of Measurement Pairs",
                                                   nbin_temp_grad,range_temp_grad[0],range_temp_grad[1]);
  hist_temp_difference_sensor_SPS->SetFillColorAlpha(plot_colors_alt[1], 0.2);
//  hist_temp_difference_sensor_SPS->SetFillStyle(3018);
  hist_temp_difference_sensor_SPS->SetLineColor(plot_colors_alt[1]);
  hist_temp_difference_sensor_SPS->SetLineWidth(2);
  hist_temp_difference_sensor_SPS->SetLineStyle(3);
  hist_temp_difference_sensor_SPS->SetMarkerColor(plot_colors_alt[1]);
  
  // Loop over all trays included in the batch
  for (int i_tray = 0; i_tray < gReader->GetTrayStrings()->size(); ++i_tray) {
    
    // Check if this was a cycle test--indexing is more dynamic for these
    int cycle_pos = gReader->GetTrayStrings()->at(i_tray).find("cycle");
    
    // Define variable arrays
    std::vector<int>* current_sipm_row = gReader->GetIV()->at(i_tray)->row;
    std::vector<int>* current_sipm_col = gReader->GetIV()->at(i_tray)->col;
    std::vector<float>* current_temp_IV = gReader->GetIV()->at(i_tray)->avg_temp;
    std::vector<float>* current_temp_SPS = gReader->GetSPS()->at(i_tray)->avg_temp;
    std::vector<float>* current_temp_IV_err = gReader->GetIV()->at(i_tray)->stdev_temp;
    std::vector<float>* current_temp_SPS_err = gReader->GetSPS()->at(i_tray)->stdev_temp;
    
    // array of temperature values at cassette locations
    float temp_values_IV[8] = {
      -1,-1,-1,-1,
      -1,-1,-1,-1
    };
    float temp_values_SPS[8] = {
      -1,-1,-1,-1,
      -1,-1,-1,-1
    };
    
    if (debug_temp_diff) {
      std::cout << "Data for tray " << gReader->GetTrayStrings()->at(i_tray) << ": " << std::endl;
    }
    
    // Loop over SiPMs in the tray and append relevant temperature difference data
    for (int i_sipm = 0; i_sipm < current_temp_IV->size(); ++i_sipm) {
      int temp_loc = 0;
      if (current_temp_IV->at(i_sipm) == -999) {
        if (i_sipm % 32 == 31) goto checkfill;
        continue;
      }
      
      // Find the position of this SiPM in the test cassette
      int cassette_index_adjusted;
      if (cycle_pos != -1) {// Cycle test--handle dynamic positioning
        
        // Get base cassette index from cycle test index
        std::string swapstring = gReader->GetTrayStrings()->at(i_tray);
        int cycle_cassette_index = std::stoi( swapstring.substr(cycle_pos + 6, swapstring.size()) );
        
        // Modify for cycle conditions
        cassette_index_adjusted = ( (cycle_cassette_index + i_sipm) % 32 );
        
      } else {// Not cycle test--use standard positioning
        cassette_index_adjusted = i_sipm % 32;
        
      }
      
      // Assign temperature to this position in the array
      temp_loc = temp_sensor_location[cassette_index_adjusted];
      temp_values_IV[temp_loc] = current_temp_IV->at(i_sipm);
      temp_values_SPS[temp_loc] = current_temp_SPS->at(i_sipm);
      
      if (debug_temp_diff) {
        std::cout << "SiPM i=" << t_blu << i_sipm << t_def;
        std::cout << " is in location " << t_grn << cassette_index_adjusted << t_def;
        std::cout << ", temploc = " << t_red << temp_loc << t_def << "." << std::endl;
      }
      
      // hook from non-tested SiPMs
    checkfill:
      
      // Check if the cassette is complete--and fill hist if so
      if (i_sipm % 32 == 31) {
        if (debug_temp_diff) std::cout << "Fill hist!" << std::endl;
        for (int i_col = 0; i_col < 4; ++i_col) {
          if (temp_values_IV[i_col] != -1 && temp_values_IV[i_col + 4] != -1) {
            hist_temp_difference_sensor_IV->Fill((temp_values_IV[i_col + 4] - temp_values_IV[i_col]) / temp_sensor_separation_cm);
            hist_temp_difference_sensor_SPS->Fill((temp_values_SPS[i_col + 4] - temp_values_SPS[i_col]) / temp_sensor_separation_cm);
            
            if (debug_temp_diff) {
              std::cout << "Good temp diff found from sensors T[";
              std::cout << t_blu << i_col + 4 << t_def << "] (";
              std::cout << temp_values_IV[i_col + 4] << ") - T[";
              std::cout << t_blu << i_col << t_def << "] (";
              std::cout << temp_values_IV[i_col] << ")" << std::endl;
            }
          }// End of check for good local temp difference
        }// end of cassette temp sensor column loop
        
        // Reset cassette for next loop
        if (cycle_pos == -1) for (int i_sensor = 0; i_sensor < 8; ++i_sensor) temp_values_IV[i_sensor] = -1;
        else break; // break if cycle test--only one test
      }// End of complete cassette check/hist fill
    }// End of SiPM loop
  }// End of batch tray file loop
  
  // Prepare the canvas
  gCanvas_solo->cd();
  gCanvas_solo->Clear();
  gCanvas_solo->SetCanvasSize(600, 500);
  gPad->SetTicks(1,1);
  gPad->SetRightMargin(0.015);
  gPad->SetBottomMargin(0.08);
  gPad->SetLeftMargin(0.09);
  
  // Draw histogram of temperature differential
  hist_temp_difference_sensor_IV->Draw("hist");
  hist_temp_difference_sensor_SPS->Draw("hist same");
  
  // Add histograms of pair difference from cycle test if available
  if (global_flag_find_cycle_temp_gradient) {
//    gData_cycletest_sipm_pair_difference_IV->SetFillStyle(3444);
    gData_cycletest_sipm_pair_difference_IV->Draw("hist same");
//    gData_cycletest_sipm_pair_difference_SPS->SetFillStyle(3844);
    gData_cycletest_sipm_pair_difference_SPS->Draw("hist same");
  }
  
  
  // Draw lines for the average of each distribution
  avg_sipm_pair_difference[0] /= count_sipm_pair_differences;
  avg_sipm_pair_difference[1] /= count_sipm_pair_differences;
  
  TLine* avg_line = new TLine();
  avg_line->SetLineStyle(7);
  avg_line->SetLineWidth(2);
  avg_line->SetLineColor(plot_colors[0]);
  avg_line->DrawLine(avg_sipm_pair_difference[0], 0, 
                     avg_sipm_pair_difference[0], hist_temp_difference_sensor_IV->GetMaximum()*1.05);
  avg_line->SetLineColor(plot_colors[1]);
  avg_line->DrawLine(avg_sipm_pair_difference[1], 0,
                     avg_sipm_pair_difference[1], hist_temp_difference_sensor_IV->GetMaximum()*1.05);
  avg_line->SetLineStyle(5);
  avg_line->SetLineColor(plot_colors_alt[0]);
  avg_line->DrawLine(hist_temp_difference_sensor_IV->GetMean(), 0,
                     hist_temp_difference_sensor_IV->GetMean(), hist_temp_difference_sensor_IV->GetMaximum()*1.05);
  avg_line->SetLineColor(plot_colors_alt[1]);
  avg_line->DrawLine(hist_temp_difference_sensor_SPS->GetMean(), 0,
                     hist_temp_difference_sensor_SPS->GetMean(), hist_temp_difference_sensor_IV->GetMaximum()*1.05);
  
  // Add legend--data
  TLegend* leg_tempgrad = new TLegend(0.15, 0.40, 0.47, 0.62);
  leg_tempgrad->SetLineWidth(0);
  leg_tempgrad->AddEntry(hist_temp_difference_sensor_IV, "Temp Sensors (IV)", "f");
  leg_tempgrad->AddEntry(hist_temp_difference_sensor_SPS, "Temp Sensors (SPS)", "f");
  leg_tempgrad->AddEntry(gData_cycletest_sipm_pair_difference_IV, "Cycled SiPM IV", "f");
  leg_tempgrad->AddEntry(gData_cycletest_sipm_pair_difference_SPS, "Cycled SiPM SPS", "f");
  leg_tempgrad->Draw();
  
  // Draw some informative text about the setup
  TLatex* top_tex[8];
  top_tex[0] = drawText("#bf{Debrecen} SiPM Test Setup @ #bf{Yale}",                  gPad->GetLeftMargin(), 0.91, false, kBlack, 0.04);
  top_tex[1] = drawText("#bf{ePIC} Test Stand",                                       gPad->GetLeftMargin(), 0.955, false, kBlack, 0.045);
  top_tex[2] = drawText(Form("Hamamatsu #bf{%s}", Hamamatsu_SiPM_Code),               1-gPad->GetRightMargin(), 0.95, true, kBlack, 0.045);
  top_tex[3] = drawText("Test Stand Systematics:",                                    gPad->GetLeftMargin() + 0.048, 0.83, false, kBlack, 0.035);
  top_tex[4] = drawText("Cassette Temperature Gradient",                              gPad->GetLeftMargin() + 0.048, 0.78, false, kBlack, 0.035);
  top_tex[5] = drawText("Assuming physical separations:",                             0.67-gPad->GetRightMargin(), 0.83, false, kBlack, 0.025);
  top_tex[6] = drawText(Form("Sensor to Sensor: %0.2f cm", temp_sensor_separation_cm),0.70-gPad->GetRightMargin(), 0.785, false, kBlack, 0.025);
  top_tex[7] = drawText(Form("SiPM to SiPM: %0.2f cm", sipm_cassette_separation_cm),  0.70-gPad->GetRightMargin(), 0.75, false, kBlack, 0.025);
  
  // Store plot
  gCanvas_solo->SaveAs("../plots/systematic_plots/temperature/temp_differential_hist.pdf");
  
  return;
}// End of systematic_analysis_summary::makeTemperatureGradientHist


//========================================================================== Cassette Location Systematics



// Analyse the data from cassette index/cycle scan data
// This systematic test comprises of a set of SiPMs:
//    - varying/cycling through each cassette location
//    - held at roughly constant temperature
//    - held at constant operating voltage
//
// This method assumes that the contiguous string "cycle"
// is in the run notes/batch strings and only includes such data.
void makeCycleScan() {
  bool cycle_debug = false;
  
  
  // *-- Check valid gReader state
  
  // Find strings with cycle
  std::vector<int> cycle_tray_indices;
  std::vector<int> cycle_cassette_indices;
  std::vector<std::string> cycle_tray_strings;
  for (int i_tray = 0; i_tray < gReader->GetTrayStrings()->size(); ++i_tray) {
    int cycle_pos = gReader->GetTrayStrings()->at(i_tray).find("cycle");
    if (cycle_pos != -1) {
      std::string swapstring = gReader->GetTrayStrings()->at(i_tray);
      cycle_tray_indices.push_back(i_tray);
      cycle_cassette_indices.push_back( std::stoi(swapstring.substr(cycle_pos + 6, swapstring.size() )) );
      cycle_tray_strings.push_back(swapstring);
      
      std::cout << "Good cycle scan tray found at index " << t_blu << i_tray << t_def;
      std::cout << " (" << t_grn << gReader->GetTrayStrings()->at(i_tray) << t_def;
      std::cout << ") with cassette base index " << t_red << cycle_cassette_indices.back() << t_def << "." << std::endl;
    }
  }
  
  // Check that vopscan values were found, return if not
  if (cycle_tray_indices.size() == 0) {
    std::cout << "Warning in systematic_analysis_summary::makeOperatingVoltageScan: No trays with \"cycle\" found in dataset." << std::endl;
    std::cout << "Check input batch file to verify cycle scan data are available." << std::endl;
    return;
  }
  
  
  // *-- Initialize data arrays
  
  // SiPM identifer data
  std::vector<int> sipm_row;
  std::vector<int> sipm_col;
  std::vector<std::vector<float> > cassette_index_adjusted; // (float type for TGraph plotting)
  
  // Data
  std::vector<std::vector<float> > Vbr_IV;
  std::vector<std::vector<float> > Vbr_25_IV;
  std::vector<std::vector<float> > Vbr_SPS;
  std::vector<std::vector<float> > Vbr_25_SPS;
  // Error (folded in from other systematics)
  float syst_box_width_to_set = 0.1;
  std::vector<float> syst_box_width;
  std::vector<std::vector<float> > Vbr_IV_err;
  std::vector<std::vector<float> > Vbr_25_IV_err;
  std::vector<std::vector<float> > Vbr_SPS_err;
  std::vector<std::vector<float> > Vbr_25_SPS_err;
  // Temperature and Error
  std::vector<std::vector<float> > temp_IV;
  std::vector<std::vector<float> > temp_SPS;
  std::vector<std::vector<float> > temp_IV_err;
  std::vector<std::vector<float> > temp_SPS_err;
  
  
  // *-- Gather data from gReader
  
  // Gather relevant data from gReader
  for (int i_cycle = 0; i_cycle < cycle_tray_indices.size(); ++i_cycle) {
    if (cycle_debug) std::cout << "current tray :: " << gReader->GetIV()->at(cycle_tray_indices[i_cycle])->tray_note << std::endl;
    
    // Account for adjustment to cassette location for many SiPMs
    
    // Gather relevant data from gReader
    syst_box_width.push_back(syst_box_width_to_set);
    std::vector<int>* current_sipm_row = gReader->GetIV()->at(cycle_tray_indices[i_cycle])->row;
    std::vector<int>* current_sipm_col = gReader->GetIV()->at(cycle_tray_indices[i_cycle])->col;
    
    std::vector<float>* current_Vbr_IV = gReader->GetIV()->at(cycle_tray_indices[i_cycle])->IV_Vpeak;
    std::vector<float>* current_Vbr_25_IV = gReader->GetIV()->at(cycle_tray_indices[i_cycle])->IV_Vpeak_25C;
    std::vector<float>* current_Vbr_SPS = gReader->GetSPS()->at(cycle_tray_indices[i_cycle])->SPS_Vbd;
    std::vector<float>* current_Vbr_25_SPS = gReader->GetSPS()->at(cycle_tray_indices[i_cycle])->SPS_Vbd_25C;
    
    std::vector<float>* current_temp_IV = gReader->GetIV()->at(cycle_tray_indices[i_cycle])->avg_temp;
    std::vector<float>* current_temp_SPS = gReader->GetSPS()->at(cycle_tray_indices[i_cycle])->avg_temp;
    std::vector<float>* current_temp_IV_err = gReader->GetIV()->at(cycle_tray_indices[i_cycle])->stdev_temp;
    std::vector<float>* current_temp_SPS_err = gReader->GetSPS()->at(cycle_tray_indices[i_cycle])->stdev_temp;
    
    // Allocate data from reader to local arrays
    for (int i_sipm = 0; i_sipm < current_Vbr_IV->size(); ++i_sipm) {
      if (current_Vbr_IV->at(i_sipm) == -999) continue;
      if (cycle_debug) std::cout << "IV V_br for SiPM " << i_sipm << " = " << current_Vbr_IV->at(i_sipm) << std::endl;
      
      // Append data so that each vector is one SiPM--for converting to TGraph later
      if (i_cycle == 0) {
        // SiPM indexing
        sipm_row.push_back(current_sipm_row->at(i_sipm));
        sipm_col.push_back(current_sipm_col->at(i_sipm));
        cassette_index_adjusted.push_back(std::vector<float>());
        
        // Cycle scan voltage data
        Vbr_IV.push_back(std::vector<float>());
        Vbr_25_IV.push_back(std::vector<float>());
        Vbr_SPS.push_back(std::vector<float>());
        Vbr_25_SPS.push_back(std::vector<float>());
        
        // Cycle scan voltage error -- currently just avg stdev systematic from reproducibility
        Vbr_IV_err.push_back(std::vector<float>());
        Vbr_25_IV_err.push_back(std::vector<float>());
        Vbr_SPS_err.push_back(std::vector<float>());
        Vbr_25_SPS_err.push_back(std::vector<float>());
        
        // Measured temperature and error
        temp_IV.push_back(std::vector<float>());
        temp_SPS.push_back(std::vector<float>());
        temp_IV_err.push_back(std::vector<float>());
        temp_SPS_err.push_back(std::vector<float>());
        
        if (cycle_debug) std::cout << "push back new SiPM...(" << sipm_row.back() << ',' << sipm_col.back() << ")." << std::endl;
      }
      
      // Adjusted cassette location for multiple SiPMs in test
      // (they can't all be in the same place at the same time after all)
      cassette_index_adjusted[i_sipm].push_back( (cycle_cassette_indices[i_cycle] + i_sipm) % 32
                                                + 32 * (cycle_cassette_indices[i_cycle] >= 32));
      
      // Cycle scan voltage data
      Vbr_IV[i_sipm].push_back(current_Vbr_IV->at(i_sipm));
      Vbr_25_IV[i_sipm].push_back(current_Vbr_25_IV->at(i_sipm) - (0.034 - gTempcorr_IV)*(25-current_temp_IV->at(i_sipm))); // Allow local corrections to temp coef.
      Vbr_SPS[i_sipm].push_back(current_Vbr_SPS->at(i_sipm));
      Vbr_25_SPS[i_sipm].push_back(current_Vbr_25_SPS->at(i_sipm));
      
      // Cycle scan voltage error -- currently just avg stdev systematic from reproducibility
      Vbr_IV_err[i_sipm].push_back(gRepError_IV[0]);     // IV - not temp corrected
      Vbr_25_IV_err[i_sipm].push_back(gRepError_IV[1]);  // IV - temp corrected
      Vbr_SPS_err[i_sipm].push_back(gRepError_SPS[0]);   // SPS - not temp corrected
      Vbr_25_SPS_err[i_sipm].push_back(gRepError_SPS[1]); // SPS - temp corrected
      
      
      // Measured temperature and error
      temp_IV[i_sipm].push_back(current_temp_IV->at(i_sipm));
      temp_SPS[i_sipm].push_back(current_temp_SPS->at(i_sipm));
      temp_IV_err[i_sipm].push_back(current_temp_IV_err->at(i_sipm));
      temp_SPS_err[i_sipm].push_back(current_temp_SPS_err->at(i_sipm));
      
    }// End of SiPM loop
  }// End of Cycle scan file loop
  
  
  // *-- Initialize graph objects
  
  // Make TGraphs of data over the V_op scan
  char data_plot_option[5] = "p 2";
  const int ntotal_scan = Vbr_IV[2].size();
  const int ntotal_sipm = Vbr_IV.size();
  std::cout << "ntotal_scan = " << ntotal_scan << std::endl;
  std::cout << "ntotal_sipm = " << ntotal_sipm << std::endl;
  
  // Data graphs
  TGraphErrors* sipm_cycle_graph_IV[ntotal_sipm];
  TGraphErrors* sipm_cycle_graph_IV_25[ntotal_sipm];
  TGraphErrors* sipm_cycle_graph_SPS[ntotal_sipm];
  TGraphErrors* sipm_cycle_graph_SPS_25[ntotal_sipm];
  TMultiGraph*  sipm_cycle_multigraph[ntotal_sipm];
  
  // Fit functions
  TF1* linfit_IV[ntotal_sipm];
  TF1* linfit_IV_25[ntotal_sipm];
  TF1* linfit_SPS[ntotal_sipm];
  TF1* linfit_SPS_25[ntotal_sipm];
  
  
  // *-- Analysis subroutine: temp gradient
  
  // Gather data for temperature gradient in cycle test
  if (global_flag_find_cycle_temp_gradient) {
    // Given that the middle is already temperature corrected, the line fit won't tell us about the physical temperature gradient,
    // but the residual gradient after correction (due to temperature difference between the sensor and SiPM position along the gradient).
    // Because of this, the best approach to explore the temperature gradient is by comparing SiPMs which are connected to the same sensor, after correction.
    //
    // Then by dividing against the difference in length we should be able to reliably find the physical gradient.
    
    // Construct histograms
    gData_cycletest_sipm_pair_difference_IV = new TH1D("hist_cycletest_sipm_pair_difference_IV",
                                                       ";Temperature Gradient [#circC/cm];Counts",
                                                       nbin_temp_grad,range_temp_grad[0],range_temp_grad[1]);
    gData_cycletest_sipm_pair_difference_IV->SetFillColorAlpha(plot_colors[0], 0.3);
    gData_cycletest_sipm_pair_difference_IV->SetLineColor(plot_colors[0]);
    gData_cycletest_sipm_pair_difference_IV->SetLineWidth(1);
    gData_cycletest_sipm_pair_difference_IV->SetMarkerColor(plot_colors[0]);
    gData_cycletest_sipm_pair_difference_IV->SetMarkerStyle(53);
    gData_cycletest_sipm_pair_difference_IV->SetMarkerSize(1.4);
    
    gData_cycletest_sipm_pair_difference_SPS = new TH1D("hist_cycletest_sipm_pair_difference_SPS",
                                                        ";Temperature Gradient [#circC/cm];Counts",
                                                        nbin_temp_grad,range_temp_grad[0],range_temp_grad[1]);
    gData_cycletest_sipm_pair_difference_SPS->SetFillColorAlpha(plot_colors[1], 0.3);
    gData_cycletest_sipm_pair_difference_SPS->SetLineColor(plot_colors[1]);
    gData_cycletest_sipm_pair_difference_SPS->SetMarkerColor(plot_colors[1]);
    gData_cycletest_sipm_pair_difference_SPS->SetMarkerStyle(54);
    gData_cycletest_sipm_pair_difference_SPS->SetMarkerSize(1.4);
    
    
    gData_cycletest_sipm_pair_difference_IV->Fill(1);
    
    // Gather pairwise temp difference data
    for (int i_sipm = 0; i_sipm < ntotal_sipm; ++i_sipm) {
      for (int i_cassette = 0; i_cassette < 32; i_cassette += 2) {
        float find_IV[2];
        float find_SPS[2];
        
        // Find data corresponding to the desired SiPM index, inefficient but ok.
        for (int i_unsorted = 0; i_unsorted < 32; ++i_unsorted) {
          if (cassette_index_adjusted[i_sipm][i_unsorted] == i_cassette)          {
            find_IV[0] = Vbr_25_IV[i_sipm][i_unsorted];
            find_SPS[0] = Vbr_25_SPS[i_sipm][i_unsorted];
          } else if (cassette_index_adjusted[i_sipm][i_unsorted] == i_cassette + 1) {
            find_IV[1] = Vbr_25_IV[i_sipm][i_unsorted];
            find_SPS[1] = Vbr_25_SPS[i_sipm][i_unsorted];
          }
        }// End of cassette index check loop
        
        // Fill temperature gradient histograms and record data for averaging.
        gData_cycletest_sipm_pair_difference_IV->Fill((find_IV[1] - find_IV[0])/(gTempcorr_IV * sipm_cassette_separation_cm));
        gData_cycletest_sipm_pair_difference_SPS->Fill((find_SPS[1] - find_SPS[0])/(gTempcorr_IV * sipm_cassette_separation_cm));
        avg_sipm_pair_difference[0] += (find_IV[1] - find_IV[0])/(gTempcorr_IV * sipm_cassette_separation_cm);
        avg_sipm_pair_difference[1] += (find_SPS[1] - find_SPS[0])/(gTempcorr_IV * sipm_cassette_separation_cm);
        ++count_sipm_pair_differences;
      }// End of cassette index loop
    }// End of SiPM loop from cycle test
  }// End of temp difference gathering
  
  
  // *-- Plotting
  
  // Plot data and store plots
  for (int i_sipm = 0; i_sipm < ntotal_sipm; ++i_sipm) {
    // *-- Prepare the TGraph objects
    sipm_cycle_multigraph[i_sipm] = new TMultiGraph();
    
    sipm_cycle_graph_IV[i_sipm] = new TGraphErrors(ntotal_scan,
                                                   cassette_index_adjusted[i_sipm].data(),  Vbr_IV[i_sipm].data(),
                                                   syst_box_width.data(),                   Vbr_IV_err[i_sipm].data());
    sipm_cycle_graph_IV[i_sipm]->SetFillColorAlpha(plot_colors[0], 0.5);
    sipm_cycle_graph_IV[i_sipm]->SetLineColor(plot_colors[0]);
    sipm_cycle_graph_IV[i_sipm]->SetLineWidth(2);
    sipm_cycle_graph_IV[i_sipm]->SetMarkerColor(plot_colors[0]);
    sipm_cycle_graph_IV[i_sipm]->SetMarkerStyle(53);
    sipm_cycle_graph_IV[i_sipm]->SetMarkerSize(1.4);
//    sipm_cycle_multigraph[i_sipm]->Add(sipm_cycle_graph_IV[i_sipm], data_plot_option);
    
    sipm_cycle_graph_IV_25[i_sipm] = new TGraphErrors(ntotal_scan,
                                                      cassette_index_adjusted[i_sipm].data(), Vbr_25_IV[i_sipm].data(),
                                                      syst_box_width.data(),                  Vbr_25_IV_err[i_sipm].data());
    sipm_cycle_graph_IV_25[i_sipm]->SetFillColorAlpha(plot_colors_alt[0], 0.5);
    sipm_cycle_graph_IV_25[i_sipm]->SetLineColor(plot_colors_alt[0]);
    sipm_cycle_graph_IV_25[i_sipm]->SetMarkerColor(plot_colors_alt[0]);
    sipm_cycle_graph_IV_25[i_sipm]->SetMarkerStyle(20);
    sipm_cycle_graph_IV_25[i_sipm]->SetMarkerSize(1.4);
    sipm_cycle_multigraph[i_sipm]->Add(sipm_cycle_graph_IV_25[i_sipm], data_plot_option);
    
    sipm_cycle_graph_SPS[i_sipm] = new TGraphErrors(ntotal_scan,
                                                    cassette_index_adjusted[i_sipm].data(),   Vbr_SPS[i_sipm].data(),
                                                    syst_box_width.data(),                    Vbr_SPS_err[i_sipm].data());
    sipm_cycle_graph_SPS[i_sipm]->SetFillColorAlpha(plot_colors[1], 0.5);
    sipm_cycle_graph_SPS[i_sipm]->SetLineColor(plot_colors[1]);
    sipm_cycle_graph_SPS[i_sipm]->SetMarkerColor(plot_colors[1]);
    sipm_cycle_graph_SPS[i_sipm]->SetMarkerStyle(54);
    sipm_cycle_graph_SPS[i_sipm]->SetMarkerSize(1.4);
//    sipm_cycle_multigraph[i_sipm]->Add(sipm_cycle_graph_SPS[i_sipm], data_plot_option);
    
    sipm_cycle_graph_SPS_25[i_sipm] = new TGraphErrors(ntotal_scan,
                                                       cassette_index_adjusted[i_sipm].data(),  Vbr_25_SPS[i_sipm].data(),
                                                       syst_box_width.data(),                   Vbr_25_SPS_err[i_sipm].data());
    sipm_cycle_graph_SPS_25[i_sipm]->SetFillColorAlpha(plot_colors_alt[1], 0.5);
    sipm_cycle_graph_SPS_25[i_sipm]->SetLineColor(plot_colors_alt[1]);
    sipm_cycle_graph_SPS_25[i_sipm]->SetMarkerColor(plot_colors_alt[1]);
    sipm_cycle_graph_SPS_25[i_sipm]->SetMarkerStyle(21);
    sipm_cycle_graph_SPS_25[i_sipm]->SetMarkerSize(1.4);
    sipm_cycle_multigraph[i_sipm]->Add(sipm_cycle_graph_SPS_25[i_sipm], data_plot_option);
    
    
    // *-- Perform fitting to linear map and estimate flatness from error on the slope
    float rangelim[2] = {0, 64};
    float slopelim[2] = {-0.07, 0.07};
    // Usefil ptions for fitting:
    //    Q - Quiet
    //    W - Ignore errors
    //    F - Use TMinuit for poly
    //    R - use rangelim only for fitting
    //    EX0 - ignore x axis errors
    char fitoption[10] = "Q EX0";
    linfit_IV[i_sipm] = new TF1(Form("linfit_IV_%i_%i", sipm_row[i_sipm],sipm_col[i_sipm]),
                                "[0] + [1]*(x-25)", rangelim[0], rangelim[1]);
    linfit_IV[i_sipm]->SetLineColor(plot_colors[0]);
    linfit_IV[i_sipm]->SetLineStyle(7);
    linfit_IV[i_sipm]->SetParLimits(1, slopelim[0], slopelim[1]);
    linfit_IV[i_sipm]->SetParameter(getAvgFromVector(Vbr_IV[i_sipm]), 0); // seed null hypothesis as a guess
    sipm_cycle_graph_IV[i_sipm]->Fit(linfit_IV[i_sipm], fitoption);
    
    linfit_IV_25[i_sipm] = new TF1(Form("linfit_IV_25C_%i_%i", sipm_row[i_sipm],sipm_col[i_sipm]),
                                   "[0] + [1]*(x-25)", rangelim[0], rangelim[1]);
    linfit_IV_25[i_sipm]->SetLineColor(plot_colors_alt[0]);
    linfit_IV_25[i_sipm]->SetLineStyle(5);
    linfit_IV_25[i_sipm]->SetParLimits(1, slopelim[0], slopelim[1]);
    linfit_IV_25[i_sipm]->SetParameters(getAvgFromVector(Vbr_25_IV[i_sipm]), 0); // seed null hypothesis as a guess
    sipm_cycle_graph_IV_25[i_sipm]->Fit(linfit_IV_25[i_sipm], fitoption);
    
    linfit_SPS[i_sipm] = new TF1(Form("linfit_SPS_%i_%i", sipm_row[i_sipm],sipm_col[i_sipm]),
                                 "[0] + [1]*(x-25)", rangelim[0], rangelim[1]);
    linfit_SPS[i_sipm]->SetLineColor(plot_colors[1]);
    linfit_SPS[i_sipm]->SetLineStyle(7);
    linfit_SPS[i_sipm]->SetParLimits(1, slopelim[0], slopelim[1]);
    linfit_SPS[i_sipm]->SetParameters(getAvgFromVector(Vbr_25_IV[i_sipm]), 0); // seed null hypothesis as a guess
    sipm_cycle_graph_SPS[i_sipm]->Fit(linfit_SPS[i_sipm], fitoption);
    
    linfit_SPS_25[i_sipm] = new TF1(Form("linfit_SPS_25C_%i_%i", sipm_row[i_sipm],sipm_col[i_sipm]),
                                    "[0] + [1]*(x-25)", rangelim[0], rangelim[1]);
    linfit_SPS_25[i_sipm]->SetLineColor(plot_colors_alt[1]);
    linfit_SPS_25[i_sipm]->SetLineStyle(5);
    linfit_SPS_25[i_sipm]->SetParLimits(1, slopelim[0], slopelim[1]);
    linfit_SPS_25[i_sipm]->SetParameters(getAvgFromVector(Vbr_25_SPS[i_sipm]), 0); // seed null hypothesis as a guess
    sipm_cycle_graph_SPS_25[i_sipm]->Fit(linfit_SPS_25[i_sipm], fitoption);
    
    // Prepare the canvas
    gCanvas_solo->cd();
    gCanvas_solo->Clear();
    gCanvas_solo->SetCanvasSize(1000, 500);
    gPad->SetTicks(1,1);
    gPad->SetRightMargin(0.015);
    gPad->SetBottomMargin(0.08);
    gPad->SetLeftMargin(0.09);
    
    // Plot the graphs--base layer
    sipm_cycle_multigraph[i_sipm]->SetTitle(";Test Cassette Index;Measured V_{br} [V]");
    sipm_cycle_multigraph[i_sipm]->GetYaxis()->SetRangeUser(voltplot_limits[0],voltplot_limits[1]);
    sipm_cycle_multigraph[i_sipm]->GetYaxis()->SetTitleOffset(1.2);
    sipm_cycle_multigraph[i_sipm]->GetXaxis()->SetTitleOffset(1.0);
    sipm_cycle_multigraph[i_sipm]->Draw("a");
    
    // Add reference lines
    TLine* typ_line = new TLine();
    typ_line->SetLineStyle(8);
    typ_line->SetLineColor(kGray+1);
    typ_line->DrawLine(31.5, voltplot_limits[0], 31.5, voltplot_limits[1]);
    
    // Add legend--data
    double ypush = -0.1;
    TLegend* leg_data = new TLegend(0.15, 0.46+ypush, 0.45, 0.57+ypush);
    leg_data->SetLineWidth(0);
//    leg_data->AddEntry(sipm_cycle_graph_IV[i_sipm], "IV (ambient)", data_plot_option);
    leg_data->AddEntry(sipm_cycle_graph_IV_25[i_sipm], "IV (25#circC)", data_plot_option);
//    leg_data->AddEntry(sipm_cycle_graph_SPS[i_sipm], "SPS (ambient)", data_plot_option);
    leg_data->AddEntry(sipm_cycle_graph_SPS_25[i_sipm], "SPS (25#circC)", data_plot_option);
    leg_data->Draw();
    
    // Add legend--fitting
    TLegend* leg_fit = new TLegend(0.55, 0.485+ypush, 0.95, 0.58+ypush);
    leg_fit->SetLineWidth(0);
    leg_fit->SetTextSize(0.035);
//    leg_fit->AddEntry(linfit_IV[i_sipm], Form("%.3f #pm %.3f",
//                                              1000*linfit_IV[i_sipm]->GetParameter(1),
//                                              1000*linfit_IV[i_sipm]->GetParError(1)), "l");
    leg_fit->AddEntry(linfit_IV_25[i_sipm], Form("%.3f #pm %.3f",
                                                 1000*linfit_IV_25[i_sipm]->GetParameter(1),
                                                 1000*linfit_IV_25[i_sipm]->GetParError(1)), "l");
//    leg_fit->AddEntry(linfit_SPS[i_sipm], Form("%.3f #pm %.3f",
//                                               1000*linfit_SPS[i_sipm]->GetParameter(1),
//                                               1000*linfit_SPS[i_sipm]->GetParError(1)), "l");
    leg_fit->AddEntry(linfit_SPS_25[i_sipm], Form("%.3f #pm %.3f",
                                                  1000*linfit_SPS_25[i_sipm]->GetParameter(1),
                                                  1000*linfit_SPS_25[i_sipm]->GetParError(1)), "l");
    leg_fit->Draw();
    
    // Draw the chi2 separately to align them horizontally
    TLatex* tex_chi2[4];
    double base = 0.545+ypush;
    double diff = 0.047;
//    tex_chi2[0] = drawText(Form("#chi^{2} = %.3f",linfit_IV[i_sipm]->GetChisquare()),       0.775, base - 0*diff, false, kBlack, 0.035);
    tex_chi2[1] = drawText(Form("#chi^{2}/NDF = %.3f",
                                linfit_IV_25[i_sipm]->GetChisquare() /
                                linfit_IV_25[i_sipm]->GetNDF()),          0.775, base - 0*diff, false, kBlack, 0.035);
//    tex_chi2[2] = drawText(Form("#chi^{2} = %.3f",linfit_SPS[i_sipm]->GetChisquare()),      0.775, base - 1*diff, false, kBlack, 0.035);
    tex_chi2[3] = drawText(Form("#chi^{2}/NDF = %.3f",
                                linfit_SPS_25[i_sipm]->GetChisquare() /
                                linfit_SPS_25[i_sipm]->GetNDF()),         0.775, base - 1*diff, false, kBlack, 0.035);
    
    // Draw some text about the fitting
    TLatex* tex_fit[3];
    tex_fit[0] = drawText("Fit Slope [mV/index]", 0.55, 0.6+ypush, false, kBlack, 0.04);
//    tex_fit[2] = drawText("Hamamatsu Nominal: #bf{34 mV/#circC}", 0.55, 0.35, false, kBlack, 0.035);
    
    // Draw some informative text about the setup
    TLatex* top_tex[6];
    top_tex[0] = drawText("#bf{Debrecen} SiPM Test Setup @ #bf{Yale}",                 gPad->GetLeftMargin(), 0.91, false, kBlack, 0.04);
    top_tex[1] = drawText("#bf{ePIC} Test Stand",                                      gPad->GetLeftMargin(), 0.955, false, kBlack, 0.045);
    top_tex[2] = drawText(Form("Hamamatsu #bf{%s}", Hamamatsu_SiPM_Code),              1-gPad->GetRightMargin(), 0.95, true, kBlack, 0.045);
    top_tex[3] = drawText(Form("Tray %s SiPM (%i,%i)",
                               gReader->GetIV()->at(cycle_tray_indices[0])->tray_note.substr(0,11).c_str(),
                               sipm_row[i_sipm],sipm_col[i_sipm]),                     1-gPad->GetRightMargin(), 0.91, true, kBlack, 0.035);
    top_tex[4] = drawText("Test Stand Systematics: Cassette Cycle Test",               gPad->GetLeftMargin() + 0.05, 0.83, false, kBlack, 0.04);
    top_tex[5] = drawText(Form("%i Total Tests During Cooldown",ntotal_scan),          gPad->GetLeftMargin() + 0.05, 0.78, false, kBlack, 0.04);
    
    // Might also be interesting to look at:
    // - Residuals against fit for closer look at potential nonlinearity
    // - Dist of fit coefficients + error relative to Hamamatsu nominal. Do we see consistent IV/SPS behavior?
    
    gCanvas_solo->SaveAs(Form("../plots/systematic_plots/cassette_index/cycle_%i_%i_Vbr.pdf",
                              sipm_row[i_sipm],sipm_col[i_sipm]));
  }// End of SiPM plot construction loop
  return;
}

//========================================================================== Operating Voltage V_op Systematics



// Analyse the data from operating voltage V_op scan data
// This systematic test comprises of a set of SiPMs:
//    - in one cassette location
//    - held at roughly constant temperature
//    - varying the test operating voltage
//
// This method assumes that the contiguous string "vopscan"
// is in the run notes/batch strings and only includes such data.
void makeOperatingVoltageScan() {
  bool debug_vop = false;
  
  // Find strings with vopscan
  std::vector<int> vop_tray_indices;
  std::vector<float> vop_tray_voltage;
  std::vector<std::string> vop_tray_strings;
  for (int i_tray = 0; i_tray < gReader->GetTrayStrings()->size(); ++i_tray) {
    if (gReader->GetTrayStrings()->at(i_tray).find("vopscan") != -1) {
      std::string swapstring = gReader->GetTrayStrings()->at(i_tray);
      vop_tray_indices.push_back(i_tray);
      vop_tray_strings.push_back(swapstring);
      vop_tray_voltage.push_back(42. + 0.01 * std::stoi(swapstring.substr(swapstring.size() - 2, swapstring.size())) );
      
      std::cout << "Good v_op scan tray found at index " << t_blu << i_tray << t_def;
      std::cout << " (" << t_grn << gReader->GetTrayStrings()->at(i_tray) << t_def;
      std::cout << ") with operating voltage " << t_red << vop_tray_voltage.back() << t_def << "." << std::endl;
    }
  }
  
  // Check that vopscan values were found, return if not
  if (vop_tray_indices.size() == 0) {
    std::cout << "Warning in systematic_analysis_summary::makeOperatingVoltageScan: No trays with \"vopscan\" found in dataset." << std::endl;
    std::cout << "Check input batch file to verify V_op scan data are available." << std::endl;
    return;
  }
  
  // Gather relevant data from gReader
  std::vector<int> sipm_row;
  std::vector<int> sipm_col;
  
  // Data
  std::vector<std::vector<float> > Vbr_IV;
  std::vector<std::vector<float> > Vbr_25_IV;
  std::vector<std::vector<float> > Vbr_SPS;
  std::vector<std::vector<float> > Vbr_25_SPS;
  // Error (folded in from other systematics)
  float syst_box_width_to_set = 0.01;
  std::vector<float> syst_box_width;
  std::vector<std::vector<float> > Vbr_IV_err;
  std::vector<std::vector<float> > Vbr_25_IV_err;
  std::vector<std::vector<float> > Vbr_SPS_err;
  std::vector<std::vector<float> > Vbr_25_SPS_err;
  for (int i_vop = 0; i_vop < vop_tray_indices.size(); ++i_vop) {
    if (debug_vop) std::cout << "current tray :: " << gReader->GetIV()->at(vop_tray_indices[i_vop])->tray_note << std::endl;
    
    // Gather relevant data from gReader
    syst_box_width.push_back(syst_box_width_to_set);
    std::vector<int>* current_sipm_row = gReader->GetIV()->at(vop_tray_indices[i_vop])->row;
    std::vector<int>* current_sipm_col = gReader->GetIV()->at(vop_tray_indices[i_vop])->col;
    std::vector<float>* current_Vbr_IV = gReader->GetIV()->at(vop_tray_indices[i_vop])->IV_Vpeak;
    std::vector<float>* current_Vbr_25_IV = gReader->GetIV()->at(vop_tray_indices[i_vop])->IV_Vpeak_25C;
    std::vector<float>* current_Vbr_SPS = gReader->GetSPS()->at(vop_tray_indices[i_vop])->SPS_Vbd;
    std::vector<float>* current_Vbr_25_SPS = gReader->GetSPS()->at(vop_tray_indices[i_vop])->SPS_Vbd_25C;
    
    // Allocate data from reader to local arrays
    for (int i_sipm = 0; i_sipm < current_Vbr_IV->size(); ++i_sipm) {
      if (current_Vbr_IV->at(i_sipm) == -999) continue;
      if (debug_vop) std::cout << "IV V_br for SiPM " << i_sipm << " = " << current_Vbr_IV->at(i_sipm) << std::endl;
      
      // Append data so that each vector is one SiPM--for converting to TGraph later
      if (i_vop == 0) {
        // SiPM indexing
        sipm_row.push_back(current_sipm_row->at(i_sipm));
        sipm_col.push_back(current_sipm_col->at(i_sipm));
        
        // V_op scan data
        Vbr_IV.push_back(std::vector<float>());
        Vbr_25_IV.push_back(std::vector<float>());
        Vbr_SPS.push_back(std::vector<float>());
        Vbr_25_SPS.push_back(std::vector<float>());
        
        // V_op scan error -- currently just avg stdev from reproducibility
        
        Vbr_IV_err.push_back(std::vector<float>());
        Vbr_25_IV_err.push_back(std::vector<float>());
        Vbr_SPS_err.push_back(std::vector<float>());
        Vbr_25_SPS_err.push_back(std::vector<float>());
        if (debug_vop) std::cout << "push back new SiPM...(" << sipm_row.back() << ',' << sipm_col.back() << ")." << std::endl;
      }
      
      // V_op scan data
      Vbr_IV[i_sipm].push_back(current_Vbr_IV->at(i_sipm));
      Vbr_25_IV[i_sipm].push_back(current_Vbr_25_IV->at(i_sipm));
      Vbr_SPS[i_sipm].push_back(current_Vbr_SPS->at(i_sipm));
      Vbr_25_SPS[i_sipm].push_back(current_Vbr_25_SPS->at(i_sipm));
      
      // V_op scan error -- currently just avg stdev from reproducibility
      Vbr_IV_err[i_sipm].push_back(gRepError_IV[0]);     // IV - not temp corrected
      Vbr_25_IV_err[i_sipm].push_back(gRepError_IV[1]);  // IV - temp corrected
      Vbr_SPS_err[i_sipm].push_back(gRepError_SPS[0]);   // SPS - not temp corrected
      Vbr_25_SPS_err[i_sipm].push_back(gRepError_SPS[1]); // SPS - temp corrected
    }// End of SiPM loop
  }// End of V_op scan file loop
  
  
  // Make TGraphs of data over the V_op scan
  char data_plot_option[5] = "p 2";
  const int ntotal_scan = Vbr_IV[2].size();
  const int ntotal_sipm = Vbr_IV.size();
  std::cout << "ntotal_scan = " << ntotal_scan << std::endl;
  std::cout << "ntotal_sipm = " << ntotal_sipm << std::endl;
  
  // Data graphs
  TGraphErrors* sipm_vopscan_graph_IV[ntotal_sipm];
  TGraphErrors* sipm_vopscan_graph_IV_25[ntotal_sipm];
  TGraphErrors* sipm_vopscan_graph_SPS[ntotal_sipm];
  TGraphErrors* sipm_vopscan_graph_SPS_25[ntotal_sipm];
  TMultiGraph* sipm_vopscan_multigraph[ntotal_sipm];
  
  // Fit
  TF1* linfit_IV[ntotal_sipm];
  TF1* linfit_IV_25[ntotal_sipm];
  TF1* linfit_SPS[ntotal_sipm];
  TF1* linfit_SPS_25[ntotal_sipm];
  
  // Plot data and store plots
  for (int i_sipm = 0; i_sipm < ntotal_sipm; ++i_sipm) {
    // *-- Prepare the TGraph objects
    sipm_vopscan_multigraph[i_sipm] = new TMultiGraph();
    
    sipm_vopscan_graph_IV[i_sipm] = new TGraphErrors(ntotal_scan, 
                                                     vop_tray_voltage.data(), Vbr_IV[i_sipm].data(),
                                                     syst_box_width.data(), Vbr_IV_err[i_sipm].data());
    sipm_vopscan_graph_IV[i_sipm]->SetFillColorAlpha(plot_colors[0], 0.5);
    sipm_vopscan_graph_IV[i_sipm]->SetLineColor(plot_colors[0]);
    sipm_vopscan_graph_IV[i_sipm]->SetLineWidth(2);
    sipm_vopscan_graph_IV[i_sipm]->SetMarkerColor(plot_colors[0]);
    sipm_vopscan_graph_IV[i_sipm]->SetMarkerStyle(53);
    sipm_vopscan_graph_IV[i_sipm]->SetMarkerSize(1.7);
    sipm_vopscan_multigraph[i_sipm]->Add(sipm_vopscan_graph_IV[i_sipm], data_plot_option);
    
    sipm_vopscan_graph_IV_25[i_sipm] = new TGraphErrors(ntotal_scan, 
                                                        vop_tray_voltage.data(), Vbr_25_IV[i_sipm].data(),
                                                        syst_box_width.data(), Vbr_25_IV_err[i_sipm].data());
    sipm_vopscan_graph_IV_25[i_sipm]->SetFillColorAlpha(plot_colors_alt[0], 0.5);
    sipm_vopscan_graph_IV_25[i_sipm]->SetLineColor(plot_colors_alt[0]);
    sipm_vopscan_graph_IV_25[i_sipm]->SetMarkerColor(plot_colors_alt[0]);
    sipm_vopscan_graph_IV_25[i_sipm]->SetMarkerStyle(20);
    sipm_vopscan_graph_IV_25[i_sipm]->SetMarkerSize(1.7);
    sipm_vopscan_multigraph[i_sipm]->Add(sipm_vopscan_graph_IV_25[i_sipm], data_plot_option);
    
    sipm_vopscan_graph_SPS[i_sipm] = new TGraphErrors(ntotal_scan, 
                                                      vop_tray_voltage.data(), Vbr_SPS[i_sipm].data(),
                                                      syst_box_width.data(), Vbr_SPS_err[i_sipm].data());
    sipm_vopscan_graph_SPS[i_sipm]->SetFillColorAlpha(plot_colors[1], 0.5);
    sipm_vopscan_graph_SPS[i_sipm]->SetLineColor(plot_colors[1]);
    sipm_vopscan_graph_SPS[i_sipm]->SetMarkerColor(plot_colors[1]);
    sipm_vopscan_graph_SPS[i_sipm]->SetMarkerStyle(54);
    sipm_vopscan_graph_SPS[i_sipm]->SetMarkerSize(1.7);
    sipm_vopscan_multigraph[i_sipm]->Add(sipm_vopscan_graph_SPS[i_sipm], data_plot_option);
    
    sipm_vopscan_graph_SPS_25[i_sipm] = new TGraphErrors(ntotal_scan, 
                                                         vop_tray_voltage.data(), Vbr_25_SPS[i_sipm].data(),
                                                         syst_box_width.data(), Vbr_25_SPS_err[i_sipm].data());
    sipm_vopscan_graph_SPS_25[i_sipm]->SetFillColorAlpha(plot_colors_alt[1], 0.5);
    sipm_vopscan_graph_SPS_25[i_sipm]->SetLineColor(plot_colors_alt[1]);
    sipm_vopscan_graph_SPS_25[i_sipm]->SetMarkerColor(plot_colors_alt[1]);
    sipm_vopscan_graph_SPS_25[i_sipm]->SetMarkerStyle(21);
    sipm_vopscan_graph_SPS_25[i_sipm]->SetMarkerSize(1.7);
    sipm_vopscan_multigraph[i_sipm]->Add(sipm_vopscan_graph_SPS_25[i_sipm], data_plot_option);
    
    
    
    // *-- Perform fitting to linear map and estimate flatness from error on the slope
    float rangelim[2] = {0, 50};
    float slopelim[2] = {-0.5, 0.5};
    // Usefil ptions for fitting:
    //    Q - Quiet
    //    W - Ignore errors
    //    F - Use TMinuit for poly
    //    R - use rangelim only for fitting
    //    EX0 - ignore x axis errors
    char fitoption[10] = "EX0 Q";
    linfit_IV[i_sipm] = new TF1(Form("linfit_IV_%i_%i", sipm_row[i_sipm],sipm_col[i_sipm]),
                                "[0] + [1]*(x-42.4)", rangelim[0], rangelim[1]);
    linfit_IV[i_sipm]->SetLineColor(plot_colors[0]);
    linfit_IV[i_sipm]->SetLineStyle(7);
    linfit_IV[i_sipm]->SetParLimits(1, slopelim[0], slopelim[1]);
    linfit_IV[i_sipm]->SetParameter(getAvgFromVector(Vbr_IV[i_sipm]), 0.021); // seed null hypothesis as a guess
    sipm_vopscan_graph_IV[i_sipm]->Fit(linfit_IV[i_sipm], fitoption);
    
    linfit_IV_25[i_sipm] = new TF1(Form("linfit_IV_25C_%i_%i", sipm_row[i_sipm],sipm_col[i_sipm]),
                                   "[0] + [1]*(x-42.4)", rangelim[0], rangelim[1]);
    linfit_IV_25[i_sipm]->SetLineColor(plot_colors_alt[0]);
    linfit_IV_25[i_sipm]->SetLineStyle(5);
    linfit_IV_25[i_sipm]->SetParLimits(1, slopelim[0], slopelim[1]);
    linfit_IV_25[i_sipm]->SetParameters(getAvgFromVector(Vbr_25_IV[i_sipm]), 0); // seed null hypothesis as a guess
    sipm_vopscan_graph_IV_25[i_sipm]->Fit(linfit_IV_25[i_sipm], fitoption);
    
    linfit_SPS[i_sipm] = new TF1(Form("linfit_SPS_%i_%i", sipm_row[i_sipm],sipm_col[i_sipm]),
                                 "[0] + [1]*(x-42.4)", rangelim[0], rangelim[1]);
    linfit_SPS[i_sipm]->SetLineColor(plot_colors[1]);
    linfit_SPS[i_sipm]->SetLineStyle(7);
    linfit_SPS[i_sipm]->SetParLimits(1, slopelim[0], slopelim[1]);
    linfit_SPS[i_sipm]->SetParameters(getAvgFromVector(Vbr_SPS[i_sipm]), 0); // seed null hypothesis as a guess
    sipm_vopscan_graph_SPS[i_sipm]->Fit(linfit_SPS[i_sipm], fitoption);
    
    linfit_SPS_25[i_sipm] = new TF1(Form("linfit_SPS_25C_%i_%i", sipm_row[i_sipm],sipm_col[i_sipm]),
                                    "[0] + [1]*(x-42.4)", rangelim[0], rangelim[1]);
    linfit_SPS_25[i_sipm]->SetLineColor(plot_colors_alt[1]);
    linfit_SPS_25[i_sipm]->SetLineStyle(5);
    linfit_SPS_25[i_sipm]->SetParLimits(1, slopelim[0], slopelim[1]);
    linfit_SPS_25[i_sipm]->SetParameters(getAvgFromVector(Vbr_25_SPS[i_sipm]), 0); // seed null hypothesis as a guess
    sipm_vopscan_graph_SPS_25[i_sipm]->Fit(linfit_SPS_25[i_sipm], fitoption);
    
    // Prepare the canvas
    gCanvas_solo->cd();
    gCanvas_solo->Clear();
    gPad->SetTicks(1,1);
    gPad->SetRightMargin(0.015);
    gPad->SetLeftMargin(0.09);
    
    // Plot the graphs--base layer
    sipm_vopscan_multigraph[i_sipm]->SetTitle(";Operating Voltage V_{op} [V];Measured V_{br} [V]");
    sipm_vopscan_multigraph[i_sipm]->GetYaxis()->SetRangeUser(voltplot_limits[0],voltplot_limits[1]);
    sipm_vopscan_multigraph[i_sipm]->GetYaxis()->SetTitleOffset(1.2);
    sipm_vopscan_multigraph[i_sipm]->GetXaxis()->SetTitleOffset(1.2);
    sipm_vopscan_multigraph[i_sipm]->Draw("a");
    
    // Add reference lines
    TLine* typ_line = new TLine();
    typ_line->SetLineStyle(8);
    typ_line->SetLineColor(kGray+1);
    typ_line->DrawLine(42.4, voltplot_limits[0], 42.4, voltplot_limits[1]);
    
    // Add legends
    TLegend* leg_data = new TLegend(0.15, 0.39, 0.45, 0.61);
    leg_data->SetLineWidth(0);
    leg_data->AddEntry(sipm_vopscan_graph_IV[i_sipm], "IV (ambient)", data_plot_option);
    leg_data->AddEntry(sipm_vopscan_graph_IV_25[i_sipm], "IV (25#circC)", data_plot_option);
    leg_data->AddEntry(sipm_vopscan_graph_SPS[i_sipm], "SPS (ambient)", data_plot_option);
    leg_data->AddEntry(sipm_vopscan_graph_SPS_25[i_sipm], "SPS (25#circC)", data_plot_option);
    leg_data->Draw();
    
    // Add legend--fitting
    TLegend* leg_fit = new TLegend(0.58, 0.39, 0.95, 0.58);
    leg_fit->SetLineWidth(0);
    leg_fit->SetTextSize(0.035);
    leg_fit->AddEntry(linfit_IV[i_sipm], Form("%.1f #pm %.1f",
                                              1000*linfit_IV[i_sipm]->GetParameter(1),
                                              1000*linfit_IV[i_sipm]->GetParError(1)), "l");
    leg_fit->AddEntry(linfit_IV_25[i_sipm], Form("%.1f #pm %.1f",
                                                 1000*linfit_IV_25[i_sipm]->GetParameter(1),
                                                 1000*linfit_IV_25[i_sipm]->GetParError(1)), "l");
    leg_fit->AddEntry(linfit_SPS[i_sipm], Form("%.1f #pm %.1f",
                                               1000*linfit_SPS[i_sipm]->GetParameter(1),
                                               1000*linfit_SPS[i_sipm]->GetParError(1)), "l");
    leg_fit->AddEntry(linfit_SPS_25[i_sipm], Form("%.1f #pm %.1f",
                                                  1000*linfit_SPS_25[i_sipm]->GetParameter(1),
                                                  1000*linfit_SPS_25[i_sipm]->GetParError(1)), "l");
    leg_fit->Draw();
    
    // Draw the chi2 separately to align them horizontally
    TLatex* tex_chi2[4];
    double base = 0.545;
    double diff = 0.047;
    tex_chi2[0] = drawText(Form("#chi^{2} = %.3f",linfit_IV[i_sipm]->GetChisquare()),       0.83, base - 0*diff, false, kBlack, 0.035);
    tex_chi2[1] = drawText(Form("#chi^{2} = %.3f",linfit_IV_25[i_sipm]->GetChisquare()),    0.83, base - 1*diff, false, kBlack, 0.035);
    tex_chi2[2] = drawText(Form("#chi^{2} = %.3f",linfit_SPS[i_sipm]->GetChisquare()),      0.83, base - 2*diff, false, kBlack, 0.035);
    tex_chi2[3] = drawText(Form("#chi^{2} = %.3f",linfit_SPS_25[i_sipm]->GetChisquare()),   0.83, base - 3*diff, false, kBlack, 0.035);
    
    // Draw some text about the fitting
    TLatex* tex_fit[3];
    tex_fit[0] = drawText("Fit Slope [mV/V]", 0.58, 0.6, false, kBlack, 0.04);
//    tex_fit[2] = drawText("Hamamatsu Nominal: #bf{34 mV/#circC}", 0.55, 0.35, false, kBlack, 0.035);
    
    // Draw some informative text about the setup
    TLatex* top_tex[6];
    top_tex[0] = drawText("#bf{Debrecen} SiPM Test Setup @ #bf{Yale}",                 gPad->GetLeftMargin(), 0.91, false, kBlack, 0.04);
    top_tex[1] = drawText("#bf{ePIC} Test Stand",                                      gPad->GetLeftMargin(), 0.955, false, kBlack, 0.045);
    top_tex[2] = drawText(Form("Hamamatsu #bf{%s}", Hamamatsu_SiPM_Code),              1-gPad->GetRightMargin(), 0.95, true, kBlack, 0.045);
    top_tex[3] = drawText(Form("Tray %s SiPM (%i,%i)",
                               gReader->GetIV()->at(vop_tray_indices[0])->tray_note.substr(0,11).c_str(),
                               sipm_row[i_sipm],sipm_col[i_sipm]),                     1-gPad->GetRightMargin(), 0.91, true, kBlack, 0.035);
    top_tex[4] = drawText("Test Stand Systematics: V_{op}",                            gPad->GetLeftMargin() + 0.05, 0.83, false, kBlack, 0.04);
    
    // Redraw the graph::assure plots sit on top of all other features
//    sipm_vopscan_multigraph[i_sipm]->Draw("a same");
    
    gCanvas_solo->SaveAs(Form("../plots/systematic_plots/operating_voltage/vopscan_%i_%i_Vbr.pdf",
                               sipm_row[i_sipm],sipm_col[i_sipm]));
  }// End of SiPM plot construction loop
  return;
}// End of systematic_analysis_summary::makeOperatingVoltageScan


//========================================================================== Surface Imperfection Systematics


// Make a plot of surface imperfection area vs measured deviation from tray average
// This allows us to explore whether surface imperfections in the SiPMs are
// correlated with performance in V_br or other tests
void makeSurfaceImperfectionCorrelation() {
  std::cout << "Beginning surface imperfection correlation..." << std::endl;
  bool debug_surface_imperfection_correlation = false;
  
  // TODO should be renormalized to tray average deviation?
  
  // Set up plotting canvas
  gCanvas_surfacecorr->cd();
  gCanvas_surfacecorr->Clear();
  gCanvas_surfacecorr->SetCanvasSize(800, 600);
  surfacecorr_pads = divideFlush(gPad, 3, 2, 0.06, 0.06, 0.1, 0.1);
  
  // Set up custom binning for the histogram
  const int nbins_x = 9;
  double hist_range_x[2] = {0.05, 500};
  double binedge_surface_area[nbins_x + 1] = {
    0.05, 1, 7, 13, 23, 47, 69, 89, 101, 500
  };
  const int nbins_y = 12;
  double hist_range_y[2] = {-0.06, 0.06};
  double binedge_deviation[nbins_y + 1];
  for (int i = 0; i <= nbins_y; ++i) binedge_deviation[i] = hist_range_y[0] + i*(hist_range_y[1] - hist_range_y[0])/nbins_y;
  
  // Set up histograms for each type of surface defect: scratch, bubble, debris
  TH2D* hist_surface_corr_IV_scratch = new TH2D("hist_surface_corr_IV_scratch",
                                                ";Obstructed SiPM Pixels;Deviation from tray average V_{IV br} - V_{IV br}^{Tray Avg} [V];Count of SiPMs",
                                                nbins_x, binedge_surface_area,
                                                nbins_y, binedge_deviation);
  TH2D* hist_surface_corr_IV_bubble = new TH2D("hist_surface_corr_IV_bubble",
                                               ";Obstructed SiPM Pixels;Deviation from tray average V_{IV br} - V_{IV br}^{Tray Avg} [V];Count of SiPMs",
                                               nbins_x, binedge_surface_area,
                                               nbins_y, binedge_deviation);
  TH2D* hist_surface_corr_IV_debris = new TH2D("hist_surface_corr_IV_debris",
                                               ";Obstructed SiPM Pixels;Deviation from tray average V_{IV br} - V_{IV br}^{Tray Avg} [V];Count of SiPMs",
                                               nbins_x, binedge_surface_area,
                                               nbins_y, binedge_deviation);
  TH2D* hist_surface_corr_PS_scratch = new TH2D("hist_surface_corr_PS_scratch",
                                                ";Obstructed SiPM Pixels;Deviation from tray average V_{SPS br} - V_{SPS br}^{Tray Avg} [V];Count of SiPMs",
                                                nbins_x, binedge_surface_area,
                                                nbins_y, binedge_deviation);
  TH2D* hist_surface_corr_PS_bubble = new TH2D("hist_surface_corr_PS_bubble",
                                               ";Obstructed SiPM Pixels;Deviation from tray average V_{SPS br} - V_{SPS br}^{Tray Avg} [V];Count of SiPMs",
                                               nbins_x, binedge_surface_area,
                                               nbins_y, binedge_deviation);
  TH2D* hist_surface_corr_PS_debris = new TH2D("hist_surface_corr_PS_debris",
                                               ";Obstructed SiPM Pixels;Deviation from tray average V_{SPS br} - V_{SPS br}^{Tray Avg} [V];Count of SiPMs",
                                               nbins_x, binedge_surface_area,
                                               nbins_y, binedge_deviation);
  
  // 1D-projections onto the y-axis
  TH1D* hist_surface_proj_IV_scratch = new TH1D("hist_surface_proj_IV_scratch",
                                                ";Deviation from tray average V_{IV br} - V_{IV br}^{Tray Avg} [V];dN^{SiPM}/d#DeltaV",
                                                nbins_y, binedge_deviation);
  TH1D* hist_surface_proj_IV_bubble = new TH1D("hist_surface_proj_IV_bubble",
                                               ";Deviation from tray average V_{IV br} - V_{IV br}^{Tray Avg} [V];dN^{SiPM}/d#DeltaV",
                                               nbins_y, binedge_deviation);
  TH1D* hist_surface_proj_IV_debris = new TH1D("hist_surface_proj_IV_debris",
                                               ";Deviation from tray average V_{IV br} - V_{IV br}^{Tray Avg} [V];dN^{SiPM}/d#DeltaV",
                                               nbins_y, binedge_deviation);
  TH1D* hist_surface_proj_PS_scratch = new TH1D("hist_surface_proj_PS_scratch",
                                                ";Deviation from tray average V_{SPS br} - V_{SPS br}^{Tray Avg} [V];dN^{SiPM}/d#DeltaV",
                                                nbins_y, binedge_deviation);
  TH1D* hist_surface_proj_PS_bubble = new TH1D("hist_surface_proj_PS_bubble",
                                               ";Deviation from tray average V_{SPS br} - V_{SPS br}^{Tray Avg} [V];dN^{SiPM}/d#DeltaV",
                                               nbins_y, binedge_deviation);
  TH1D* hist_surface_proj_PS_debris = new TH1D("hist_surface_proj_PS_debris",
                                               ";Deviation from tray average V_{SPS br} - V_{SPS br}^{Tray Avg} [V];dN^{SiPM}/d#DeltaV",
                                               nbins_y, binedge_deviation);
  
  // 1D-projections onto the y-axis for control group: scratched but not on the surface
  TH1D* hist_surface_cont_IV_scratch = new TH1D("hist_surface_cont_IV_scratch",
                                                ";Deviation from tray average V_{IV br} - V_{IV br}^{Tray Avg} [V];dN^{SiPM}/d#DeltaV",
                                                nbins_y, binedge_deviation);
  TH1D* hist_surface_cont_IV_bubble = new TH1D("hist_surface_cont_IV_bubble",
                                               ";Deviation from tray average V_{IV br} - V_{IV br}^{Tray Avg} [V];dN^{SiPM}/d#DeltaV",
                                               nbins_y, binedge_deviation);
  TH1D* hist_surface_cont_IV_debris = new TH1D("hist_surface_cont_IV_debris",
                                               ";Deviation from tray average V_{IV br} - V_{IV br}^{Tray Avg} [V];dN^{SiPM}/d#DeltaV",
                                               nbins_y, binedge_deviation);
  TH1D* hist_surface_cont_PS_scratch = new TH1D("hist_surface_cont_PS_scratch",
                                                ";Deviation from tray average V_{SPS br} - V_{SPS br}^{Tray Avg} [V];dN^{SiPM}/d#DeltaV",
                                                nbins_y, binedge_deviation);
  TH1D* hist_surface_cont_PS_bubble = new TH1D("hist_surface_cont_PS_bubble",
                                               ";Deviation from tray average V_{SPS br} - V_{SPS br}^{Tray Avg} [V];dN^{SiPM}/d#DeltaV",
                                               nbins_y, binedge_deviation);
  TH1D* hist_surface_cont_PS_debris = new TH1D("hist_surface_cont_PS_debris",
                                               ";Deviation from tray average V_{SPS br} - V_{SPS br}^{Tray Avg} [V];dN^{SiPM}/d#DeltaV",
                                               nbins_y, binedge_deviation);
  
  // TH1D for nob-obstructed as extra control group
  TH1D* hist_nonobstructed_IV = new TH1D("hist_nonobstructed_IV",
                                         ";Deviation from tray average V_{IV br} - V_{IV br}^{Tray Avg} [V];dN^{SiPM}/d#DeltaV",
                                         nbins_y, binedge_deviation);
  TH1D* hist_nonobstructed_PS = new TH1D("hist_nonobstructed_PS",
                                         ";Deviation from tray average V_{SPS br} - V_{SPS br}^{Tray Avg} [V];dN^{SiPM}/d#DeltaV",
                                         nbins_y, binedge_deviation);
  
  // Check for existing data about the current tray using stat struct
  char dir_base[50] = "../data/obstructed-area";
  struct stat check_dir;
  if (stat(dir_base, &check_dir) != 0) {
    // Failed to find base directory
    std::cout << t_red << "ERROR" << t_def;
    std::cout << " :: Could not find directory for data about obstructed area ";
    std::cout << t_blu << dir_base << t_def << ". Please add the directory and relevant data." << std::endl;
    return;
  }// End of directory check
  
  
  // Loop over all trays to gather data
  int n_trays = gReader->GetTrayStrings()->size();
  for (int i_tray = 0; i_tray < n_trays; ++i_tray) {
    
    
    // Directory OK, check for current tray's file:
    char subfile[100];
    snprintf(subfile, 100, "%s/%s-area.txt",dir_base,gReader->GetTrayStrings()->at(i_tray).c_str());
    struct stat check_file;
    
    // failed to find file within the directory, continue to next tray
    if (stat(subfile, &check_file) != 0 || (check_file.st_mode & S_IFDIR)) continue;
    
    // Found a file! Print that we found it.
    std::cout << "Data found for surface imperfections in tray ";
    std::cout << t_grn << gReader->GetTrayStrings()->at(i_tray).c_str() << t_def << std::endl;
    
    // *-- Analyze the found file
    std::cout << "i_tray = " << i_tray << std::endl;
    double avg_IV_this_tray = getAvgVpeak(i_tray);
    double avg_PS_this_tray = getAvgVbreakdown(i_tray);
    
    // Keep track of which SiPMs have area defects
    bool has_defect[NROW][NCOL];
    for (int i = 0; i < NROW*NCOL; ++i) has_defect[i/NCOL][i%NCOL] = false;
    
    // Append data to histograms
    std::ifstream area_file(subfile);
    std::string line;
    while (getline(area_file, line)) {
      std::stringstream linestream(line);
      std::string entry;
      int row, col;
      double area;
      
      int info_index = 0;
      while (getline(linestream, entry, ' ')) {
        if (entry.size() == 0) continue; // repeated space
        if (entry[0] == '#') break; // comment
        
        // Initial entries--row and column of the SiPM
        if (info_index == 0) {
          row = std::stoi(entry);
          ++info_index;
          continue;
        } if (info_index == 1) {
          col = std::stoi(entry);
          ++info_index;
          continue;
        }// End of SiPM row, col
        
        // Remaining entries -- Info about the defects
        // Each SiPM may have many defects
        if (info_index % 2) { // odd : obstruction type
          // Mark the current SiPM as having a defect
          has_defect[row][col] = true;
          
          if (gReader->GetVbdTrayIndexIV(i_tray, row, col, global_flag_run_at_25_celcius) == -999) break; // no data for this SiPM--go to next line
          
          // Find the deviation for this SiPM from the data
          double voltage_deviation_IV = gReader->GetVbdTrayIndexIV(i_tray, row, col, global_flag_run_at_25_celcius) - avg_IV_this_tray;
          double voltage_deviation_PS = gReader->GetVbdTrayIndexSPS(i_tray, row, col, global_flag_run_at_25_celcius) - avg_PS_this_tray;
          
          if (debug_surface_imperfection_correlation) {
            std::cout << "Check deviation for debug: (" << row  << ',' << col << ") [";
            std::cout << NCOL*row + col << "] dIV = " << voltage_deviation_IV;
            std::cout << ", dSPS = " << voltage_deviation_PS << std::endl;
          }
          
          // Fill the relevant histogram
          switch (entry[0]) {
            case 's': // scratch
              // TH2D
              hist_surface_corr_IV_scratch->Fill(area, voltage_deviation_IV);
              hist_surface_corr_PS_scratch->Fill(area, voltage_deviation_PS);
              // y-projection, separate control group
              if (area == 0.1) {
                hist_surface_cont_IV_scratch->Fill(voltage_deviation_IV);
                hist_surface_cont_PS_scratch->Fill(voltage_deviation_PS);
              } else {
                hist_surface_proj_IV_scratch->Fill(voltage_deviation_IV);
                hist_surface_proj_PS_scratch->Fill(voltage_deviation_PS);
              }break;
            case 'b': // bubble/manufacturing defect
              // TH2D
              hist_surface_corr_IV_bubble->Fill(area, voltage_deviation_IV);
              hist_surface_corr_PS_bubble->Fill(area, voltage_deviation_PS);
              // y-projection, separate control group
              if (area == 0.1) {
                hist_surface_cont_IV_bubble->Fill(voltage_deviation_IV);
                hist_surface_cont_PS_bubble->Fill(voltage_deviation_PS);
              } else {
                hist_surface_proj_IV_bubble->Fill(voltage_deviation_IV);
                hist_surface_proj_PS_bubble->Fill(voltage_deviation_PS);
              }break;
            case 'd': // debris
              // TH2D
              hist_surface_corr_IV_debris->Fill(area, voltage_deviation_IV);
              hist_surface_corr_PS_debris->Fill(area, voltage_deviation_PS);
              // y-projection, separate control group
              if (area == 0.1) {
                hist_surface_cont_IV_debris->Fill(voltage_deviation_IV);
                hist_surface_cont_PS_debris->Fill(voltage_deviation_PS);
              } else {
                hist_surface_proj_IV_debris->Fill(voltage_deviation_IV);
                hist_surface_proj_PS_debris->Fill(voltage_deviation_PS);
              }break;
            default:
              std::cout << t_red << "Warning" << t_def << " :: ";
              std::cout << "Read surface imperfection type '";
              std::cout << t_blu << entry[0]  << t_def << "' not recognized. Discarding entry." << std::endl;
          }// End of hist type switch/fill
        } else { // Even :: tells the quantity of SiPM pixels obstructed
          if (entry[0] == 'c') { // Control group--defect does not obstruct any pixels
            area = 0.1; // to fill the small bin in log space
          } else area = std::stoi(entry);
        }// End of data read conditionals
        
        ++info_index;
      }// End of line read
    }// End of file read
    
    // Add non-obstructed to the double-control
    for (int row = 0; row < NROW; ++row) {
      for (int col = 0; col < NCOL; ++col) {
        if (gReader->GetVbdTrayIndexIV(i_tray, row, col, global_flag_run_at_25_celcius) == -999) continue; // no data for this SiPM--go to next line
        if (has_defect[row][col]) continue; // non-obstructed only
        
        
        double voltage_deviation_IV = gReader->GetVbdTrayIndexIV(i_tray, row, col, global_flag_run_at_25_celcius) - avg_IV_this_tray;
        double voltage_deviation_PS = gReader->GetVbdTrayIndexSPS(i_tray, row, col, global_flag_run_at_25_celcius) - avg_PS_this_tray;
        hist_nonobstructed_IV->Fill(voltage_deviation_IV);
        hist_nonobstructed_PS->Fill(voltage_deviation_PS);
      }
    }// End of non-obstructed SiPM fill
  }// End of tray loop
  
  
  // Plot and print -- double differential correlation plots
  double max_all_TH2 = 0;
  for (int p = 0; p < 6; ++p) {
    surfacecorr_pads[p/3][p%3]->cd();
    gPad->SetLogx();
    gPad->SetTicks(1,1);
    
    // Find max in all hists to make sure all histograms have the same z-axis scale
    if (hist_surface_corr_IV_scratch->GetMaximum() > max_all_TH2)
      max_all_TH2 = hist_surface_corr_IV_scratch->GetMaximum();
  }
  
  surfacecorr_pads[0][0]->cd();
  hist_surface_corr_IV_scratch->GetYaxis()->SetTitleOffset(1.9);
  hist_surface_corr_IV_scratch->GetYaxis()->SetTitleSize(0.04);
  hist_surface_corr_IV_scratch->GetZaxis()->SetRangeUser(0, max_all_TH2);
  hist_surface_corr_IV_scratch->Draw("col");
  drawText("IV", 1. - gPad->GetRightMargin() - 0.05, 1. - gPad->GetTopMargin() - 0.08, true, kRed+1, 0.06);
  
  surfacecorr_pads[0][1]->cd();
  hist_surface_corr_IV_bubble->GetZaxis()->SetRangeUser(0, max_all_TH2);
  hist_surface_corr_IV_bubble->Draw("col");
  drawText("IV", 1. - gPad->GetRightMargin() - 0.05, 1. - gPad->GetTopMargin() - 0.08, true, kRed+1, 0.065);
  
  surfacecorr_pads[0][2]->cd();
  hist_surface_corr_IV_debris->GetZaxis()->SetTitleOffset(0.9);
  hist_surface_corr_IV_debris->GetZaxis()->SetTitleSize(0.05);
  hist_surface_corr_IV_debris->GetZaxis()->SetRangeUser(0, max_all_TH2);
  hist_surface_corr_IV_debris->Draw("colz");
  drawText("IV", 1. - gPad->GetRightMargin() - 0.05, 1. - gPad->GetTopMargin() - 0.08, true, kRed+1, 0.06);
  
  surfacecorr_pads[1][0]->cd();
  hist_surface_corr_PS_scratch->GetXaxis()->SetTitleOffset(0.9);
  hist_surface_corr_PS_scratch->GetXaxis()->SetTitleSize(0.05);
  hist_surface_corr_PS_scratch->GetYaxis()->SetTitleOffset(1.9);
  hist_surface_corr_PS_scratch->GetYaxis()->SetTitleSize(0.04);
  hist_surface_corr_PS_scratch->GetZaxis()->SetRangeUser(0, max_all_TH2);
  hist_surface_corr_PS_scratch->Draw("col");
  drawText("SPS", 1. - gPad->GetRightMargin() - 0.05, 1. - gPad->GetTopMargin() - 0.08, true, kRed+1, 0.06);
  drawText("Surface Scratch", 0.25, 0.04, false, kRed+1, 0.08);
  
  surfacecorr_pads[1][1]->cd();
  hist_surface_corr_PS_bubble->GetXaxis()->SetTitleOffset(0.75);
  hist_surface_corr_PS_bubble->GetXaxis()->SetTitleSize(0.06);
  hist_surface_corr_PS_bubble->GetXaxis()->SetLabelSize(0.04);
  hist_surface_corr_PS_bubble->GetXaxis()->SetLabelOffset(0.0005);
  hist_surface_corr_PS_bubble->GetZaxis()->SetRangeUser(0, max_all_TH2);
  hist_surface_corr_PS_bubble->Draw("col");
  drawText("SPS", 1. - gPad->GetRightMargin() - 0.05, 1. - gPad->GetTopMargin() - 0.08, true, kRed+1, 0.065);
  drawText("Surface Bubble", 0.20, 0.04, false, kRed+1, 0.092);
  
  surfacecorr_pads[1][2]->cd();
  hist_surface_corr_PS_debris->GetXaxis()->SetTitleOffset(0.9);
  hist_surface_corr_PS_debris->GetXaxis()->SetTitleSize(0.05);
  hist_surface_corr_PS_debris->GetZaxis()->SetTitleOffset(0.9);
  hist_surface_corr_PS_debris->GetZaxis()->SetTitleSize(0.05);
  hist_surface_corr_PS_debris->GetZaxis()->SetRangeUser(0, max_all_TH2);
  hist_surface_corr_PS_debris->Draw("colz");
  drawText("SPS", 1. - gPad->GetRightMargin() - 0.05, 1. - gPad->GetTopMargin() - 0.08, true, kRed+1, 0.06);
  drawText("Obstructing Debris", 0.15, 0.04, false, kRed+1, 0.08);
  
  
  // Draw some informative text about the setup
  gCanvas_surfacecorr->cd();
  TLatex* top_tex[6];
  top_tex[0] = drawText("#bf{Debrecen} SiPM Test Setup @ #bf{Yale}",                 0.05, 0.91, false, kBlack, 0.04);
  top_tex[1] = drawText("#bf{ePIC} Test Stand",                                      0.05, 0.955, false, kBlack, 0.045);
  top_tex[2] = drawText(Form("Hamamatsu #bf{%s}", Hamamatsu_SiPM_Code),              0.95, 0.95, true, kBlack, 0.045);
//  top_tex[3] = drawText(Form("Tray %s SiPM (%i,%i)",
//                             gReader->GetIV()->at(cycle_tray_indices[0])->tray_note.substr(0,11).c_str(),
//                             sipm_row[i_sipm],sipm_col[i_sipm]),                   1-gPad->GetRightMargin(), 0.91, true, kBlack, 0.035);
  top_tex[4] = drawText("Systematics: Imperfection Correlation",          0.95, 0.91, true, kBlack, 0.035);
//  top_tex[5] = drawText(Form("%i Total Tests During Cooldown",ntotal_scan),          gPad->GetLeftMargin() + 0.05, 0.78, false, kBlack, 0.04);
  
  gCanvas_surfacecorr->SaveAs("../plots/systematic_plots/surface_imperfections/imperfection_correlation.pdf");
  
  
  // Plot and print -- y-projections with separated control groups
  for (int p = 0; p < 6; ++p) {
    surfacecorr_pads[p/3][p%3]->cd();
    gPad->SetLogx(0);
  }
  
  // Scale all plots to unity
  hist_nonobstructed_IV->Scale(1./hist_nonobstructed_IV->Integral(), "width");
  hist_nonobstructed_PS->Scale(1./hist_nonobstructed_PS->Integral(), "width");
  
  hist_surface_proj_IV_scratch->Scale(1./hist_surface_proj_IV_scratch->Integral(), "width");
  hist_surface_cont_IV_scratch->Scale(1./hist_surface_cont_IV_scratch->Integral(), "width");
  hist_surface_proj_PS_scratch->Scale(1./hist_surface_proj_PS_scratch->Integral(), "width");
  hist_surface_cont_PS_scratch->Scale(1./hist_surface_cont_PS_scratch->Integral(), "width");
  
  hist_surface_proj_IV_bubble->Scale(1./hist_surface_proj_IV_bubble->Integral(), "width");
  hist_surface_cont_IV_bubble->Scale(1./hist_surface_cont_IV_bubble->Integral(), "width");
  hist_surface_proj_PS_bubble->Scale(1./hist_surface_proj_PS_bubble->Integral(), "width");
  hist_surface_cont_PS_bubble->Scale(1./hist_surface_cont_PS_bubble->Integral(), "width");
  
  hist_surface_proj_IV_debris->Scale(1./hist_surface_proj_IV_debris->Integral(), "width");
  hist_surface_cont_IV_debris->Scale(1./hist_surface_cont_IV_debris->Integral(), "width");
  hist_surface_proj_PS_debris->Scale(1./hist_surface_proj_PS_debris->Integral(), "width");
  hist_surface_cont_PS_debris->Scale(1./hist_surface_cont_PS_debris->Integral(), "width");
  
  // Format plots
  double proj_plot_range[2] = {0.1, 29};
  hist_nonobstructed_IV->SetLineColor(kBlack);
  hist_nonobstructed_PS->SetLineColor(kBlack);
  
  hist_surface_proj_IV_scratch->SetLineColor(plot_colors[0]);
  hist_surface_cont_IV_scratch->SetLineColor(plot_colors_alt[0] + 3);
  hist_surface_proj_PS_scratch->SetLineColor(plot_colors[1]);
  hist_surface_cont_PS_scratch->SetLineColor(plot_colors_alt[1]);
  hist_surface_proj_IV_scratch->SetMarkerColor(plot_colors[0]);
  hist_surface_cont_IV_scratch->SetMarkerColor(plot_colors_alt[0] + 3);
  hist_surface_proj_PS_scratch->SetMarkerColor(plot_colors[1]);
  hist_surface_cont_PS_scratch->SetMarkerColor(plot_colors_alt[1]);
  hist_surface_proj_IV_scratch->SetMarkerStyle(20);
  hist_surface_cont_IV_scratch->SetMarkerStyle(20);
  hist_surface_proj_PS_scratch->SetMarkerStyle(21);
  hist_surface_cont_PS_scratch->SetMarkerStyle(21);
  
  hist_surface_proj_IV_bubble->SetLineColor(plot_colors[0]);
  hist_surface_cont_IV_bubble->SetLineColor(plot_colors_alt[0] + 3);
  hist_surface_proj_PS_bubble->SetLineColor(plot_colors[1]);
  hist_surface_cont_PS_bubble->SetLineColor(plot_colors_alt[1]);
  hist_surface_proj_IV_bubble->SetMarkerColor(plot_colors[0]);
  hist_surface_cont_IV_bubble->SetMarkerColor(plot_colors_alt[0] + 3);
  hist_surface_proj_PS_bubble->SetMarkerColor(plot_colors[1]);
  hist_surface_cont_PS_bubble->SetMarkerColor(plot_colors_alt[1]);
  hist_surface_proj_IV_bubble->SetMarkerStyle(20);
  hist_surface_cont_IV_bubble->SetMarkerStyle(20);
  hist_surface_proj_PS_bubble->SetMarkerStyle(21);
  hist_surface_cont_PS_bubble->SetMarkerStyle(21);
  
  hist_surface_proj_IV_debris->SetLineColor(plot_colors[0]);
  hist_surface_cont_IV_debris->SetLineColor(plot_colors_alt[0] + 3);
  hist_surface_proj_PS_debris->SetLineColor(plot_colors[1]);
  hist_surface_cont_PS_debris->SetLineColor(plot_colors_alt[1]);
  hist_surface_proj_IV_debris->SetMarkerColor(plot_colors[0]);
  hist_surface_cont_IV_debris->SetMarkerColor(plot_colors_alt[0] + 3);
  hist_surface_proj_PS_debris->SetMarkerColor(plot_colors[1]);
  hist_surface_cont_PS_debris->SetMarkerColor(plot_colors_alt[1]);
  hist_surface_proj_IV_debris->SetMarkerStyle(20);
  hist_surface_cont_IV_debris->SetMarkerStyle(20);
  hist_surface_proj_PS_debris->SetMarkerStyle(21);
  hist_surface_cont_PS_debris->SetMarkerStyle(21);
  
  surfacecorr_pads[0][0]->cd();
  hist_surface_proj_IV_scratch->GetYaxis()->SetRangeUser(proj_plot_range[0], proj_plot_range[1]);
  hist_surface_proj_IV_scratch->GetYaxis()->SetTitleOffset(1.9);
  hist_surface_proj_IV_scratch->GetYaxis()->SetTitleSize(0.04);
  hist_surface_proj_IV_scratch->Draw("hist p");
  hist_surface_cont_IV_scratch->Draw("hist same");
  hist_nonobstructed_IV->Draw("hist same");
  drawText("IV", 1. - gPad->GetRightMargin() - 0.05, 1. - gPad->GetTopMargin() - 0.08, true, kRed+1, 0.06);
  
  TLegend* leg_IV = new TLegend(0.2, 0.55, 0.65, 0.75);
  leg_IV->SetLineWidth(0);
  leg_IV->AddEntry(hist_surface_proj_IV_scratch, "Pixels Obstructed", "p");
  leg_IV->AddEntry(hist_surface_cont_IV_scratch, "Pixels Unobstructed", "l");
  leg_IV->AddEntry(hist_nonobstructed_IV, "No Surface Defect", "l");
  leg_IV->Draw();
  
  surfacecorr_pads[0][1]->cd();
  hist_surface_proj_IV_bubble->GetYaxis()->SetRangeUser(proj_plot_range[0], proj_plot_range[1]);
  hist_surface_proj_IV_bubble->Draw("hist p");
  hist_surface_cont_IV_bubble->Draw("hist same");
  hist_nonobstructed_IV->Draw("hist same");
  drawText("IV", 1. - gPad->GetRightMargin() - 0.05, 1. - gPad->GetTopMargin() - 0.08, true, kRed+1, 0.06);
  
  surfacecorr_pads[0][2]->cd();
  hist_surface_proj_IV_debris->GetYaxis()->SetRangeUser(proj_plot_range[0], proj_plot_range[1]);
  hist_surface_proj_IV_debris->Draw("hist p");
  hist_surface_cont_IV_debris->Draw("hist same");
  hist_nonobstructed_IV->Draw("hist same");
  drawText("IV", 1. - gPad->GetRightMargin() - 0.05, 1. - gPad->GetTopMargin() - 0.08, true, kRed+1, 0.06);
  
  surfacecorr_pads[1][0]->cd();
  hist_surface_proj_PS_scratch->GetYaxis()->SetRangeUser(proj_plot_range[0], proj_plot_range[1]);
  hist_surface_proj_PS_scratch->GetYaxis()->SetTitleOffset(1.9);
  hist_surface_proj_PS_scratch->GetYaxis()->SetTitleSize(0.04);
  hist_surface_proj_PS_scratch->GetXaxis()->SetTitleOffset(1.05);
  hist_surface_proj_PS_scratch->GetXaxis()->SetTitleSize(0.04);
  hist_surface_proj_PS_scratch->Draw("hist p");
  hist_surface_cont_PS_scratch->Draw("hist same");
  hist_nonobstructed_PS->Draw("hist same");
  drawText("SPS", 1. - gPad->GetRightMargin() - 0.05, 1. - gPad->GetTopMargin() - 0.08, true, kRed+1, 0.06);
  drawText("Surface Scratch", 0.25, 0.04, false, kRed+1, 0.08);
  
  TLegend* leg_PS = new TLegend(0.2, 0.75, 0.65, 0.95);
  leg_PS->SetLineWidth(0);
  leg_PS->AddEntry(hist_surface_proj_PS_scratch, "Pixels Obstructed", "p");
  leg_PS->AddEntry(hist_surface_cont_PS_scratch, "Pixels Unobstructed", "l");
  leg_PS->AddEntry(hist_nonobstructed_PS, "No Surface Defect", "l");
  leg_PS->Draw();
  
  surfacecorr_pads[1][1]->cd();
  hist_surface_proj_PS_bubble->GetYaxis()->SetRangeUser(proj_plot_range[0], proj_plot_range[1]);
  hist_surface_proj_PS_bubble->GetXaxis()->SetTitleOffset(0.9);
  hist_surface_proj_PS_bubble->GetXaxis()->SetTitleSize(0.05);
  hist_surface_proj_PS_bubble->GetXaxis()->SetLabelSize(0.04);
  hist_surface_proj_PS_bubble->GetXaxis()->SetLabelOffset(0.0005);
  hist_surface_proj_PS_bubble->Draw("hist p");
  hist_surface_cont_PS_bubble->Draw("hist same");
  hist_nonobstructed_PS->Draw("hist same");
  drawText("SPS", 1. - gPad->GetRightMargin() - 0.05, 1. - gPad->GetTopMargin() - 0.08, true, kRed+1, 0.065);
  drawText("Surface Bubble", 0.20, 0.04, false, kRed+1, 0.092);
  
  surfacecorr_pads[1][2]->cd();
  hist_surface_proj_PS_debris->GetYaxis()->SetRangeUser(proj_plot_range[0], proj_plot_range[1]);
  hist_surface_proj_PS_debris->GetXaxis()->SetTitleOffset(1.1);
  hist_surface_proj_PS_debris->GetXaxis()->SetTitleSize(0.04);
  hist_surface_proj_PS_debris->Draw("hist p");
  hist_surface_cont_PS_debris->Draw("hist same");
  hist_nonobstructed_PS->Draw("hist same");
  drawText("SPS", 1. - gPad->GetRightMargin() - 0.05, 1. - gPad->GetTopMargin() - 0.08, true, kRed+1, 0.06);
  drawText("Obstructing Debris", 0.15, 0.04, false, kRed+1, 0.08);
  
  gCanvas_surfacecorr->cd();
  delete top_tex[4];
  top_tex[5] = drawText("Systematics: Imperfection Projection",          0.95, 0.91, true, kBlack, 0.035);
  
  
  gCanvas_surfacecorr->SaveAs("../plots/systematic_plots/surface_imperfections/imperfection_projection.pdf");
  
  
}// End of systematic_analysis_summary::makeSurfaceImperfectionCorrelation

