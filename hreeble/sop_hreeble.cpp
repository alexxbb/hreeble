#include <iostream>
#include <OP/OP_Operator.h>
#include <OP/OP_OperatorTable.h>
#include <OP/OP_AutoLockInputs.h>
#include <UT/UT_ValArray.h>
#include <UT/UT_Vector2.h>
#include <UT/UT_Interrupt.h>
#include <SYS/SYS_Math.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimPoly.h>
#include <GA/GA_ElementWrangler.h>
#include <GEO/GEO_PolyCounts.h>
#include "sop_hreeble.h"
#include "Element.h"
#include "misc.h"

typedef UT_Vector2R V2R;
typedef UT_Pair<GA_Offset, GA_Offset> OffsetPair;

static PRM_Name prm_names[] = { PRM_Name("panel_height", "Panel Height"),
								PRM_Name("panel_inset", "Panel Inset"), 
								PRM_Name("elem_density", "Elements Density"),
								PRM_Name("elem_scale", "Element Scale"),
								PRM_Name("elem_height", "Element Height"),
								PRM_Name("elem_shapes", "Element Shapes"),
								PRM_Name("gen_panels", "Generate Panels"),
								PRM_Name("source_groups", "Source Prim Group"),
								PRM_Name("elem_groups", "Create Output Groups"),
								PRM_Name("convex", "Convex Geometry"),};

static PRM_Default seed_def(12345);
static PRM_Default inset_def(0.01);
static PRM_Default panel_height_def[] = { PRM_Default(0.001), PRM_Default(0.1) };
static PRM_Default elem_scale_def[] = { PRM_Default(0.5), PRM_Default(1.0) };
static PRM_Default elem_height_def[] = { PRM_Default(0.02), PRM_Default(0.1) };
static PRM_Default elem_shapes_def = PRM_Default(4);
static PRM_Range seed_range(PRM_RANGE_RESTRICTED, 1, PRM_RANGE_UI, 45000);
static PRM_Range panel_height_range(PRM_RANGE_RESTRICTED, 0.001, PRM_RANGE_UI, 0.1);
static PRM_Range elem_density_range(PRM_RANGE_RESTRICTED, 1, PRM_RANGE_UI, 10);
static PRM_Range elem_scale_range(PRM_RANGE_RESTRICTED, 0.0, PRM_RANGE_UI, 1);
static PRM_Range elem_height_range(PRM_RANGE_UI, 0.0001, PRM_RANGE_UI, 1);

static PRM_Item elem_shapes[] = { PRM_Item("stripev", "StripeV", "hr_stripe1"),
								  PRM_Item("stripev2", "StripeV2", "hr_stripe2"),
								  PRM_Item("stripev3", "StripeV3", "hr_stripe3"), 
								  PRM_Item("tshape", "TShape", "hr_tshape"), 
								  PRM_Item("rshape", "RShape", "hr_rshape"), 
								  PRM_Item("square", "Square", "hr_square"), 
								  PRM_Item(),};

static PRM_ChoiceList elem_shapes_list(PRM_CHOICELIST_TOGGLE, elem_shapes);
static uint prm_num_shapes = sizeof(elem_shapes) / sizeof(PRM_Item);

PRM_Template SOP_Hreeble::myparms[] = {
	PRM_Template(PRM_STRING, 1, &prm_names[7], 0, &SOP_Node::primGroupMenu, 0, 0, SOP_Node::getGroupSelectButton(GA_GROUP_PRIMITIVE)),
	PRM_Template(PRM_INT, 1, &PRMseedName, &seed_def, 0, &seed_range), /*seed*/
	PRM_Template(PRM_TOGGLE_E, 1, &prm_names[6], PRMoneDefaults), /*generate panels*/
	PRM_Template(PRM_FLT, 1, &prm_names[1], &inset_def), /*inset*/
	PRM_Template(PRM_FLT, 2, &prm_names[0], panel_height_def, 0, &panel_height_range), /*height*/
	PRM_Template(PRM_INT, 1, &prm_names[2], PRMoneDefaults, 0, &elem_density_range), /*element density*/
	PRM_Template(PRM_FLT, 2, &prm_names[3], elem_scale_def, 0, &elem_scale_range), /*element scale*/
	PRM_Template(PRM_FLT, 2, &prm_names[4], elem_height_def, 0, &elem_height_range), /*element height*/
	PRM_Template(PRM_ICONSTRIP, 1 , &prm_names[5], &elem_shapes_def, &elem_shapes_list),
	PRM_Template(PRM_TOGGLE_E, 1, &prm_names[9], PRMoneDefaults), /*convex geometry*/
	PRM_Template(PRM_TOGGLE_E, 1 , &prm_names[8], PRMzeroDefaults), /*create groups*/
	PRM_Template()
};

