import ROOT

ROOT.gInterpreter.ProcessLine('#include "mean.h"')

# b = ROOT.MeanTrending()
# print b.get()

print ROOT.dd(ROOT.vector(str)())
