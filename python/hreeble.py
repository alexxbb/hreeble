import random
from init_hrpyc import hou

shape = None
geo = None
old_prims = []

class Shape(object):
    def __init__(self, coords, prim, flip=False, **kwargs):
        self.coords = coords
        self.prim = prim
        self._num_coords = sum(map(len, self.coords))

    def pivot(self):
        pivot = hou.Vector2()
        for sub in self.coords:
            for pt in sub:
                pivot += pt
        pivot *= (1.0 / self._num_coords)
        return pivot

    def bbox(self):
        min_s = self.coords[0][0][0]
        max_s = 0.0
        min_t = self.coords[0][0][1]
        max_t = 0.0
        for elem in self.coords:
            for pt in elem:
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
            for j in range(len(self.coords[i])):
                _tmp = self.coords[i][j] + (self.coords[i][j] - pivot) * (val - 1)
                _tmp[0] = max(min(_tmp[0], 1.0), 0.0)
                _tmp[1] = max(min(_tmp[1], 1.0), 0.0)
                self.coords[i][j] = _tmp

    def move_by_vec(self, vec):
        for i in range(len(self.coords)):
            for j in range(len(self.coords[i])):
                self.coords[i][j] = self.coords[i][j] + vec

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
            offset *= 1.1
            self.move_by_vec(offset)

    def build_shape(self, height=0.1):
        def build_poly(*points):
            poly = geo.createPolygon()
            for pt in points:
                poly.addVertex(pt)

        global geo
        primN = self.prim.normal().normalized()
        for elem in self.coords:
            point_pairs = []
            top_points = []
            for pt in elem:
                pos_b = hou.Vector3(self.prim.attribValueAtInterior("P", pt[0], pt[1]))
                pos_t = pos_b + primN * height
                point_b = geo.createPoint()
                point_t = geo.createPoint()
                point_b.setPosition(pos_b)
                point_t.setPosition(pos_t)
                top_points.append(point_t)
                point_pairs.append((point_b, point_t))
            point_pairs.append(point_pairs[0])
            for i in range(len(elem)):
                build_poly(point_pairs[i][0],
                           point_pairs[i+1][0],
                           point_pairs[i+1][1],
                           point_pairs[i][1])
            build_poly(*top_points)


class StripeV(Shape):
    def __init__(self, prim, num=1, flip=False):
        subshape = [hou.Vector2(0.0, 0.0), hou.Vector2(0.0, 1.0),
                    hou.Vector2(0.1, 1.0), hou.Vector2(0.1, 0.0)]
        coords = [subshape]
        if num <= 3:
            for i in range(1, num):
                step = i/8.0
                sub = subshape[:]
                for j in range(len(sub)):
                    _tmp = hou.Vector2(sub[j])
                    _tmp[0] += step
                    sub[j] = _tmp
                coords.append(sub)
        super(StripeV, self).__init__(coords=coords, prim=prim)


class TShapeH(Shape):
    def __init__(self, prim, flip=False, **kwargs):
        coords = [[hou.Vector2(0.0, 0.0),
                  hou.Vector2(0.0, 0.33),
                  hou.Vector2(0.33, 0.33),
                  hou.Vector2(0.33, 0.66),
                  hou.Vector2(0.66, 0.66),
                  hou.Vector2(0.66, 0.33),
                  hou.Vector2(0.99, 0.33),
                  hou.Vector2(0.99, 0.0),
                  ]]
        if flip:
            for i in range(len(coords[0])):
                coords[0][i][1] = 1 - coords[0][i][1]
        super(TShapeH, self).__init__(coords=coords, prim=prim)


class TShapeV(Shape):
    def __init__(self, prim, flip=False, **kwargs):
        coords = [[hou.Vector2(0.0, 0.0),
                  hou.Vector2(0.0, 0.99),
                  hou.Vector2(0.33, 0.99),
                  hou.Vector2(0.33, 0.66),
                  hou.Vector2(0.66, 0.66),
                  hou.Vector2(0.66, 0.33),
                  hou.Vector2(0.33, 0.33),
                  hou.Vector2(0.33, 0.0),
                  ]]
        if flip:
            for i in range(len(coords[0])):
                coords[0][i][0] = 1 - coords[0][i][0]
        super(TShapeV, self).__init__(coords=coords, prim=prim)


