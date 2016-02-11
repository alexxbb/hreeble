#include "Element.h"
#include <GEO/GEO_PrimPoly.h>
#include <UT/UT_Pair.h>

typedef UT_Vector2R V2R;
typedef UT_Pair<GA_Offset, GA_Offset> OffsetPair;

Element::Element()
{
}

Element::Element(SubElem elem)
{
	subelements.append(elem);
}


Element::~Element()
{
}


BBox2D Element::bbox()
{
	fpreal min_s = subelements(0).coords(0).x();
	fpreal max_s = 0.0;
	fpreal min_t = subelements(0).coords(0).y();
	fpreal max_t = 0.0;

	for (const auto &subelem : subelements) {
		for (const auto &pt : subelem.coords) {
			if (pt.x() > max_s)
				max_s = pt.x();
			else if (pt.x() < min_s)
				min_s = pt.x();

			if (pt.y() > max_t)
				max_t = pt.y();
			else if (pt.y() < min_t)
				min_t = pt.y();
	}
	}
	BBox2D bbox = { UT_Vector2R(min_s, min_t), UT_Vector2R(max_s, max_t) };
	return bbox;
}


UT_Vector2R Element::pivot()
{
	UT_Vector2R pivot(0.0, 0.0);
	for (const auto &elem: subelements) {
		for (const auto &pt : elem.coords) {
			pivot += pt;
		}
	}
	pivot /= num_points();
	return pivot;
}


UT_Vector2R Element::bounds_intersection()
{
	auto sign = [](const fpreal val) { return val > 0 ? 1 : 0; };
	auto outside = [](fpreal val) { return val > 1.0 || val < 0.0; };

	V2R offset(0.0, 0.0);
	BBox2D bbox = this->bbox();
	if (outside(bbox.minvec.x())) {
		offset += V2R(sign(bbox.minvec.x()) - bbox.minvec.x(), 0.0);
	}
	if (outside(bbox.minvec.y())) {
		offset += V2R(0.0, sign(bbox.minvec.y()) - bbox.minvec.y());
	}

	if (outside(bbox.maxvec.x())) {
		offset += V2R(sign(bbox.maxvec.x()) - bbox.maxvec.x(), 0.0);
	}
	if (outside(bbox.maxvec.y())) {
		offset += V2R(0.0, sign(bbox.maxvec.y()) - bbox.maxvec.y());

	}
	return offset;
}


void Element::append(SubElem elem)
{
	subelements.append(elem);
}


void Element::move_by_vec(const UT_Vector2R & vec)
{
	for (auto &subelem : subelements) {
		for (auto &pt : subelem.coords) {
			pt += vec;
		}
	}
}


void Element::transform(const UT_Vector2R & new_pos, const fpreal & scale)
{
	// Move
	auto vec = new_pos - this->pivot();
	move_by_vec(vec);
	UT_Vector2R pivot = this->pivot();
	// Scale
	for (auto &subelem : subelements) {
		for (auto &pt : subelem.coords) {
			pt += (pt - pivot) * (scale - 1);
		}
	}
	// Place
	V2R offset = bounds_intersection();
	if (offset.length() != 0) {
		offset *= 1.1;
		move_by_vec(offset);
	}
	if (bounds_intersection().length() != 0) {
		for (auto &subelem : subelements) {
			for (auto &pt : subelem.coords) {
				pt(0) = SYSmax(SYSmin(pt.x(), 1.0), 0.0);
				pt(1) = SYSmax(SYSmin(pt.y(), 1.0), 0.0);
			}
		}
	}
}


void Element::build(GU_Detail *gdp, const GEO_Primitive *prim, const UT_Vector3 &primN, const fpreal & height)
{
	auto build_prim = [gdp](const GA_OffsetArray &points) -> GEO_Primitive*{
		auto new_prim = static_cast<GEO_PrimPoly*>(gdp->appendPrimitive(GA_PRIMPOLY));
		for (const auto pt_off : points) {
			new_prim->appendVertex(pt_off);
		}
		new_prim->close();
		return static_cast<GEO_Primitive*>(new_prim);
	};

	GA_RWHandleV3 ph = gdp->getP();
	for (const auto &subelem : subelements) {
		GA_Offset offset_block = gdp->appendPointBlock(subelem.coords.entries() * 2);
		GA_OffsetArray front_offsets;
		UT_ValArray<UT_Pair<GA_Offset, GA_Offset>> pairs;
		for (exint i = 0; i < subelem.coords.entries(); i++) {
			const UT_Vector2R &coord = subelem.coords(i);
			UT_Vector4 pos_b;
			UT_Vector3 pos_t;

			GA_Offset pt_off1 = offset_block + i;
			GA_Offset pt_off2 = offset_block + i + 4;
			prim->evaluateInteriorPoint(pos_b, coord.x(), coord.y());
			pos_t = pos_b + primN * height;

			ph.set(pt_off1, pos_b);
			ph.set(pt_off2, pos_t);
			front_offsets.append(pt_off2);
			pairs.append(OffsetPair(pt_off1, pt_off2));

		}
		pairs.append(pairs(0));
		GA_OffsetArray prim_points;
		for (GA_Size i = 0; i < subelem.coords.entries(); i++){
			OffsetPair pair1 = pairs(i);
			OffsetPair pair2 = pairs(i + 1);
			prim_points.clear();
			prim_points.append(pair1.myFirst);
			prim_points.append(pair2.myFirst);
			prim_points.append(pair2.mySecond);
			prim_points.append(pair1.mySecond);
			build_prim(prim_points);
		}
		auto front_prim = build_prim(front_offsets);
		front_offsets.clear();
		pairs.clear();
	}
}


exint Element::num_points()
{
	exint num = 0;
	for (const auto &elem : subelements) {
		num += elem.coords.entries();
	}
	return num;
}


std::unique_ptr<Element> make_element(const ElementTypes &elem_type) 
{
	std::unique_ptr<Element> w(new Element);
	uint num_elems = 1;
	if (elem_type <= ElementTypes::STRIPEV3)
	{
		switch (elem_type)
		{
		case ElementTypes::STRIPEV2:
			num_elems = 2;
			break;
		case ElementTypes:: STRIPEV3:
			num_elems = 3;
			break;
		default:
			break;
		}

		for (uint i = 0; i < num_elems; i++) {
			auto elem = SubElem();
			fpreal step = i / 8.0;
			elem.coords.append(UT_Vector2R(step, 0.0));
			elem.coords.append(UT_Vector2R(step, 1.0));
			elem.coords.append(UT_Vector2R(0.1 + step, 1.0));
			elem.coords.append(UT_Vector2R(0.1 + step, 0.0));
			w->append(elem);
		}
	}
	else if (elem_type == ElementTypes::STRIPEV)
	{
	}
	return w;
}
