
#ifndef OVERWATCH_STDDEV_H
#define OVERWATCH_STDDEV_H

#include "object.h"

class StdDevTrending : public TrendingObject {

public:

    StdDevTrending(
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
        return {hist->GetStdDev(), hist->GetStdDevError()};
    }
};

#endif //OVERWATCH_STDDEV_H
