import random
from init_hrpyc import hou


class Shape(object):
    def __init__(self, coords, prim, flip=False):
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

def build_prim(pt0, pt1, pt2, pt3):
    poly = geo.createPolygon()
    poly.addVertex(pt0)
    poly.addVertex(pt1)
    poly.addVertex(pt2)
    poly.addVertex(pt3)
    return poly

def extrude_prim(prim, height, inset=0.1):
    primN = prim.normal()
    primP = hou.Vector3(prim.attribValueAtInterior("P", 0.5, 0.5))
    center = primP + primN * height
    pairs = []
    for vt in prim.vertices():
        new_pt = geo.createPoint()
        new_pt_pos = vt.point().position() + primN * height
        inset_dir = (center - new_pt_pos).normalized()
        new_pt_pos += inset_dir * inset
        new_pt.setPosition(new_pt_pos)
        pairs.append((vt.point(), new_pt))
    build_prim(pairs[0][1], pairs[1][1], pairs[2][1], pairs[3][1])
    for i in range(3):
        build_prim(pairs[i][0], pairs[i+1][0], pairs[i+1][1], pairs[i][1])
    build_prim(pairs[-1][0], pairs[0][0], pairs[0][1], pairs[-1][1])
    old_prims.append(prim)

class Stripe(Shape):
    def __init__(self, prim, flip=False):
        coords = [hou.Vector2(0.0, 0.0), hou.Vector2(0.0, 0.6),
                  hou.Vector2(0.1, 0.6), hou.Vector2(0.1, 0.0)]
        super(Stripe, self).__init__(coords=coords, prim=prim)


class TShapeH(Shape):
    def __init__(self, prim, flip=False):
        coords = [hou.Vector2(0.0, 0.0),
                  hou.Vector2(0.0, 0.33),
                  hou.Vector2(0.33, 0.33),
                  hou.Vector2(0.33, 0.66),
                  hou.Vector2(0.66, 0.66),
                  hou.Vector2(0.66, 0.33),
                  hou.Vector2(0.99, 0.33),
                  hou.Vector2(0.99, 0.0),
                  ]
        if flip:
            for i in range(len(coords)):
                coords[i][1] = 1 - coords[i][1]
        super(TShapeH, self).__init__(coords=coords, prim=prim)


class TShapeV(Shape):
    def __init__(self, prim, flip=False):
        coords = [hou.Vector2(0.0, 0.0),
                  hou.Vector2(0.0, 0.99),
                  hou.Vector2(0.33, 0.99),
                  hou.Vector2(0.33, 0.66),
                  hou.Vector2(0.66, 0.66),
                  hou.Vector2(0.66, 0.33),
                  hou.Vector2(0.33, 0.33),
                  hou.Vector2(0.33, 0.0),
                  ]
        if flip:
            for i in range(len(coords)):
                coords[i][0] = 1 - coords[i][0]
        super(TShapeV, self).__init__(coords=coords, prim=prim)


shape = None
geo = None


def main():
    global shape
    global geo
    node = hou.pwd()
    geo = node.geometry()
    geo.addAttrib(hou.attribType.Point, "st", (0.0, 0.0))
    scale = node.evalParm("scale")
    seed = node.evalParm("seed")

    kill = []
    types = [TShapeV, TShapeH, Stripe]
    for prim in geo.prims():
        random.seed(seed + prim.number())
        for i in range(10):
            shape_type = random.choice(types)
            shape = shape_type(prim, flip=random.choice((True, False)))
            shape.move_to_pos(hou.Vector2(random.random(), random.random()))
            shape.scale(hou.hmath.fit01(hou.hmath.rand(i), 0.05, 0.5))
            shape.place()
            shape.build_poly()
        kill.append(prim)

    geo.deletePrims(kill, True)


if __name__ == '__main__':
    node = hou.node("/obj/detail/placement")
    node.cook(True)
