
#ifndef OVERWATCH_OBJECT_H
#define OVERWATCH_OBJECT_H

#include <TStyle.h>
#include <TH1.h>
#include <TGraphErrors.h>
#include <TCanvas.h>
#include <fstream>

class TrendingObject {

public:

    std::string name;
    std::string desc;
    std::vector<std::string> histogramNames;
    std::string subsystemName;
    std::string directoryPrefix;
    int currentEntry = 0;
    int maxEntries;
    std::vector<std::pair<float, float> > trendedValues;
    std::string drawOptions = "AP";


    TrendingObject(
            std::string name,
            std::string desc,
            std::vector<std::string> histogramNames,
            std::string subsystemName,
            int maxEntries,
            std::string directoryPrefix
    ) :
            name(name),
            desc(desc),
            histogramNames(histogramNames),
            subsystemName(subsystemName),
            directoryPrefix(directoryPrefix),
            maxEntries(maxEntries) {}

    virtual std::pair<float, float> getStatistic(TH1 *hist) = 0;

    virtual void extractTrendValue(TH1 *hist) {
        if (currentEntry > maxEntries) {
            trendedValues.erase(trendedValues.begin());
        } else {
            currentEntry++;
        }
        std::pair<float, float> newValue = getStatistic(hist);
        trendedValues.push_back(newValue);
    }

    virtual TGraphErrors *retrieveHist() {
        auto histogram = new TGraphErrors(maxEntries);
        histogram->SetName(name.c_str());
        histogram->GetXaxis()->SetTimeDisplay(true);
        histogram->SetTitle(desc.c_str());
        histogram->SetMarkerStyle(kFullCircle);
        for (int i = 0; i < trendedValues.size(); i++) {
            histogram->SetPoint(i, i, trendedValues[i].first);
            histogram->SetPointError(i, 0, trendedValues[i].second);
        }
        return histogram;
    }

    virtual void processHist(TCanvas *canvas) {
        resetCanvas(canvas);
        canvas->cd();
        gStyle->SetOptTitle(false);
        auto histogram = retrieveHist();
        histogram->Draw(drawOptions.c_str());

        auto outputName = name;
        std::replace(outputName.begin(), outputName.end(), '/', '_');
        auto imgFile = directoryPrefix + "/trending/" + subsystemName + "/img/" + outputName + ".png";
        auto jsonFile = directoryPrefix + "/trending/" + subsystemName + "/json/" + outputName + ".json";
        canvas->SaveAs(imgFile.c_str());

        std::ofstream file;
        file.open(jsonFile);
        file << TBufferJSON().ConvertToJSON(canvas).Data();
        file.close();
    }

    void resetCanvas(TCanvas *canvas) {
        canvas->Clear();
        canvas->SetLogx(false);
        canvas->SetLogy(false);
        canvas->SetLogz(false);
    }
};

#endif //OVERWATCH_OBJECT_H
