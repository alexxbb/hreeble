#include <iostream>
#include <OP/OP_Operator.h>
#include <OP/OP_OperatorTable.h>
#include <OP/OP_AutoLockInputs.h>
#include <UT/UT_ValArray.h>
#include <UT/UT_Vector2.h>
#include <UT/UT_Interrupt.h>
#include <SYS/SYS_Math.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PolyExtrude.h>
#include <GU/GU_PolyExtrude2.h>
#include "sop_hreeble.h"
#include <Element.h>

typedef UT_Vector2R V2R;
typedef UT_Pair<GA_Offset, GA_Offset> OffsetPair;

static PRM_Name prm_names[] = { PRM_Name("panel_height", "Panel Height"),
								PRM_Name("panel_inset", "Panel Inset"), 
								PRM_Name("elem_density", "Elements Density"),
								PRM_Name("elem_scale", "Element Scale"),
								PRM_Name("elem_height", "Element Height"),
								PRM_Name("elem_shapes", "Element Shapes")};

static PRM_Default seed_def(12345);
static PRM_Default inset_def(0.1);
static PRM_Default panel_height_def[] = { PRM_Default(0.001), PRM_Default(1.0) };
static PRM_Default elem_scale_def[] = { PRM_Default(0.01), PRM_Default(1.0) };
static PRM_Default elem_height_def[] = { PRM_Default(0.1), PRM_Default(1.0) };
static PRM_Default elem_shapes_def = PRM_Default(4);
static PRM_Range seed_range(PRM_RANGE_RESTRICTED, 1, PRM_RANGE_UI, 45000);
static PRM_Range panel_height_range(PRM_RANGE_RESTRICTED, 0.001, PRM_RANGE_UI, 0.1);
static PRM_Range elem_density_range(PRM_RANGE_RESTRICTED, 1, PRM_RANGE_UI, 10);
static PRM_Range elem_scale_range(PRM_RANGE_UI, 0.001, PRM_RANGE_UI, 1);
static PRM_Range elem_height_range(PRM_RANGE_UI, 0.001, PRM_RANGE_UI, 1);

static PRM_Item elem_shapes[] = { PRM_Item("stripev", "StripeV", "SOP_normal"),
								  PRM_Item("stripev2", "StripeV2", "SOP_normal"),
								  PRM_Item("stripev3", "StripeV3", "SOP_normal"), 
								  PRM_Item(),};

static PRM_ChoiceList elem_shapes_list(PRM_CHOICELIST_TOGGLE, elem_shapes);

PRM_Template SOP_Hreeble::myparms[] = {
	PRM_Template(PRM_INT, 1, &PRMseedName, &seed_def, 0, &seed_range), /*seed*/
	PRM_Template(PRM_FLT, 1, &prm_names[1], &inset_def), /*inset*/
	PRM_Template(PRM_FLT, 2, &prm_names[0], panel_height_def, 0, &panel_height_range), /*height*/
	PRM_Template(PRM_INT, 1, &prm_names[2], PRMoneDefaults, 0, &elem_density_range), /*element density*/
	PRM_Template(PRM_FLT, 2, &prm_names[3], elem_scale_def, 0, &elem_scale_range), /*element scale*/
	PRM_Template(PRM_FLT, 2, &prm_names[4], elem_height_def, 0, &elem_height_range), /*element height*/
	PRM_Template(PRM_ICONSTRIP, (sizeof(elem_shapes)/sizeof(PRM_Item)) - 1 , &prm_names[5], &elem_shapes_def, &elem_shapes_list),
	PRM_Template()
};

void newSopOperator(OP_OperatorTable *table) {
	table->addOperator(new OP_Operator(
		"hreeble",
		"Hreeble",
		SOP_Hreeble::creator,
		SOP_Hreeble::myparms,
		1,
		1));
}

SOP_Hreeble::SOP_Hreeble(OP_Network * net, const char * name, OP_Operator * op):
	SOP_Node(net, name, op), my_seed(0)
{
}

SOP_Hreeble::~SOP_Hreeble() { }

OP_Node * SOP_Hreeble::creator(OP_Network *net, const char *name, OP_Operator *op)
{
	return new SOP_Hreeble(net, name, op);
}

