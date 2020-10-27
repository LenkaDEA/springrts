// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#include <list>

#include "AAIAirForceManager.h"

#include "AAIMap.h"
#include "AAI.h"
#include "AAIBuildTable.h"
#include "AAIUnitTable.h"
#include "AAIConfig.h"
#include "AAIGroup.h"
#include "AAISector.h"

#include "LegacyCpp/UnitDef.h"
using namespace springLegacyAI;


AAIAirForceManager::AAIAirForceManager(AAI *ai)
{
	this->ai = ai;

	num_of_targets = 0;

	targets.resize(cfg->MAX_AIR_TARGETS);

	for(int i = 0; i < cfg->MAX_AIR_TARGETS; ++i)
		targets[i].unit_id = -1;
}

AAIAirForceManager::~AAIAirForceManager(void)
{

}

void AAIAirForceManager::CheckTarget(const UnitId& unitId, const AAIUnitCategory& category, float health)
{
	// do not attack own units
	if(ai->GetAICallback()->GetUnitTeam(unitId.id) != ai->GetMyTeamId()) 
	{
		float3 pos = ai->GetAICallback()->GetUnitPos(unitId.id);

		// calculate in which sector unit is located
		AAISector* sector = ai->Getmap()->GetSectorOfPos(pos);

		// check if unit is within the map
		if(sector)
		{
			// check for anti air defences if low on units
			if( (sector->GetLostAirUnits() > 2.5f) && (ai->GetUnitGroupsList(EUnitCategory::AIR_COMBAT).size() < 5) )
				return;

			AAIGroup *group(nullptr);
			int max_groups;

			if(health > 8000)
				max_groups = 3;
			else if(health > 4000)
				max_groups = 2;
			else
				max_groups = 1;

			for(int i = 0; i < max_groups; ++i)
			{
				if(category.IsAirCombat() == true)
				{
					group = GetAirGroup(100.0, EUnitType::ANTI_AIR);

					if(group)
						group->DefendAirSpace(&pos);
				}
				else if(category.IsBuilding() == true)
				{
					group = GetAirGroup(100.0, EUnitType::ANTI_STATIC);

					if(group)
						group->BombTarget(unitId.id, &pos);
				}
				else
				{
					group = GetAirGroup(100.0, EUnitType::ANTI_SURFACE);

					if(group)
						group->AirRaidUnit(unitId.id);
				}
			}
		}
	}
}

void AAIAirForceManager::CheckBombTarget(int unit_id, int def_id)
{
	// dont continue if target list already full
	if(num_of_targets >= cfg->MAX_AIR_TARGETS)
		return;

	// do not add own units or units that ar already on target list
	if( (ai->GetAICallback()->GetUnitTeam(unit_id) != ai->GetMyTeamId()) && !IsTarget(unit_id))
	{
		const float3 pos = ai->GetAICallback()->GetUnitPos(unit_id);

		// check if unit is within the map
		if( ai->Getmap()->IsPositionWithinMap(pos) )
		{
			AddTarget(unit_id, def_id);
		}
	}
}

void AAIAirForceManager::AddTarget(int unit_id, int def_id)
{
	for(int i = 0; i < cfg->MAX_AIR_TARGETS; ++i)
	{
		if(targets[i].unit_id == -1)
		{
			ai->LogConsole("Target added...");

			targets[i].pos = ai->GetAICallback()->GetUnitPos(unit_id);
			targets[i].def_id = def_id;
			targets[i].cost = ai->s_buildTree.GetTotalCost(UnitDefId(def_id));
			targets[i].health = ai->GetAICallback()->GetUnitHealth(unit_id);

			ai->Getut()->units[unit_id].status = BOMB_TARGET;

			++num_of_targets;

			return;
		}
	}

	// could not add target, randomly overwrite one of the existing targets
	/*i = rand()%cfg->MAX_AIR_TARGETS;
	targets[i].pos.x = pos.x;
	targets[i].pos.z = pos.z;
	targets[i].def_id = def_id;
	targets[i].cost = cost;
	targets[i].health = health;
	targets[i].category = category;*/
}

void AAIAirForceManager::RemoveTarget(int unit_id)
{
	for(int i = 0; i < cfg->MAX_AIR_TARGETS; ++i)
	{
		if(targets[i].unit_id == unit_id)
		{
			ai->LogConsole("Target removed...");

			targets[i].unit_id = -1;

			ai->Getut()->units[unit_id].status = ENEMY_UNIT;

			--num_of_targets;

			return;
		}
	}
}

void AAIAirForceManager::BombBestUnit(float cost, float danger)
{
	float highestRating(0.0f);
	int   selectedTargetId(-1);

	for(int i = 0; i < cfg->MAX_AIR_TARGETS; ++i)
	{
		if(targets[i].unit_id != -1)
		{
			AAISector* sector = ai->Getmap()->GetSectorOfPos(targets[i].pos);

			if(sector)
			{
				const float healthRating = ai->Getbt()->GetUnitDef(targets[i].def_id).health / targets[i].health; // favor already damaged targets
				//! @todo Check this formula
				const float rating = pow(targets[i].cost, cost) / (1.0f + sector->GetEnemyCombatPower(ETargetType::AIR) * danger) * healthRating;

				if(rating > highestRating)
				{
					highestRating = rating;
					selectedTargetId = i;
				}
			}
		}
	}

	if(selectedTargetId != -1)
	{
		AAIGroup *group = GetAirGroup(100.0, EUnitType::ANTI_STATIC);

		if(group)
		{
			//ai->LogConsole("Bombing...");
			group->BombTarget(targets[selectedTargetId].unit_id, &targets[selectedTargetId].pos);

			targets[selectedTargetId].unit_id = -1;
			--num_of_targets;
		}
	}
}

AAIGroup* AAIAirForceManager::GetAirGroup(float importance, EUnitType groupType) const
{
	for(auto group = ai->GetUnitGroupsList(EUnitCategory::AIR_COMBAT).begin(); group != ai->GetUnitGroupsList(EUnitCategory::AIR_COMBAT).end(); ++group)
	{
		if( ((*group)->task_importance < importance) && (*group)->GetUnitTypeOfGroup().IsUnitTypeSet(groupType) )
			return *group;
	}
	
	return nullptr;
}

bool AAIAirForceManager::IsTarget(int unit_id)
{
	for(int i = 0; i < cfg->MAX_AIR_TARGETS; ++i)
	{
		if(targets[i].unit_id == unit_id)
			return true;
	}

	return false;
}
