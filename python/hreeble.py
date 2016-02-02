import random
from init_hrpyc import hou


class Shape(object):
    def __init__(self, coords, prim):
        self.coords = coords
        self.prim = prim

    def pivot(self):
        pivot = hou.Vector2()
        for pt in self.coords:
            pivot += pt
        pivot *= (1.0 / len(self.coords))
        return pivot

    def bbox(self):
        min_s = self.coords[0][0]
        max_s = 0.0
        min_t = self.coords[0][1]
        max_t = 0.0
        for pt in self.coords:
            if pt[0] > max_s:
                max_s = pt[0]
            elif pt[0] < min_s:
                min_s = pt[0]

            if pt[1] > max_t:
                max_t = pt[1]
            elif pt[1] < min_t:
                min_t = pt[1]

        return hou.Vector2(min_s, min_t), hou.Vector2(max_s, max_t)


    def scale(self, val):
        pivot = self.pivot()
        for i in range(len(self.coords)):
            _tmp = self.coords[i] + (self.coords[i] - pivot) * (val - 1)
            # _tmp[0] = max(0.0, min(_tmp[0], 1.0))
            # _tmp[1] = max(0.0, min(_tmp[1], 1.0))
            self.coords[i] = _tmp

    def move_by_vec(self, vec):
        for i in range(len(self.coords)):
            self.coords[i] = self.coords[i] + vec

    def move_to_pos(self, new_pos):
        vec = new_pos - self.pivot()
        self.move_by_vec(vec)

    def place(self):
        offset = hou.Vector2(0.0, 0.0)
        sign = lambda x: int(x > 0)
        bbox_min, bbox_max = self.bbox()
        if bbox_min[0] > 1.0 or bbox_min[0] < 0.0:
            offset += hou.Vector2((sign(bbox_min[0]) - bbox_min[0]), 0.0)
        if bbox_min[1] > 1.0 or bbox_min[1] < 0.0:
            offset += hou.Vector2(0.0, (sign(bbox_min[1]) - bbox_min[1]))

        if bbox_max[0] > 1.0 or bbox_max[0] < 0.0:
            offset += hou.Vector2((sign(bbox_max[0]) - bbox_max[0]), 0.0)
        if bbox_max[1] > 1.0 or bbox_max[1] < 0.0:
            offset += hou.Vector2(0.0, (sign(bbox_max[1]) - bbox_max[1]))

        if offset.length():
            self.move_by_vec(offset)

    def build_poly(self):
        global geo
        poly = geo.createPolygon()
        for pt in self.coords:
            p = hou.Vector3(self.prim.attribValueAtInterior("P", pt[0], pt[1]))
            new_point = geo.createPoint()
            new_point.setPosition(p)
            new_point.setAttribValue("st", (round(pt[0], 2), round(pt[1], 2)))
            poly.addVertex(new_point)


shape = None
geo = None


def main():
    global shape
    global geo
    node = hou.pwd()
    geo = node.geometry()
    geo.addAttrib(hou.attribType.Point, "st", (0.0, 0.0))
    prim = geo.prims()[0]
    scale = node.evalParm("scale")

    points = [hou.Vector2(0.0, 0.0), hou.Vector2(0.0, 0.6),
              hou.Vector2(0.1, 0.6), hou.Vector2(0.1, 0.0)]

    kill = []
    for prim in geo.prims():
        for i in range(1):
            shape = Shape(points[:], prim)
            shape.move_to_pos(hou.Vector2(random.random(), random.random()))
            shape.scale(hou.hmath.fit01(hou.hmath.rand(i), 0.2, 0.6))
            shape.place()
            shape.build_poly()
        kill.append(prim)

    geo.deletePrims(kill, True)


if __name__ == '__main__':
    node = hou.node("/obj/detail/placement")
    node.cook(True)
