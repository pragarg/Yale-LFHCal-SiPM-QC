//  *--
//  compare_robot_cassette.cpp
//
//  Produces plots comparing data results from
//  the Cassette setup (trays 250717 through some of 251211)
//  and the robot/automated setup delivered by Debrecen
//  to Yale in April 2026.
//
//  Broken away from sipm_batch_summary_sheet.c
//
//  Changelog ::
//    - 5/21/2026   : Created by Ryan Hamilton
//  *--

#include "sipm_batch_summary_sheet.cpp"

//========================================================================== Global Variables



//========================================================================== Forward declarations

// Correlations and comparisons
void makeCorrelationIV(std::string tray1, std::string tray2, bool flag_run_at_25_celcius = true);
void makeCorrelationSPS(std::string tray1, std::string tray2, bool flag_run_at_25_celcius = true);
void makeCorrelationDarkCurrent(std::string tray1, std::string tray2, bool below_breakdown = true);

//========================================================================== Macro Main

// Main macro method: generate SiPM data
void sipm_batch_summary_sheet() {
  SiPMDataReader* reader = new SiPMDataReader();
  gErrorIgnoreLevel = kWarning;
  
  
  // Check robot
  reader->ReadFile("../batch_data_robotcheck.txt");
//  reader->SetPrintSPS();
//  reader->SetFlatTrayString();  // Do not require "-results" in text file
  
  // Read IV and SPS data
  reader->ReadDataIV();
  reader->ReadDataSPS();
  
  // Initialize canvases
  gCanvas_solo = new TCanvas();
  gCanvas_double = new TCanvas();
  gStyle->SetOptStat(0);
  
  int n_trays = gReader->GetTrayStrings()->size();
  for (int i_tray = 0; i_tray < n_trays; ++i_tray) {
    std::cout << "Average V_bd (25C) for tray " << gReader->GetTrayStrings()->at(i_tray) << " \t:: " << getAvgVbreakdown(i_tray, true);
    std::cout << " (" << t_red << countOutliersVbreakdown(i_tray, true) << t_def << " Outliers beyond tray avg +/-" << declare_Vbd_outlier_range << "V)" << std::endl;
  }
  
  makeIndexSeries(true);
  makeIndexSeries(false);
  makeIndexDifference(true);
  makeIndexDifference(false);
  
  // Make 2D mappings to invesitage strange deviations
  makeTrayMapVpeak(false);
  makeTrayMapVbreakdown(false);
  makeTestMapVpeak();
  makeTestMapVbreakdown();
  
  gReader->WriteCompressedFile(-1);
  
  makeHist_DarkCurrent();
  
  makeCorrelationIV("250911-1606", "250911-1606-robot", false);
  makeCorrelationIV("250911-1606", "250911-1606-robot", true);
  makeCorrelationSPS("250911-1606", "250911-1606-robot", false);
  makeCorrelationSPS("250911-1606", "250911-1606-robot", true);
  makeCorrelationDarkCurrent("250911-1606", "250911-1606-robot", false);
  makeCorrelationDarkCurrent("250911-1606", "250911-1606-robot", true);
}// End of compare_robot_cassette::main



//========================================================================== Pair plot generating Macros: Comparisons/Correlations


