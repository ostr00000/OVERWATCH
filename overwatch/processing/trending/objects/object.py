import abc
import logging
import os
import sys

import ROOT
from persistent import Persistent

import overwatch.processing.trending.constants as CON

# https://stackoverflow.com/questions/35673474/using-abc-abcmeta-in-a-way-it-is-compatible-both-with-python-2-7-and-python-3-5/41622155#41622155
if sys.version_info >= (3, 4):
    ABC = abc.ABC
else:
    ABC = abc.ABCMeta('ABC', (object,), {'__slots__': ()})

try:
    from typing import *  # noqa
except ImportError:
    pass
else:
    from overwatch.processing.processingClasses import histogramContainer  # noqa

logger = logging.getLogger(__name__)


class TrendingObject(ABC, Persistent):

    def __init__(self, name, description, histogramNames, subsystemName, parameters):
        # type: (str, str, list, str, dict) -> None
        self.name = name
        self.desc = description
        self.histogramNames = histogramNames
        self.subsystemName = subsystemName
        self.parameters = parameters

        self.currentEntry = 0
        self.maxEntries = self.parameters.get(CON.ENTRIES, 100)
        self.trendedValues = self.initializeTrendingArray()

        self.histogram = None
        # Ensure that the axis and points are drawn on the TGraph
        self.drawOptions = 'AP'

    def __str__(self):
        return self.name

    @abc.abstractmethod
    def initializeTrendingArray(self):  # type: () -> Any
        """Example:
        return np.zeros((self.maxEntries, 2), dtype=np.float)
        """
        pass

    @abc.abstractmethod
    def extractTrendValue(self, hist):  # type: (histogramContainer) -> None
        """Example:
        if self.currentEntry > self.maxEntries:
            self.trendedValues = np.delete(self.trendedValues, 0, axis=0)
        else:
            self.currentEntry += 1

        newValue = hist.hist.GetMean(), hist.hist.GetMeanError()
        self.trendedValues = np.append(self.trendedValues, [newValue], axis=0)
        """
        pass

    @abc.abstractmethod
    def retrieveHist(self):  # type: () -> ROOT.TObject
        """Example:
        histogram = ROOT.TGraphErrors(self.maxEntries)
        histogram.SetName(self.name)
        histogram.GetXaxis().SetTimeDisplay(True)
        histogram.SetTitle(self.desc)
        histogram.SetMarkerStyle(ROOT.kFullCircle)

        for i in range(len(self.trendedValues)):
            histogram.SetPoint(i, i, self.trendedValues[i, 0])
            histogram.SetPointError(i, 0, self.trendedValues[i, 1])

        return histogram
        """
        pass

    def processHist(self, canvas):
        self.resetCanvas(canvas)
        # Ensure we plot onto the right canvas
        canvas.cd()

        ROOT.gStyle.SetOptTitle(False)  # turn off title
        self.histogram = self.retrieveHist()
        self.histogram.Draw(self.drawOptions)

        # Replace any slashes with underscores to ensure that it can be used safely as a filename
        outputNameWithoutExt = self.name.replace("/", "_") + '.{}'
        outputPath = os.path.join(self.parameters[CON.DIR_PREFIX], CON.TRENDING,
                                  self.subsystemName, '{}', outputNameWithoutExt)
        imgFile = outputPath.format(CON.IMAGE, self.parameters[CON.EXTENSION])
        jsonFile = outputPath.format(CON.JSON, 'json')

        logger.debug("Saving hist to {}".format(imgFile))
        canvas.SaveAs(imgFile)

        with open(jsonFile, "wb") as f:
            f.write(ROOT.TBufferJSON.ConvertToJSON(canvas).Data().encode())

    @staticmethod
    def resetCanvas(canvas):
        canvas.Clear()
        # Reset log status, since clear does not do this
        canvas.SetLogx(False)
        canvas.SetLogy(False)
        canvas.SetLogz(False)