bool SOP_Hreeble::updateParmsFlags() {
	bool changed = SOP_Node::updateParmsFlags();
	uint generate_pannels = GeneratePanelsPRM();
	uint elem_shapes = SelectedShapesPRM();
	changed |= enableParm("panel_height", generate_pannels);
	changed |= enableParm("panel_inset", generate_pannels);
	changed |= enableParm("elem_density", elem_shapes);
	changed |= enableParm("elem_scale", elem_shapes);
	changed |= enableParm("elem_height", elem_shapes);
	changed |= enableParm("elem_groups", elem_shapes);
	return changed;
}

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
	SOP_Node(net, name, op), my_seed(0), source_prim_group(nullptr), elements_group(nullptr), elements_front_group(nullptr)
{
	//flags().timeDep = 1;
}

SOP_Hreeble::~SOP_Hreeble() { }

OP_Node * SOP_Hreeble::creator(OP_Network *net, const char *name, OP_Operator *op)
{
	return new SOP_Hreeble(net, name, op);
}

OP_ERROR SOP_Hreeble::cookInputGroups(OP_Context & ctx, int alone)
{
	return cookInputPrimitiveGroups(ctx, source_prim_group, alone);
}

//void SOP_Hreeble::split_primitive(GEO_Primitive * prim, UT_ValArray<GEO_Primitive*>& result, const unsigned short dir)
//{
//	//auto build_prim = [this](const UT_ValArray<GA_Offset> &poffsets) -> GEO_Primitive*{
//	//	auto new_prim = static_cast<GEO_PrimPoly*>(gdp->appendPrimitive(GA_PRIMPOLY));
//	//	for (const auto off : poffsets) {
//	//		new_prim->appendVertex(off);
//	//	}
//	//	new_prim->close();
//	//	return static_cast<GEO_Primitive*>(new_prim);
//	//};
//
//	GA_Offset new_pt0 = gdp->appendPointOffset();
//	GA_Offset new_pt1 = gdp->appendPointOffset();
//	UT_ValArray<GA_Offset> prim_points_offsets;
//	UT_ValArray<GA_Offset> new_prim_offsets;
//	UT_ValArray<GA_Offset> vert_interp_offsets;
//	
//	GA_VertexWrangler wrangler(*gdp, *gdp, GA_AttributeFilter::selectByName("uv"));
//	GA_Size new_vtx_num;
//	GA_Offset new_vtx_off;
//	GEO_PrimPoly *new_prim;
//	auto VtxToPtOff = [&prim, this](const GA_Size &index) {return gdp->vertexPoint(prim->getVertexOffset(index)); };
//	if (dir == 0) {
//		auto svec0 = ph.get(VtxToPtOff(3)) - ph.get(VtxToPtOff(0));
//		auto svec1 = ph.get(VtxToPtOff(2)) - ph.get(VtxToPtOff(1));
//		ph.set(new_pt0, ph.get(VtxToPtOff(0)) + svec0 * 0.5);
//		ph.set(new_pt1, ph.get(VtxToPtOff(1)) + svec1 * 0.5);
//		
//		new_prim = static_cast<GEO_PrimPoly*>(gdp->appendPrimitive(GA_PRIMPOLY));
//		new_vtx_num = new_prim->appendVertex(VtxToPtOff(0)); // vtx 0
//		new_vtx_off = new_prim->getVertexOffset(new_vtx_num);
//		wrangler.copyAttributeValues(new_vtx_off, prim->getVertexOffset(0));
//
//		new_vtx_num = new_prim->appendVertex(VtxToPtOff(1)); // vtx 1
//		new_vtx_off = new_prim->getVertexOffset(new_vtx_num);
//		wrangler.copyAttributeValues(new_vtx_off, prim->getVertexOffset(1));
//		
//		new_vtx_num = new_prim->appendVertex(new_pt1); // new_pt0
//		new_vtx_off = new_prim->getVertexOffset(new_vtx_num);
//		wrangler.lerpAttributeValues(new_vtx_off, prim->getVertexOffset(1), prim->getVertexOffset(2), 0.5);
//		
//		new_vtx_num = new_prim->appendVertex(new_pt0); // new_pt1
//		new_vtx_off = new_prim->getVertexOffset(new_vtx_num);
//		wrangler.lerpAttributeValues(new_vtx_off, prim->getVertexOffset(0), prim->getVertexOffset(3), 0.5);
//		new_prim->setClosed(true);
//		result.append(static_cast<GEO_Primitive*>(new_prim));
//
//		new_prim = static_cast<GEO_PrimPoly*>(gdp->appendPrimitive(GA_PRIMPOLY));
//		new_vtx_num = new_prim->appendVertex(new_pt1); // vtx 0
//		new_vtx_off = new_prim->getVertexOffset(new_vtx_num);
//		wrangler.lerpAttributeValues(new_vtx_off, prim->getVertexOffset(0), prim->getVertexOffset(3), 0.5);
//
//		new_vtx_num = new_prim->appendVertex(new_pt0); // vtx 1
//		new_vtx_off = new_prim->getVertexOffset(new_vtx_num);
//		wrangler.lerpAttributeValues(new_vtx_off, prim->getVertexOffset(1), prim->getVertexOffset(2), 0.5);
//		
//		new_vtx_num = new_prim->appendVertex(VtxToPtOff(2)); // new_pt0
//		new_vtx_off = new_prim->getVertexOffset(new_vtx_num);
//		wrangler.copyAttributeValues(new_vtx_off, prim->getVertexOffset(2));
//		
//		new_vtx_num = new_prim->appendVertex(VtxToPtOff(3)); // new_pt1
//		new_vtx_off = new_prim->getVertexOffset(new_vtx_num);
//		wrangler.copyAttributeValues(new_vtx_off, prim->getVertexOffset(3));
//		new_prim->setClosed(true);
//		result.append(static_cast<GEO_Primitive*>(new_prim));
//
//	}
//	//else {
//	//	auto tvec0 = ph.get(prim_points_offsets(1)) - ph.get(prim_points_offsets(0));
//	//	auto tvec1 = ph.get(prim_points_offsets(2)) - ph.get(prim_points_offsets(3));
//	//	ph.set(new_pt0, ph.get(prim_points_offsets(0)) + tvec0 * 0.5);
//	//	ph.set(new_pt1, ph.get(prim_points_offsets(3)) + tvec1 * 0.5);
//
//	//	new_prim_offsets.append(prim_points_offsets(0));
//	//	new_prim_offsets.append(new_pt0);
//	//	new_prim_offsets.append(new_pt1);
//	//	new_prim_offsets.append(prim_points_offsets(3));
//	//	result.append(build_prim(new_prim_offsets));
//
//	//	new_prim_offsets.clear();
//	//	new_prim_offsets.append(new_pt0);
//	//	new_prim_offsets.append(prim_points_offsets(1));
//	//	new_prim_offsets.append(prim_points_offsets(2));
//	//	new_prim_offsets.append(new_pt1);
//	//	result.append(build_prim(new_prim_offsets));
//	//}
//	bla:
//	kill_prims.append(prim);
//}

