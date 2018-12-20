
#ifndef OVERWATCH_MEAN_H
#define OVERWATCH_MEAN_H

#include "object.h"

class MeanTrending : public TrendingObject {

public:

    MeanTrending(
            std::string name,
            std::string desc,
            std::vector<std::string> histogramNames,
            std::string subsystemName,
            int maxEntries,
            std::string directoryPrefix
    ) : TrendingObject(name,
                       desc,
                       histogramNames,
                       subsystemName,
                       maxEntries,
                       directoryPrefix) {}


    std::pair<float, float> getStatistic(TH1 *hist) {
        return {hist->GetMean(), hist->GetMeanError()};
    }
};

#endif //OVERWATCH_MEAN_H