// Draw a 2D correlation plot of the IV results between two trays stored in the reader
void makeCorrelationIV(std::string tray1, std::string tray2, bool flag_run_at_25_celcius) {
  
  gCanvas_solo->Clear();
  gCanvas_solo->SetCanvasSize(600, 600);
  gCanvas_solo->cd();
  gPad->SetTicks(1,1);
  gPad->SetLogy(0);
  gPad->SetRightMargin(0.05);
  gPad->SetLeftMargin(0.135);
  gPad->SetBottomMargin(0.085);
  gPad->SetTopMargin(0.075);
  
  // Find the data for the two requested trays from the reader
  int index_1 = -1;
  int index_2 = -1;
  for (int i_tray = 0; i_tray < gReader->GetTrayStrings()->size(); ++i_tray) {
    if (tray1.compare(gReader->GetTrayStrings()->at(i_tray)) == 0) index_1 = i_tray;
    if (tray2.compare(gReader->GetTrayStrings()->at(i_tray)) == 0) index_2 = i_tray;
  }// End of tray loop
  bool failed_to_find_tray = false;
  if (index_1 == -1) {
    std::cerr << "Error in <sipm_batch_summary_sheet::makeCorrelationIV>: input tray 1 not found." << std::endl;
    failed_to_find_tray = true;
  } if (index_2 == -1) {
    std::cerr << "Error in <sipm_batch_summary_sheet::makeCorrelationIV>: input tray 2 not found." << std::endl;
    failed_to_find_tray = true;
  } if (failed_to_find_tray) return;
  
  // Gather the input data to arrays that can be plotted
  int count_failures_1 = 0;
  std::vector<float> IV_valid_1;
  std::vector<float>* IV_data_1;
  if (!flag_run_at_25_celcius) IV_data_1 = gReader->GetIV()->at(index_1)->IV_Vpeak;
  else                         IV_data_1 = gReader->GetIV()->at(index_1)->IV_Vpeak_25C;
  int count_failures_2 = 0;
  std::vector<float> IV_valid_2;
  std::vector<float>* IV_data_2;
  if (!flag_run_at_25_celcius) IV_data_2 = gReader->GetIV()->at(index_2)->IV_Vpeak;
  else                         IV_data_2 = gReader->GetIV()->at(index_2)->IV_Vpeak_25C;
  
  for (int i_sipm = 0; i_sipm < 460; ++i_sipm) {
    bool is_failure = false;
    if (IV_data_1->at(i_sipm) == -999) {++count_failures_1; is_failure = true;}
    if (IV_data_2->at(i_sipm) == -999) {++count_failures_2; is_failure = true;}
    
    if (is_failure) continue;
    
    IV_valid_1.push_back(IV_data_1->at(i_sipm));
    IV_valid_2.push_back(IV_data_2->at(i_sipm));
  }// End of SiPM data loop
  
  // Plot a TGraph object of the correlation
  TGraph* graph_correlation = new TGraph(IV_valid_1.size(), IV_valid_1.data(), IV_valid_2.data());
  
  // Label axes
  graph_correlation->SetTitle("");
  graph_correlation->GetXaxis()->SetTitle(Form("%s V_{br}^{IV}",tray1.c_str()));
  graph_correlation->GetYaxis()->SetTitle(Form("%s V_{br}^{IV}",tray2.c_str()));
  
  // Draw
  graph_correlation->SetMarkerColor(plot_colors[0]);
  graph_correlation->SetMarkerStyle(20);
  graph_correlation->Draw("AP");
  
  // Add some lines to help make the correlation more legible
  double min_axis[2] = {
    graph_correlation->GetXaxis()->GetBinLowEdge(graph_correlation->GetXaxis()->GetFirst()),
    graph_correlation->GetYaxis()->GetBinLowEdge(graph_correlation->GetYaxis()->GetFirst())
  };
  double max_axis[2] = {
    graph_correlation->GetXaxis()->GetBinUpEdge(graph_correlation->GetXaxis()->GetLast()),
    graph_correlation->GetYaxis()->GetBinUpEdge(graph_correlation->GetYaxis()->GetLast())
  };
  
  // Line for x = y in the correlation
  TLine* line_equality = new TLine();
  double range_corr[2] = {
    TMath::Max(min_axis[0], min_axis[1]),
    TMath::Min(max_axis[0], max_axis[1]),
  };
  line_equality->DrawLine(range_corr[0], range_corr[0], range_corr[1], range_corr[1]);
  
  // Outlier lines
  double avg_IV[2] = {
    getAvgVpeak(index_1, flag_run_at_25_celcius),
    getAvgVpeak(index_2, flag_run_at_25_celcius)
  };
  TLine* line_outliers = new TLine();
  line_outliers->SetLineStyle(7);
  line_outliers->SetLineColor(kGray+2);
  line_outliers->DrawLine(avg_IV[0] - declare_Vbd_outlier_range, min_axis[1], avg_IV[0] - declare_Vbd_outlier_range, max_axis[1]);
  line_outliers->DrawLine(avg_IV[0] + declare_Vbd_outlier_range, min_axis[1], avg_IV[0] + declare_Vbd_outlier_range, max_axis[1]);
  line_outliers->DrawLine(min_axis[0], avg_IV[1] - declare_Vbd_outlier_range, max_axis[0], avg_IV[1] - declare_Vbd_outlier_range);
  line_outliers->DrawLine(min_axis[0], avg_IV[1] + declare_Vbd_outlier_range, max_axis[0], avg_IV[1] + declare_Vbd_outlier_range);
  
  // Add a small legend explaining what each item is
  TLegend* correlation_legend = new TLegend(0.2, 0.74, 0.5, 0.86);
  correlation_legend->AddEntry(graph_correlation, "Correlation", "p");
  correlation_legend->AddEntry(line_equality, "Equality", "l");
  correlation_legend->AddEntry(line_outliers, "Outliers", "l");
//  correlation_legend->SetLineWidth(0);
  correlation_legend->Draw();
  
  // Draw some text giving info on the setup
  drawText("#bf{ePIC} Test Stand", gPad->GetLeftMargin(), 0.965, false, kBlack, 0.03);
  drawText("#bf{Debrecen} SiPM Test Setup @ #bf{Yale}", gPad->GetLeftMargin(), 0.935, false, kBlack, 0.03);
  drawText(Form("Hamamatsu #bf{%s}", Hamamatsu_SiPM_Code), 1-gPad->GetRightMargin(), 0.965, true, kBlack, 0.03);
  drawText(Form("%s", string_tempcorr[flag_run_at_25_celcius]), 1-gPad->GetRightMargin(), 0.935, true, kBlack, 0.023);
  
  gCanvas_solo->SaveAs(Form("../plots/single_plots/correlations/IVcorr_Vbr%s_%s_%s.pdf",
                            string_tempcorr_short[flag_run_at_25_celcius],
                            gReader->GetTrayStrings()->at(index_1).c_str(),
                            gReader->GetTrayStrings()->at(index_2).c_str()));
  
  return;
}// End of sipm_batch_summary_sheet::makeCorrelationIV