def extrude_prim(prim, height, inset=0.1):
    global old_prims
    def build_prim(*points):
        poly = geo.createPolygon()
        for pt in points:
            poly.addVertex(pt)
        return poly
    primN = prim.normal()
    primP = hou.Vector3(prim.attribValueAtInterior("P", 0.5, 0.5))
    center = primP + primN * height
    point_pairs = []
    for vt in prim.vertices():
        new_pt = geo.createPoint()
        new_pt_pos = vt.point().position() + primN * height
        inset_dir = (center - new_pt_pos).normalized()
        new_pt_pos += inset_dir * inset
        new_pt.setPosition(new_pt_pos)
        point_pairs.append((vt.point(), new_pt))
    point_pairs.append(point_pairs[0])
    top_prim = build_prim(point_pairs[0][1], point_pairs[1][1], point_pairs[2][1], point_pairs[3][1])
    for i in range(len(prim.vertices())):
        build_prim(point_pairs[i][0],
                   point_pairs[i+1][0],
                   point_pairs[i+1][1],
                   point_pairs[i][1])
    old_prims.append(prim)
    return top_prim


def split_prim(geo, prim, dir=None):
    global old_prims
    old_prims.append(prim)
    def build_poly(*points):
        poly = geo.createPolygon()
        for pt in points:
            poly.addVertex(pt)
        return poly

    points = [vt.point() for vt in prim.vertices()]
    tvec = points[1].position() - points[0].position()
    svec = points[3].position() - points[0].position()

    if dir is None:
        dir = random.randint(0, 1)
    new_pt0 = geo.createPoint()
    new_pt1 = geo.createPoint()
    if dir == 0:
        new_pt0.setPosition(points[0].position() + svec * 0.5)
        new_pt1.setPosition(points[1].position() + svec * 0.5)
        poly1 = build_poly(points[0], points[1], new_pt1, new_pt0)
        poly2 = build_poly(new_pt0, new_pt1, points[2], points[3])
    else:
        new_pt0.setPosition(points[0].position() + tvec * 0.5)
        new_pt1.setPosition(points[3].position() + tvec * 0.5)
        poly1 = build_poly(points[0], new_pt0, new_pt1, points[3])
        poly2 = build_poly(new_pt0, points[1], points[2], new_pt1)
    return poly1, poly2

def divide(geo, prim):
    result = []
    direction = random.randint(0, 1)
    t = split_prim(geo, prim, direction)
    prim_to_split = t[0]
    result.append(t[1])
    if random.random() > 0.5:
        prim_to_split = t[1]
        result[0] = t[0]
    result.extend(split_prim(geo, prim_to_split, 1 - direction))
    return result


def main():
    global old_prims
    global shape
    global geo
    node = hou.pwd()
    geo = node.geometry()
    size = node.evalParmTuple("size")
    seed = node.evalParm("seed")
    density = node.evalParm("density")
    height = node.evalParmTuple("height")

    types = [TShapeV, TShapeH, StripeV]
    for orig_prim in geo.prims():
        splitted_prims = divide(geo, orig_prim)
        for splitted_prim in splitted_prims:
            top_prim = extrude_prim(splitted_prim, hou.hmath.fit01(random.random(), 0.05, 0.2), 0.02)
            for i in range(density):
                random.seed(seed + top_prim.number() + i)
                shape_type = random.choice(types)
                shape = shape_type(top_prim, flip=random.choice((True, False)), num=random.randint(1, 3))
                shape.move_to_pos(hou.Vector2(random.random(), random.random()))
                shape.scale(hou.hmath.fit01(hou.hmath.rand(i), size[0], size[1]))
                shape.place()
                shape.build_shape(hou.hmath.fit01(random.random(), height[0], height[1]))

    geo.deletePrims(old_prims, False)


if __name__ == '__main__':
    node = hou.node("/obj/detail/placement")
    # node = hou.node("/obj/test/split_prim")
    node.cook(True)