void SOP_Hreeble::split_primitive(GEO_Primitive * prim, UT_ValArray<GEO_PrimPoly*>& result, const unsigned short dir)
{
	auto build_prim = [this](const UT_ValArray<GA_Offset> &offsets) -> GEO_PrimPoly*{
		auto prim = static_cast<GEO_PrimPoly*>(gdp->appendPrimitive(GA_PRIMPOLY));
		for (const auto off : offsets) {
			prim->appendVertex(off);
		}
		prim->close();
		return prim;
	};
	GA_Offset new_pt0 = gdp->appendPointOffset();
	GA_Offset new_pt1 = gdp->appendPointOffset();
	UT_ValArray<GA_Offset> prim_points_offsets;
	UT_ValArray<GA_Offset> new_prim_offsets;
	for (GA_Iterator it(prim->getVertexRange()); !it.atEnd(); ++it) {
		prim_points_offsets.append(gdp->vertexPoint(*it));
	}
	if (dir == 0) {
		auto svec0 = ph.get(prim_points_offsets(3)) - ph.get(prim_points_offsets(0));
		auto svec1 = ph.get(prim_points_offsets(2)) - ph.get(prim_points_offsets(1));
		ph.set(new_pt0, ph.get(prim_points_offsets(0)) + svec0 * 0.5);
		ph.set(new_pt1, ph.get(prim_points_offsets(1)) + svec1 * 0.5);

		new_prim_offsets.append(prim_points_offsets(0));
		new_prim_offsets.append(prim_points_offsets(1));
		new_prim_offsets.append(new_pt1);
		new_prim_offsets.append(new_pt0);
		result.append(build_prim(new_prim_offsets));

		new_prim_offsets.clear();
		new_prim_offsets.append(new_pt0);
		new_prim_offsets.append(new_pt1);
		new_prim_offsets.append(prim_points_offsets(2));
		new_prim_offsets.append(prim_points_offsets(3));
		result.append(build_prim(new_prim_offsets));
	}
	else {
		auto tvec0 = ph.get(prim_points_offsets(1)) - ph.get(prim_points_offsets(0));
		auto tvec1 = ph.get(prim_points_offsets(2)) - ph.get(prim_points_offsets(3));
		ph.set(new_pt0, ph.get(prim_points_offsets(0)) + tvec0 * 0.5);
		ph.set(new_pt1, ph.get(prim_points_offsets(3)) + tvec1 * 0.5);

		new_prim_offsets.append(prim_points_offsets(0));
		new_prim_offsets.append(new_pt0);
		new_prim_offsets.append(new_pt1);
		new_prim_offsets.append(prim_points_offsets(3));
		result.append(build_prim(new_prim_offsets));

		new_prim_offsets.clear();
		new_prim_offsets.append(new_pt0);
		new_prim_offsets.append(prim_points_offsets(1));
		new_prim_offsets.append(prim_points_offsets(2));
		new_prim_offsets.append(new_pt1);
		result.append(build_prim(new_prim_offsets));
	}
	kill_prims.append(prim);
}

void SOP_Hreeble::divide(GEO_Primitive * prim, UT_ValArray<GEO_PrimPoly*>& result)
{
	unsigned short dir = SYStrunc(SYSrandom(my_seed) * 2);
	auto prim_to_split = prim;
	split_primitive(prim_to_split, result, dir);

	auto nr = my_seed * 1999;
	unsigned short index = SYStrunc(SYSrandom(nr) * 2);
	prim_to_split = static_cast<GEO_Primitive*>(result(index));
	result.removeIndex(index);
	split_primitive(prim_to_split, result, (1 - dir));
}

GEO_Primitive* SOP_Hreeble::extrude(GEO_Primitive * prim, const fpreal & height, const fpreal &inset)
{
	auto build_prim = [this](const GA_OffsetArray &points) -> GEO_Primitive*{
		auto prim = static_cast<GEO_PrimPoly*>(gdp->appendPrimitive(GA_PRIMPOLY));
		for (const auto pt_off : points) {
			prim->appendVertex(pt_off);
		}
		prim->close();
		return static_cast<GEO_Primitive*>(prim);
	};

	UT_Vector3 primN = prim->computeNormal();
	UT_Vector4 primP;
	prim->evaluateInteriorPoint(primP, 0.5, 0.5);
	UT_Vector3 top_center = primP + primN * height;
	GA_OffsetArray front_points;
	UT_ValArray<UT_Pair<GA_Offset, GA_Offset>> pairs;
	GA_Offset new_points = gdp->appendPointBlock(prim->getVertexCount());
	GA_Iterator it(prim->getVertexRange());
	GA_Size i = 0;
	for (i, it; i < prim->getVertexCount(), !it.atEnd(); i++, ++it) {
		GA_Offset cur_pt_off = gdp->vertexPoint(*it);
		GA_Offset new_pt_off = new_points + i;
		front_points.append(new_pt_off);
		UT_Vector3 pt_pos = ph.get(cur_pt_off) + primN * height;
		UT_Vector3 inset_dir = (top_center - pt_pos);
		inset_dir.normalize();
		pt_pos += inset_dir * inset;
		ph.set(new_pt_off, pt_pos);
		pairs.append(OffsetPair(cur_pt_off, new_pt_off));
	}

	pairs.append(pairs(0));
	GA_OffsetArray prim_points;
	for (GA_Size i = 0; i < prim->getVertexCount(); i++){
		OffsetPair pair1 = pairs(i);
		OffsetPair pair2 = pairs(i + 1);
		prim_points.clear();
		prim_points.append(pair1.myFirst);
		prim_points.append(pair2.myFirst);
		prim_points.append(pair2.mySecond);
		prim_points.append(pair1.mySecond);
		build_prim(prim_points);
	}
	auto top_prim = build_prim(front_points);
	kill_prims.append(prim);
	return top_prim;
}

