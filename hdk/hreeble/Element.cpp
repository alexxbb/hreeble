#include "Element.h"



Element::Element()
{
}


Element::~Element()
{
}


BBox2D Element::bbox()
{
	fpreal min_s = coords(0).x();
	fpreal max_s = 0.0;
	fpreal min_t = coords(0).y();
	fpreal max_t = 0.0;

	for (auto pt : coords) {
		if (pt.x() > max_s)
			max_s = pt.x();
		else if (pt.x() < min_s)
			min_s = pt.x();

		if (pt.y() > max_t)
			max_t = pt.y();
		else if (pt.y() < min_t)
			min_t = pt.y();
	}
	BBox2D bbox = { UT_Vector2R(min_s, min_t), UT_Vector2R(max_s, max_t) };
	return bbox;
}


UT_Vector2R Element::pivot()
{
	UT_Vector2R pivot(0.0, 0.0);
	for (const auto pt: coords) {
		pivot += pt;
	}
	pivot /= coords.entries();
	return pivot;
}


void Element::transform(const UT_Vector2R & new_pos, const fpreal & scale)
{
}


std::unique_ptr<Element> make_element(const ElementTypes elem_type) 
{
	std::unique_ptr<Element> w(new Element);
	switch (elem_type)
	{
	case ElementTypes::STRIPE:
		w->coords.append(UT_Vector2R(0.0, 0.0));
		w->coords.append(UT_Vector2R(0.0, 1.0));
		w->coords.append(UT_Vector2R(0.1, 1.0));
		w->coords.append(UT_Vector2R(0.1, 0.0));
	default:
		break;
	}
	return w;
}
