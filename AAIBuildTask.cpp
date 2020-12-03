// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#include <set>
using namespace std;

#include "AAIBuildTask.h"
#include "AAI.h"
#include "AAIConstructor.h"
#include "AAIUnitTable.h"
#include "AAIBuildTable.h"
#include "AAIExecute.h"
#include "AAIMap.h"
#include "AAISector.h"

AAIBuildTask::AAIBuildTask(AAI *ai, int unit_id, int def_id, float3 *pos, int tick):
	build_pos(*pos)
{
	this->ai = ai;
	this->unit_id = unit_id;
	this->def_id = def_id;

	builder_id = -1;
}

AAIBuildTask::~AAIBuildTask(void)
{
}

void AAIBuildTask::BuilderDestroyed()
{
	builder_id = -1;

	// com only allowed if buildpos is inside the base
	bool commander = false;

	AAISector* sector = ai->Getmap()->GetSectorOfPos(build_pos);

	if(sector && sector->GetDistanceToBase() == 0)
		commander = true;

	// look for new builder
	AAIConstructor* new_builder = ai->Getut()->FindClosestAssistant(build_pos, 10, commander);

	if(new_builder)
	{
		new_builder->TakeOverConstruction(this);
		builder_id = new_builder->m_myUnitId.id;
	}
}

void AAIBuildTask::BuildtaskFailed()
{
	// cleanup buildmap etc.
	if(ai->s_buildTree.GetMovementType(UnitDefId(def_id)).IsStatic() == true)
		ai->Getexecute()->ConstructionFailed(build_pos, def_id);

	// tell builder to stop construction (and release assisters) (if still alive)
	if( (builder_id >= 0) && (ai->Getut()->units[builder_id].cons != nullptr) )
		ai->Getut()->units[builder_id].cons->ConstructionFinished();
}