void SOP_Hreeble::split_primitive(GEO_Primitive * source_prim, UT_ValArray<GEO_Primitive*>& result, const unsigned short dir)
{
	GA_OffsetArray prim_ptoffs;
	GA_OffsetArray prim_vtoffs;
	for (GA_Iterator it(source_prim->getVertexRange()); !it.atEnd(); ++it){
		prim_vtoffs.append(*it);
		prim_ptoffs.append(gdp->vertexPoint(*it));
	}

	GA_Offset new_ptof = gdp->appendPointBlock(2);
	GA_VertexWrangler twrangler(*gdp);
	//auto VtxToPt = [this](const GEO_PrimPoly *prim, const GA_Size &index) {return gdp->vertexPoint(prim->getVertexOffset(index)); };
	GEO_PrimPoly *prim1 = GEO_PrimPoly::build(gdp, 4, false, false);
	GEO_PrimPoly *prim2 = GEO_PrimPoly::build(gdp, 4, false, false);
	if (dir == 0) {
		auto svec0 = ph.get(prim_ptoffs(3)) - ph.get(prim_ptoffs(0));
		auto svec1 = ph.get(prim_ptoffs(2)) - ph.get(prim_ptoffs(1));
		ph.set(new_ptof, ph.get(prim_ptoffs(1)) + svec1 * 0.5); // vertex  top middle
		ph.set(new_ptof + 1, ph.get(prim_ptoffs(0)) + svec0 * 0.5); // vertex bottom middle
		
		prim1->setVertexPoint(0, prim_ptoffs(0));
		twrangler.copyAttributeValues(prim1->getVertexOffset(0), prim_vtoffs(0));
		prim1->setVertexPoint(1, prim_ptoffs(1));
		twrangler.copyAttributeValues(prim1->getVertexOffset(1), prim_vtoffs(1));
		prim1->setVertexPoint(2, new_ptof);
		twrangler.lerpAttributeValues(prim1->getVertexOffset(2), prim_vtoffs(1), prim_vtoffs(2), 0.5);
		prim1->setVertexPoint(3, new_ptof + 1);
		twrangler.lerpAttributeValues(prim1->getVertexOffset(3), prim_vtoffs(0), prim_vtoffs(3), 0.5);
		
		prim2->setVertexPoint(0, new_ptof+1);
		twrangler.lerpAttributeValues(prim2->getVertexOffset(0), prim_vtoffs(0), prim_vtoffs(3), 0.5);
		prim2->setVertexPoint(1, new_ptof);
		twrangler.lerpAttributeValues(prim2->getVertexOffset(1), prim_vtoffs(1), prim_vtoffs(2), 0.5);
		prim2->setVertexPoint(2, prim_ptoffs(2));
		twrangler.copyAttributeValues(prim2->getVertexOffset(2), prim_vtoffs(2));
		prim2->setVertexPoint(3, prim_ptoffs(3));
		twrangler.copyAttributeValues(prim2->getVertexOffset(3), prim_vtoffs(3));
	}
	else {
		auto svec0 = ph.get(prim_ptoffs(1)) - ph.get(prim_ptoffs(0));
		auto svec1 = ph.get(prim_ptoffs(2)) - ph.get(prim_ptoffs(3));
		ph.set(new_ptof, ph.get(prim_ptoffs(0)) + svec0 * 0.5); // vertex  left middle
		ph.set(new_ptof+1, ph.get(prim_ptoffs(3)) + svec1 * 0.5); // vertex right middle
		
		prim1->setVertexPoint(0, prim_ptoffs(0));
		twrangler.copyAttributeValues(prim1->getVertexOffset(0), prim_vtoffs(0));
		prim1->setVertexPoint(1, new_ptof);
		twrangler.lerpAttributeValues(prim1->getVertexOffset(1), prim_vtoffs(0), prim_vtoffs(1), 0.5);
		prim1->setVertexPoint(2, new_ptof + 1);
		twrangler.lerpAttributeValues(prim1->getVertexOffset(2), prim_vtoffs(2), prim_vtoffs(3), 0.5);
		prim1->setVertexPoint(3, prim_ptoffs(3));
		twrangler.copyAttributeValues(prim1->getVertexOffset(3), prim_vtoffs(3));
		
		prim2->setVertexPoint(0, new_ptof);
		twrangler.lerpAttributeValues(prim2->getVertexOffset(0), prim_vtoffs(0), prim_vtoffs(1), 0.5);
		prim2->setVertexPoint(1, prim_ptoffs(1));
		twrangler.copyAttributeValues(prim2->getVertexOffset(1), prim_vtoffs(1));
		prim2->setVertexPoint(2, prim_ptoffs(2));
		twrangler.copyAttributeValues(prim2->getVertexOffset(2), prim_vtoffs(2));
		prim2->setVertexPoint(3, new_ptof + 1);
		twrangler.lerpAttributeValues(prim2->getVertexOffset(3), prim_vtoffs(2), prim_vtoffs(3), 0.5);
	}


	result.append((GEO_Primitive*)prim1);
	result.append((GEO_Primitive*)prim2);
	kill_prims.append(source_prim);
}

