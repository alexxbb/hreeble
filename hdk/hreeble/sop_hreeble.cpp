#include <iostream>
#include <OP/OP_Operator.h>
#include <OP/OP_OperatorTable.h>
#include <OP/OP_AutoLockInputs.h>
#include <UT/UT_ValArray.h>
#include <UT/UT_Vector2.h>
#include <SYS/SYS_Math.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PolyExtrude.h>
#include <GU/GU_PolyExtrude2.h>
#include "sop_hreeble.h"
#include <Element.h>

typedef UT_Vector2R V2R;
typedef UT_Pair<GA_Offset, GA_Offset> OffsetPair;

static PRM_Name prm_names[] = { PRM_Name("panel_height", "Panel Height"),
								PRM_Name("panel_inset", "Panel Inset") };

static PRM_Default seed_def(12345);
static PRM_Default inset_def(0.05);
static PRM_Default panel_height_def[] = { PRM_Default(0.001), PRM_Default(1.0) };
static PRM_Range panel_height_range(PRM_RANGE_RESTRICTED, 0.001, PRM_RANGE_UI, 0.1);

PRM_Template SOP_Hreeble::myparms[] = {
	PRM_Template(PRM_INT, 1, &PRMseedName, &seed_def), /*seed*/
	PRM_Template(PRM_FLT, 1, &prm_names[1], &inset_def), /*inset*/
	PRM_Template(PRM_FLT, 2, &prm_names[0], panel_height_def, 0, &panel_height_range), /*height*/
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

void SOP_Hreeble::extrude(GEO_Primitive * prim, const fpreal & height, const fpreal &inset, GEO_Primitive  * result_prim)
{
	auto build_prim = [this](const GA_OffsetArray &points) -> GEO_PrimPoly*{
		auto prim = static_cast<GEO_PrimPoly*>(gdp->appendPrimitive(GA_PRIMPOLY));
		for (const auto pt_off : points) {
			prim->appendVertex(pt_off);
		}
		prim->close();
		return prim;
	};
	//GU_PolyExtrudeParms parms;
	//parms.setInset(inset, 0);
	//parms.myOutputBack = false;
	//GU_PolyExtrude extrude(gdp);
	//extrude.extrude(parms);

	//work_group->clear();
	//work_group->add(prim);
	//auto extrude = GU_PolyExtrude2(gdp, work_group);
	//extrude.setInset(inset);
	//extrude.extrude(height);

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
	result_prim = (GEO_Primitive*)build_prim(front_points);
	kill_prims.append(prim);
}

OP_ERROR SOP_Hreeble::cookMySop(OP_Context & ctx)
{
	//auto elem = make_element(ElementTypes::STRIPE);
	my_seed = SeedPRM();
	fpreal64 panel_height_parm[2];
	PanelHeightPRM(panel_height_parm);
	fpreal panel_inset_parm = PanelInsetPRM();
	OP_AutoLockInputs inputs(this);
	if (inputs.lock(ctx) >= UT_ERROR_ABORT)
		return error();

	gdp->clearAndDestroy();
	duplicateSource(0, ctx);
	work_group = gdp->newPrimitiveGroup("temp", 1);

	ph = gdp->getP();
	GEO_Primitive *source_prim;
	UT_ValArray<GEO_PrimPoly*> new_prims;
	kill_prims.clear();

	fpreal panel_height = 0.0;
	for (GA_Iterator it(gdp->getPrimitiveRange()); !it.atEnd(); ++it) {
		my_seed += gdp->primitiveIndex(*it);
		source_prim = static_cast<GEO_Primitive*>(gdp->getPrimitive(*it));
		new_prims.clear();
		if (source_prim->getVertexCount() == 4)
			divide(source_prim, new_prims);
		for (auto prim : new_prims) {
			panel_height = SYSfit01((fpreal64)SYSrandom(my_seed), panel_height_parm[0], panel_height_parm[1]);
			GEO_Primitive *extruded_front_prim = 0;
			extrude(static_cast<GEO_Primitive*>(prim), panel_height, panel_inset_parm, extruded_front_prim);
		}
	}

	for (auto prim : kill_prims) {
		gdp->destroyPrimitive((*prim), true);
	}
	return error();
}
