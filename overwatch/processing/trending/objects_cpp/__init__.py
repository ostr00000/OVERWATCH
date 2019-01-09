import ROOT

ROOT.gInterpreter.ProcessLine('#include "overwatch/processing/trending/objects_cpp/mean.h"')
MeanTrending = ROOT.MeanTrending

ROOT.gInterpreter.ProcessLine('#include "overwatch/processing/trending/objects_cpp/maximum.h"')
MaximumTrending = ROOT.MaximumTrending

ROOT.gInterpreter.ProcessLine('#include "overwatch/processing/trending/objects_cpp/stdDev.h"')
StdDevTrending = ROOT.StdDevTrending
