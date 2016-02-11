#pragma once
#include <UT/UT_Vector2Array.h>
#include <GU/GU_Detail.h>

struct BBox2D
{
	UT_Vector2R minvec;
	UT_Vector2R maxvec;
};

enum class ElementTypes {
	STRIPEV  = 0x0001,
	STRIPEV2 = 0x0002,
	STRIPEV3 = 0x0004,
	//TSHAPE   = 0x0008,
};


class SubElem
{
public:
	UT_Vector2RArray coords;
};

class Element
{
public:
	Element();
	Element(SubElem elem);
	~Element();
	BBox2D bbox();
	UT_Vector2R pivot();
	UT_Vector2R bounds_intersection();
	void append(SubElem elem);
	void transform(const UT_Vector2R &new_pos, const fpreal &scale);
	void build(GU_Detail *gdp, const GEO_Primitive *prim, const UT_Vector3 &primN, const fpreal &height);

private:
	UT_ValArray<SubElem> subelements;
	void move_by_vec(const UT_Vector2R &vec);
	exint num_points();

};

std::unique_ptr<Element> make_element(const ElementTypes &elem_type);

