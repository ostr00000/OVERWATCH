#!/usr/bin/env python

""" This package provides the implementation of individual trending objects.

"""

from .mean import MeanTrending  # noqa
from .stdDev import StdDevTrending  # noqa
from .maximum import MaximumTrending  # noqa

# import ROOT
# ROOT.gInterpreter.ProcessLine('#include "overwatch/processing/trending/objects_cpp/mean.h"')
# MeanTrending = ROOT.MeanTrending
#
# ROOT.gInterpreter.ProcessLine('#include "overwatch/processing/trending/objects_cpp/maximum.h"')
# MaximumTrending = ROOT.MaximumTrending
#
# ROOT.gInterpreter.ProcessLine('#include "overwatch/processing/trending/objects_cpp/stdDev.h"')
# StdDevTrending = ROOT.StdDevTrending
