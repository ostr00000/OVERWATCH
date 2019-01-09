# C++ interface for trending

This module allows user to implement trending objects in C++. 

Object has to extend TrendingObject class localized in `object.h`, and override `getgetStatistic` function.

`getStatistic` function should return pair of floats, where first element statistic value represents and second its error.

Then you have to convert C++, object to its Python interface. This should look as presented below.

```python
ROOT.gInterpreter.ProcessLine('#include "overwatch/processing/trending/objects_cpp/stdDev.h"')
StdDevTrending = ROOT.StdDevTrending
```