// Draw a 2D correlation plot of the SPS results between two trays stored in the reader
void makeCorrelationSPS(std::string tray1, std::string tray2, bool flag_run_at_25_celcius) {
  
  gCanvas_solo->Clear();
  gCanvas_solo->SetCanvasSize(800, 800);
  gCanvas_solo->cd();
  gPad->SetTicks(1,1);
  gPad->SetLogy(0);
  gPad->SetRightMargin(0.05);
  gPad->SetLeftMargin(0.135);
  gPad->SetBottomMargin(0.085);
  gPad->SetTopMargin(0.075);
  
  // Find the data for the two requested trays from the reader
  int index_1 = -1;
  int index_2 = -1;
  for (int i_tray = 0; i_tray < gReader->GetTrayStrings()->size(); ++i_tray) {
    if (tray1.compare(gReader->GetTrayStrings()->at(i_tray)) == 0) index_1 = i_tray;
    if (tray2.compare(gReader->GetTrayStrings()->at(i_tray)) == 0) index_2 = i_tray;
  }// End of tray loop
  bool failed_to_find_tray = false;
  if (index_1 == -1) {
    std::cerr << "Error in <sipm_batch_summary_sheet::makeCorrelationSPS>: input tray 1 not found." << std::endl;
    failed_to_find_tray = true;
  } if (index_2 == -1) {
    std::cerr << "Error in <sipm_batch_summary_sheet::makeCorrelationSPS>: input tray 2 not found." << std::endl;
    failed_to_find_tray = true;
  } if (failed_to_find_tray) return;
  
  // Gather the input data to arrays that can be plotted
  int count_failures_1 = 0;
  std::vector<float> SPS_valid_1;
  std::vector<float>* SPS_data_1;
  if (!flag_run_at_25_celcius) SPS_data_1 = gReader->GetSPS()->at(index_1)->SPS_Vbd;
  else                         SPS_data_1 = gReader->GetSPS()->at(index_1)->SPS_Vbd_25C;
  int count_failures_2 = 0;
  std::vector<float> SPS_valid_2;
  std::vector<float>* SPS_data_2;
  if (!flag_run_at_25_celcius) SPS_data_2 = gReader->GetSPS()->at(index_2)->SPS_Vbd;
  else                         SPS_data_2 = gReader->GetSPS()->at(index_2)->SPS_Vbd_25C;
  
  for (int i_sipm = 0; i_sipm < 460; ++i_sipm) {
    bool is_failure = false;
    if (SPS_data_1->at(i_sipm) == -999) {++count_failures_1; is_failure = true;}
    if (SPS_data_2->at(i_sipm) == -999) {++count_failures_2; is_failure = true;}
    
    if (is_failure) continue;
    
    SPS_valid_1.push_back(SPS_data_1->at(i_sipm));
    SPS_valid_2.push_back(SPS_data_2->at(i_sipm));
  }// End of SiPM data loop
  
  // Plot a TGraph object of the correlation
  TGraph* graph_correlation = new TGraph(SPS_valid_1.size(), SPS_valid_1.data(), SPS_valid_2.data());
  
  // Label axes
  graph_correlation->SetTitle("");
  graph_correlation->GetXaxis()->SetTitle(Form("%s V_{br}^{SPS}",tray1.c_str()));
  graph_correlation->GetYaxis()->SetTitle(Form("%s V_{br}^{SPS}",tray2.c_str()));
  
  graph_correlation->SetMarkerColor(plot_colors[1]);
  graph_correlation->SetMarkerStyle(21);
  graph_correlation->Draw("AP");
  
  // Add some lines to help make the correlation more legible
  double min_axis[2] = {
    graph_correlation->GetXaxis()->GetBinLowEdge(graph_correlation->GetXaxis()->GetFirst()),
    graph_correlation->GetYaxis()->GetBinLowEdge(graph_correlation->GetYaxis()->GetFirst())
  };
  double max_axis[2] = {
    graph_correlation->GetXaxis()->GetBinUpEdge(graph_correlation->GetXaxis()->GetLast()),
    graph_correlation->GetYaxis()->GetBinUpEdge(graph_correlation->GetYaxis()->GetLast())
  };
  
  // Line for x = y in the correlation
  TLine* line_equality = new TLine();
  double range_corr[2] = {
    TMath::Max(min_axis[0], min_axis[1]),
    TMath::Min(max_axis[0], max_axis[1]),
  };
  line_equality->DrawLine(range_corr[0], range_corr[0], range_corr[1], range_corr[1]);
  
  // Outlier lines
  double avg_SPS[2] = {
    getAvgVbreakdown(index_1, flag_run_at_25_celcius),
    getAvgVbreakdown(index_2, flag_run_at_25_celcius)
  };
  TLine* line_outliers = new TLine();
  line_outliers->SetLineStyle(7);
  line_outliers->SetLineColor(kGray+2);
  line_outliers->DrawLine(avg_SPS[0] - declare_Vbd_outlier_range, min_axis[1], avg_SPS[0] - declare_Vbd_outlier_range, max_axis[1]);
  line_outliers->DrawLine(avg_SPS[0] + declare_Vbd_outlier_range, min_axis[1], avg_SPS[0] + declare_Vbd_outlier_range, max_axis[1]);
  line_outliers->DrawLine(min_axis[0], avg_SPS[1] - declare_Vbd_outlier_range, max_axis[0], avg_SPS[1] - declare_Vbd_outlier_range);
  line_outliers->DrawLine(min_axis[0], avg_SPS[1] + declare_Vbd_outlier_range, max_axis[0], avg_SPS[1] + declare_Vbd_outlier_range);
  
  // Add a small legend explaining what each item is
  TLegend* correlation_legend = new TLegend(0.2, 0.74, 0.5, 0.86);
  correlation_legend->AddEntry(graph_correlation, "Correlation", "p");
  correlation_legend->AddEntry(line_equality, "Equality", "l");
  correlation_legend->AddEntry(line_outliers, "Outliers", "l");
  //  correlation_legend->SetLineWidth(0);
  correlation_legend->Draw();
  
  // Draw some text giving info on the setup
  drawText("#bf{ePIC} Test Stand", gPad->GetLeftMargin(), 0.965, false, kBlack, 0.03);
  drawText("#bf{Debrecen} SiPM Test Setup @ #bf{Yale}", gPad->GetLeftMargin(), 0.935, false, kBlack, 0.03);
  drawText(Form("Hamamatsu #bf{%s}", Hamamatsu_SiPM_Code), 1-gPad->GetRightMargin(), 0.965, true, kBlack, 0.03);
  drawText(Form("%s", string_tempcorr[flag_run_at_25_celcius]), 1-gPad->GetRightMargin(), 0.935, true, kBlack, 0.023);
  
  gCanvas_solo->SaveAs(Form("../plots/single_plots/correlations/SPScorr_Vbr%s_%s_%s.pdf",
                            string_tempcorr_short[flag_run_at_25_celcius],
                            gReader->GetTrayStrings()->at(index_1).c_str(),
                            gReader->GetTrayStrings()->at(index_2).c_str()));
  
  return;
}// End of sipm_batch_summary_sheet::makeCorrelationSPS



