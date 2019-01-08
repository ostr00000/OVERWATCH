import ROOT


def pythonStringListToVector(list):
    vector = ROOT.vector(str)()
    for s in list:
        vector.push_back(s)
    return vector
