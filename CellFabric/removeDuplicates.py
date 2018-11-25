
import json
import pytest


class Scanline:
    def __init__(self, proto, indices, kk):
        self.proto = proto
        self.indices = indices
        self.kk = kk
        self.rects = []
        self.clear()

    def clear(self):
        self.start = None
        self.end = None
        self.currentNet = None

    def isEmpty(self):
        return self.start is None

    def emit(self):
        r = self.proto[:]
        r[self.kk] = self.start
        r[self.kk+2] = self.end
        self.rects.append((r, self.currentNet))

    def set(self, rect, netName):
        self.start = rect[self.kk]
        self.end = rect[self.kk+2]
        self.currentNet = netName


def removeDuplicates(data):

    layers = [('poly', 'v'), ('ndiff', 'h'), ('pdiff', 'h'), ('polycon', 'h'),
              ('diffcon', 'v'), ('metal0', 'h'), ('metal1', 'v'), ('metal2', 'h')]

    hLayers = {layer for (layer, dir) in layers if dir == 'h'}
    vLayers = {layer for (layer, dir) in layers if dir == 'v'}

    indicesTbl = {'h': ([1, 3], 0), 'v': ([0, 2], 1)}

    tbl = {}

    for d in data['terminals']:
        layer = d['layer']
        rect = d['rect']
        netName = d['netName']

        if layer in hLayers:
            c2 = rect[1]+rect[3]
        elif layer in vLayers:
            c2 = rect[0]+rect[2]
        else:
            assert layer in hLayers or layer in vLayers, layer

        if layer not in tbl:
            tbl[layer] = {}
        if c2 not in tbl[layer]:
            tbl[layer][c2] = []

        tbl[layer][c2].append((rect, netName))

    terminals = []

    for (layer, dir) in layers:
        if layer not in tbl:
            continue
        (indices, kk) = indicesTbl[dir]

        for (c2, v) in tbl[layer].items():

            sl = Scanline(v[0][0], indices, kk)

            if v:
                (rect0, _) = v[0]
                for (rect, netName) in v[1:]:
                    assert all(rect[i] == rect0[i] for i in indices)

                s = sorted(v, key=lambda p: p[0][kk])

                for (rect, netName) in s:
                    if sl.isEmpty():
                        sl.set(rect, netName)
                    elif rect[kk] <= sl.end:  # continue
                        sl.end = max(sl.end, rect[kk+2])
                        assert sl.currentNet == netName, (
                            layer, sl.currentNet, netName)
                    else:  # gap
                        sl.emit()
                        sl.set(rect, netName)

                if not sl.isEmpty():
                    sl.emit()
                    sl.clear()


#        print( layer, c2, len(v), len(sl.rects))

            for (rect, netName) in sl.rects:
                terminals.append(
                    {'layer': layer, 'netName': netName, 'rect': rect})

    return {'bbox': data['bbox'], 'terminals': terminals, 'globalRoutes': data['globalRoutes'], 'globalRouteGrid': data['globalRouteGrid']}


def test_vertical():
    terminals = [{'layer': 'metal1', 'netName': 'x', 'rect': [0, 0, 100, 300]}]
    data = {"bbox": [0, 100, 0, 100], "terminals": terminals,
            "globalRoutes": [], "globalRouteGrid": []}
    newData = removeDuplicates(data)


def test_horizontal():
    terminals = [{'layer': 'metal2', 'netName': 'x', 'rect': [0, 0, 300, 100]}]
    data = {"bbox": [0, 100, 0, 100], "terminals": terminals,
            "globalRoutes": [], "globalRouteGrid": []}
    newData = removeDuplicates(data)


def test_different_widths():
    terminals = [{'layer': 'metal2', 'netName': 'x', 'rect': [0, -50, 300, 50]},
                 {'layer': 'metal2', 'netName': 'x', 'rect': [0, -60, 300, 60]}]
    data = {"bbox": [0, 100, 0, 100], "terminals": terminals,
            "globalRoutes": [], "globalRouteGrid": []}
    with pytest.raises(AssertionError):
        newData = removeDuplicates(data)


def test_bad_layer():
    terminals = [{'layer': 'metal14',
                  'netName': 'x', 'rect': [0, -50, 300, 50]}]
    data = {"bbox": [0, 100, 0, 100], "terminals": terminals,
            "globalRoutes": [], "globalRouteGrid": []}
    with pytest.raises(AssertionError):
        newData = removeDuplicates(data)


def test_overlapping():
    terminals = [{'layer': 'metal2', 'netName': 'x', 'rect': [0, -50, 300, 50]},
                 {'layer': 'metal2', 'netName': 'x', 'rect': [200, -50, 600, 50]}]
    data = {"bbox": [0, -50, 600, 50], "terminals": terminals,
            "globalRoutes": [], "globalRouteGrid": []}
    newData = removeDuplicates(data)
    assert len(newData['terminals']) == 1
    assert newData['terminals'][0]['rect'] == [0, -50, 600, 50]


def test_short():
    terminals = [{'layer': 'metal2', 'netName': 'x', 'rect': [0, -50, 300, 50]},
                 {'layer': 'metal2', 'netName': 'y', 'rect': [200, -50, 600, 50]}]
    data = {"bbox": [0, -50, 600, 50], "terminals": terminals,
            "globalRoutes": [], "globalRouteGrid": []}
    with pytest.raises(AssertionError):
        newData = removeDuplicates(data)


def test_underlapping():
    terminals = [{'layer': 'metal2', 'netName': 'x', 'rect': [0, -50, 300, 50]},
                 {'layer': 'metal2', 'netName': 'x', 'rect': [100, -50, 200, 50]}]
    data = {"bbox": [0, -50, 300, 50], "terminals": terminals,
            "globalRoutes": [], "globalRouteGrid": []}
    newData = removeDuplicates(data)
    assert len(newData['terminals']) == 1
    assert newData['terminals'][0]['rect'] == [0, -50, 300, 50]


def test_disjoint():
    terminals = [{'layer': 'metal2', 'netName': 'x', 'rect': [0, -50, 300, 50]},
                 {'layer': 'metal2', 'netName': 'x', 'rect': [400, -50, 600, 50]}]
    data = {"bbox": [0, -50, 600, 50], "terminals": terminals,
            "globalRoutes": [], "globalRouteGrid": []}
    newData = removeDuplicates(data)
    assert len(newData['terminals']) == 2
    assert newData['terminals'][0]['rect'] == [0, -50, 300, 50]
    assert newData['terminals'][1]['rect'] == [400, -50, 600, 50]


if __name__ == "__main__":
    with open("mydesign_dr_globalrouting.json", "rt") as fp:
        data = json.load(fp)

    newData = removeDuplicates(data)

    with open("mydesign_dr_globalrouting.json", "wt") as fp:
        json.dump(newData, fp, indent=2)