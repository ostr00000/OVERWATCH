#!/usr/bin/env python
""" Simple container of parameter for TrendingObject.

.. code-author: Pawel Ostrowski <ostr000@interia.pl>, AGH University of Science and Technology
"""
import past.builtins

from overwatch.base import config
from overwatch.processing.alarms.alarm import Alarm
# from overwatch.processing.trending.objects.object import TrendingObject
from overwatch.processing.trending.objects_cpp.utilities import pythonStringListToVector

(databaseParameters, _) = config.readConfig(config.configurationType.database)

try:
    from typing import *  # noqa
except ImportError:
    pass

basestring = past.builtins.basestring
import overwatch.processing.trending.constants as CON


class TrendingInfoException(Exception):
    def __init__(self, **kwargs):
        self.kwargs = kwargs

    def __str__(self):
        return ', '.join('{}:{}'.format(k, v) for k, v in self.kwargs.items())


class TrendingInfo:
    """ Container for data for TrendingObject

    When TrendingInfo is initialized, data is validated.
    """

    __slots__ = ['name', 'desc', 'histogramNames', 'trendingClass', '_alarms']

    def __init__(self, name, desc, histogramNames, trendingClass):
        """
        Args:
            name (str): using in database to map name to trendingObject, must be unique
            desc (str): verbose description of trendingObject, it is displayed on generated histograms
            histogramNames (list): list of histogram names from which trendingObject depends
            trendingClass: concrete class of abstract class TrendingObject
        """
        # type: (str, str, List[str],  Type[TrendingObject]) -> None
        # trending objects within subsystem must have different names
        self.name = self._validate(name)
        self.desc = self._validate(desc)
        self.histogramNames = self._validateHist(histogramNames)
        self.trendingClass = self._validateTrendingClass(trendingClass)

        self._alarms = []

    def addAlarm(self, alarms):  # type: (Union(Alarm, List[Alarm])) -> None
        if not isinstance(alarms, list):
            alarms = [alarms]
        for alarm in alarms:
            if isinstance(alarm, Alarm):
                self._alarms.append(alarm)
            else:
                raise TrendingInfoException(msg='WrongAlarmType')

    def createTrendingClass(self, subsystemName, parameters):  # type: (str, dict) -> TrendingObject
        """Create instance of TrendingObject from previously set parameters
        Returns:
            TrendingObject: newly created object
        """

        histogramNames = pythonStringListToVector(self.histogramNames)

        trend = self.trendingClass(self.name, self.desc, histogramNames, subsystemName,
                                   parameters.get(CON.ENTRIES, 100),
                                   parameters[CON.DIR_PREFIX])
        trend.setAlarms(self._alarms)
        return trend

    @staticmethod
    def _validate(obj):  # type: (str) -> str
        if not isinstance(obj, basestring):
            raise TrendingInfoException(msg='WrongType', expected=basestring, got=type(obj))
        return obj

    @classmethod
    def _validateHist(cls, objects):  # type: (Collection[str]) -> Collection[str]
        try:
            if len(objects) < 1:
                raise TrendingInfoException(msg='NoHistograms')
        except TypeError:
            raise TrendingInfoException(msg='NotCollection', got=objects)

        for obj in objects:
            cls._validate(obj)
        return objects

    @staticmethod
    def _validateTrendingClass(cls):  # type: (Any) -> Type[TrendingObject]
        # if not issubclass(cls, TrendingObject):
        #     raise TrendingInfoException(msg='WrongTrendingClass', expected=TrendingObject, got=cls)
        return cls