// Draw a 2D correlation plot of the Dark Current results between two trays stored in the reader
void makeCorrelationDarkCurrent(std::string tray1, std::string tray2, bool below_breakdown) {
  
  gCanvas_solo->Clear();
  gCanvas_solo->SetCanvasSize(800, 800);
  gCanvas_solo->cd();
  gPad->SetTicks(1,1);
  gPad->SetLogy(0);
  gPad->SetRightMargin(0.05);
  gPad->SetLeftMargin(0.135);
  gPad->SetBottomMargin(0.085);
  gPad->SetTopMargin(0.075);
  
  // Find the data for the two requested trays from the reader
  int index_1 = -1;
  int index_2 = -1;
  for (int i_tray = 0; i_tray < gReader->GetTrayStrings()->size(); ++i_tray) {
    if (tray1.compare(gReader->GetTrayStrings()->at(i_tray)) == 0) index_1 = i_tray;
    if (tray2.compare(gReader->GetTrayStrings()->at(i_tray)) == 0) index_2 = i_tray;
  }// End of tray loop
  bool failed_to_find_tray = false;
  if (index_1 == -1) {
    std::cerr << "Error in <sipm_batch_summary_sheet::makeCorrelationDarkCurrent>: input tray 1 not found." << std::endl;
    failed_to_find_tray = true;
  } if (index_2 == -1) {
    std::cerr << "Error in <sipm_batch_summary_sheet::makeCorrelationDarkCurrent>: input tray 2 not found." << std::endl;
    failed_to_find_tray = true;
  } if (failed_to_find_tray) return;
  
  // Gather the input data to arrays that can be plotted
  int count_failures_1 = 0;
  std::vector<float> IDark_valid_1;
  std::vector<float>* IDark_data_1;
  if (below_breakdown) IDark_data_1 = gReader->GetIV()->at(index_1)->Idark_3below;
  else                 IDark_data_1 = gReader->GetIV()->at(index_1)->Idark_4above;
  int count_failures_2 = 0;
  std::vector<float> IDark_valid_2;
  std::vector<float>* IDark_data_2;
  if (below_breakdown) IDark_data_2 = gReader->GetIV()->at(index_2)->Idark_3below;
  else                 IDark_data_2 = gReader->GetIV()->at(index_2)->Idark_4above;
  
  for (int i_sipm = 0; i_sipm < 460; ++i_sipm) {
    bool is_failure = false;
    if (IDark_data_1->at(i_sipm) == -999) {++count_failures_1; is_failure = true;}
    if (IDark_data_2->at(i_sipm) == -999) {++count_failures_2; is_failure = true;}
    
    if (is_failure) continue;
    
    IDark_valid_1.push_back(IDark_data_1->at(i_sipm));
    IDark_valid_2.push_back(IDark_data_2->at(i_sipm));
  }// End of SiPM data loop
  
  // Plot a TGraph object of the correlation
  TGraph* graph_correlation = new TGraph(IDark_valid_1.size(), IDark_valid_1.data(), IDark_valid_2.data());
  
  // Label axes
  graph_correlation->SetTitle("");
  graph_correlation->GetXaxis()->SetTitle(Form("%s I_{Dark}",tray1.c_str()));
  graph_correlation->GetYaxis()->SetTitle(Form("%s I_{Dark}",tray2.c_str()));
  
  graph_correlation->SetMarkerColor(plot_colors[2]);
  graph_correlation->SetMarkerStyle(33);
  graph_correlation->Draw("AP");
  
  // Add some lines to help make the correlation more legible
  double min_axis[2] = {
    graph_correlation->GetXaxis()->GetBinLowEdge(graph_correlation->GetXaxis()->GetFirst()),
    graph_correlation->GetYaxis()->GetBinLowEdge(graph_correlation->GetYaxis()->GetFirst())
  };
  double max_axis[2] = {
    graph_correlation->GetXaxis()->GetBinUpEdge(graph_correlation->GetXaxis()->GetLast()),
    graph_correlation->GetYaxis()->GetBinUpEdge(graph_correlation->GetYaxis()->GetLast())
  };
  
  // Line for x = y in the correlation
  TLine* line_equality = new TLine();
  double range_corr[2] = {
    TMath::Max(min_axis[0], min_axis[1]),
    TMath::Min(max_axis[0], max_axis[1]),
  };
  line_equality->DrawLine(range_corr[0], range_corr[0], range_corr[1], range_corr[1]);
  
  // Outlier lines
  TLine* line_outliers = new TLine();
  line_outliers->SetLineStyle(7);
  line_outliers->SetLineColor(kGray+2);
  line_outliers->DrawLine(Hamamatsu_spec_max_Idark, min_axis[1], Hamamatsu_spec_max_Idark, max_axis[1]);
  line_outliers->DrawLine(min_axis[0], Hamamatsu_spec_max_Idark, max_axis[0], Hamamatsu_spec_max_Idark);
  
  // Add a small legend explaining what each item is
  TLegend* correlation_legend = new TLegend(0.2, 0.74, 0.5, 0.86);
  correlation_legend->AddEntry(graph_correlation, "Correlation", "p");
  correlation_legend->AddEntry(line_equality, "Equality", "l");
  correlation_legend->AddEntry(line_outliers, "Outliers", "l");
  //  correlation_legend->SetLineWidth(0);
  correlation_legend->Draw();
  
  // Draw some text giving info on the setup
  char string_dark_current_types[2][10] = {"4above","3below"};
  char string_dark_current_types_long[2][20] = {"V_{br} + 4","V_{br} - 3"};
  drawText("#bf{ePIC} Test Stand", gPad->GetLeftMargin(), 0.965, false, kBlack, 0.03);
  drawText("#bf{Debrecen} SiPM Test Setup @ #bf{Yale}", gPad->GetLeftMargin(), 0.935, false, kBlack, 0.03);
  drawText(Form("Hamamatsu #bf{%s}", Hamamatsu_SiPM_Code), 1-gPad->GetRightMargin(), 0.965, true, kBlack, 0.03);
  drawText(Form("Dark Current at %s", string_dark_current_types_long[below_breakdown]), 1-gPad->GetRightMargin(), 0.935, true, kBlack, 0.023);
  
  
  gCanvas_solo->SaveAs(Form("../plots/single_plots/correlations/IDark_corrl_Vbr%s_%s_%s.pdf",
                            string_dark_current_types[below_breakdown],
                            gReader->GetTrayStrings()->at(index_1).c_str(),
                            gReader->GetTrayStrings()->at(index_2).c_str()));
  
  return;
}// End of sipm_batch_summary_sheet::makeCorrelationDarkCurrent