void SOP_Hreeble::divide(GEO_Primitive * prim, UT_ValArray<GEO_Primitive*>& result)
{
	unsigned short dir = SYStrunc(SYSrandom(my_seed) * 2);
	auto prim_to_split = prim;
	split_primitive(prim_to_split, result, dir);

	auto nr = my_seed * 1999;
	unsigned short index = SYStrunc(SYSrandom(nr) * 2);
	prim_to_split = result(index);
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
	fpreal time = ctx.getTime();
	uint seed_parm = SeedPRM();
	fpreal64 panel_height_parm[2];
	fpreal64 elem_scale_parm[2];
	fpreal64 elem_height_parm[2];
	PanelHeightPRM(panel_height_parm, time);
	ElemScalePRM(elem_scale_parm, time);
	ElemHeightPRM(elem_height_parm, time);
	fpreal panel_inset_parm = PanelInsetPRM();
	uint element_density = ElemDensityPRM();
	uint shapes_parm = SelectedShapesPRM();
	uint generate_panels = GeneratePanelsPRM();

	UT_ValArray<uint> selected_shapes;
	for (uint i = 0; i < prm_num_shapes; i++) {
		if ((shapes_parm & (0x01 << i)) != 0) {
			selected_shapes.append(0x01 << i);
		}
	}

	UT_AutoInterrupt boss("Making hreeble...");
	OP_AutoLockInputs inputs(this);
	if (error() > UT_ERROR_ABORT 
		|| inputs.lock(ctx) >= UT_ERROR_ABORT
		|| cookInputGroups(ctx) > UT_ERROR_ABORT 
		|| (source_prim_group && source_prim_group->isEmpty()))
		return error();

	gdp->clearAndDestroy();
	duplicateSource(0, ctx);
	ph = gdp->getP();
	if (DoConvexPRM() == 1)
		gdp->convex(GA_Size(4));
	UT_ValArray<GEO_Primitive*> panel_prims;
	UT_ValArray<GEO_Primitive*> top_prims;
	kill_prims.clear();
	uint num_selected_shapes = selected_shapes.entries();
	if (num_selected_shapes == 0 && generate_panels == 0) return error();
	elements_group = nullptr;
	elements_front_group = nullptr;
	if (CreateGroupsPRM() != 0) {
		elements_group = gdp->newPrimitiveGroup("elements");
		elements_front_group = gdp->newPrimitiveGroup("elements_front");
	}
	fpreal panel_height = 0.0;
	my_seed = seed_parm;
	for (GA_Iterator it(gdp->getPrimitiveRange(source_prim_group)); !it.atEnd(); ++it) {
		GEO_Primitive *source_prim = static_cast<GEO_Primitive*>(gdp->getPrimitive(*it));
		top_prims.clear();
		if (boss.wasInterrupted())
			break;
		if (generate_panels != 0) {
			panel_prims.clear();
			if (source_prim->getVertexCount() == 4)
				divide(source_prim, panel_prims); // Divide source prim into panels
			else if (source_prim->getVertexCount() == 3)
				panel_prims.append(source_prim);
			//for (auto prim : panel_prims) {
			//	panel_height = SYSfit01((fpreal64)SYSfastRandom(my_seed), panel_height_parm[0], panel_height_parm[1]);
			//	GEO_Primitive *extruded_front_prim = extrude(prim, panel_height, panel_inset_parm);
			//	top_prims.append(extruded_front_prim);
			//}
		}
		else {
			top_prims.append(source_prim);
		}
		continue;
		if (num_selected_shapes != 0) {
			for (const auto prim: top_prims){
				UT_Vector3 primN = prim->computeNormal();
				auto num_vtx = prim->getVertexCount();
				ElementTypes type;
				for (uint i = 0; i < element_density; i++) {
					uint elem_seed = seed_parm + prim->getMapIndex() * 130145 + i * 12987;
					if (num_vtx == 3)
						type = ElementTypes::TRIANGLE;
					else
						type = static_cast<ElementTypes>(hreeble::rand_choice(selected_shapes, elem_seed));
					fpreal elem_height = SYSfit01((fpreal64)SYSfastRandom(elem_seed), elem_height_parm[0], elem_height_parm[1]);
					UT_Vector2R elem_pos(SYSfastRandom(elem_seed), SYSfastRandom(elem_seed));
					fpreal elem_scale = SYSfit01((fpreal64)SYSfastRandom(elem_seed), elem_scale_parm[0], elem_scale_parm[1]);
					auto element = make_element(type, (short)hreeble::rand_bool(elem_seed), elements_group, elements_front_group);
					element->transform(elem_pos, elem_scale, hreeble::rand_bool(elem_seed + 11234));
					element->build(gdp, prim, primN, elem_height);
				}
			}
		}
	}

	for (auto prim : kill_prims) {
		gdp->destroyPrimitive((*prim), true);
	}
	return error();
}