OP_ERROR SOP_Hreeble::cookMySop(OP_Context & ctx)
{
	uint seed_parm = SeedPRM();
	fpreal64 panel_height_parm[2];
	fpreal64 elem_scale_parm[2];
	fpreal64 elem_height_parm[2];
	PanelHeightPRM(panel_height_parm);
	ElemScalePRM(elem_scale_parm);
	ElemHeightPRM(elem_height_parm);
	fpreal panel_inset_parm = PanelInsetPRM();
	uint element_density = ElemDensityPRM();
	uint shapes_parm = SelectedShapesPRM();

	UT_ValArray<uint> selected_shapes;
	for (uint i = 0; i < 3; i++) {
		if ((shapes_parm & (0x01 << i)) != 0) {
			selected_shapes.append(0x01 << i);
		}
	}

	OP_AutoLockInputs inputs(this);
	if (inputs.lock(ctx) >= UT_ERROR_ABORT)
		return error();

	gdp->clearAndDestroy();
	duplicateSource(0, ctx);

	UT_AutoInterrupt boss("Cooking hreeble.....");
	if (boss.wasInterrupted())
		return error();

	work_group = gdp->newPrimitiveGroup("temp", 1);
	ph = gdp->getP();
	GEO_Primitive *source_prim;
	UT_ValArray<GEO_PrimPoly*> pannels_prims;
	kill_prims.clear();

	uint num_selected_shapes = selected_shapes.entries();
	fpreal panel_height = 0.0;
	my_seed = seed_parm;
	for (GA_Iterator it(gdp->getPrimitiveRange()); !it.atEnd(); ++it) {
		source_prim = static_cast<GEO_Primitive*>(gdp->getPrimitive(*it));
		pannels_prims.clear();
		if (boss.wasInterrupted())
			break;
		if (source_prim->getVertexCount() == 4)
			// Divide source prim into panels
			divide(source_prim, pannels_prims);
		else if (source_prim->getVertexCount() == 3)
			pannels_prims.append(static_cast<GEO_PrimPoly*>(source_prim));
		for (auto prim : pannels_prims) {
			panel_height = SYSfit01((fpreal64)SYSfastRandom(my_seed), panel_height_parm[0], panel_height_parm[1]);
			// Extrude panel
			GEO_Primitive *extruded_front_prim = extrude(static_cast<GEO_Primitive*>(prim), panel_height, panel_inset_parm);
			if (num_selected_shapes != 0) {
				UT_Vector3 primN = extruded_front_prim->computeNormal();
				for (uint i = 0; i < element_density; i++) {
					uint elem_seed = seed_parm + extruded_front_prim->getMapIndex() + i * 1000;
					uint shape_index = (int)SYSfloor((fpreal64)SYSfastRandom(elem_seed) * num_selected_shapes);
					ElementTypes type = static_cast<ElementTypes>(selected_shapes(shape_index));
					fpreal elem_height = SYSfit01((fpreal64)SYSfastRandom(elem_seed), elem_height_parm[0], elem_height_parm[1]);
					UT_Vector2R elem_pos(SYSfastRandom(elem_seed), SYSfastRandom(elem_seed));
					fpreal elem_scale = SYSfit01((fpreal64)SYSfastRandom(elem_seed), elem_scale_parm[0], elem_scale_parm[1]);
					auto element = make_element(type);
					element->transform(elem_pos, elem_scale);
					element->build(gdp, extruded_front_prim, primN, elem_height);
				}
			}
		}
	}

	for (auto prim : kill_prims) {
		gdp->destroyPrimitive((*prim), true);
	}
	return error();
}
