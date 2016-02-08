#pragma once
#include <UT/UT_Vector2Array.h>

struct BBox2D
{
	UT_Vector2R minvec;
	UT_Vector2R maxvec;
};

enum class ElementTypes {
	STRIPE,
	TSHAPE
};

class Element
{
public:
	Element();
	~Element();
	BBox2D bbox();
	UT_Vector2R pivot();
	void transform(const UT_Vector2R &new_pos, const fpreal &scale);

	UT_Vector2RArray coords;

};

std::unique_ptr<Element> make_element(const ElementTypes elem_type);

