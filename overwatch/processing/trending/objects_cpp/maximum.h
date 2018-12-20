
#ifndef OVERWATCH_MAXIMUM_H
#define OVERWATCH_MAXIMUM_H

#include "object.h"

class MaximumTrending : public TrendingObject {

public:

    MaximumTrending(
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
        return {hist->GetMaximum(), 0};
    }

};

#endif //OVERWATCH_MAXIMUM_H
