// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#include <set>

#include "AAI.h"
#include "AAIConstructor.h"
#include "AAIBuildTask.h"
#include "AAIExecute.h"
#include "AAIBrain.h"
#include "AAIBuildTable.h"
#include "AAIUnitTable.h"
#include "AAIConfig.h"
#include "AAIMap.h"
#include "AAISector.h"


#include "LegacyCpp/UnitDef.h"
#include "LegacyCpp/CommandQueue.h"
using namespace springLegacyAI;


AAIConstructor::AAIConstructor(AAI *ai, UnitId unitId, UnitDefId defId, bool factory, bool builder, bool assistant, std::list<int>* buildque) :
	m_myUnitId(unitId),
	m_myDefId(defId),
	m_constructedUnitId(),
	m_constructedDefId(),
	m_isFactory(factory),
	m_isBuilder(builder),
	m_isAssistant(assistant),
 	m_buildPos(ZeroVector),
	m_assistUnitId(),
	m_activity(EConstructorActivity::IDLE),
	m_buildqueue(buildque)
{
	this->ai = ai;
	build_task = 0;
}

AAIConstructor::~AAIConstructor(void)
{
}

bool AAIConstructor::isBusy() const
{
	const CCommandQueue *commands = ai->Getcb()->GetCurrentUnitCommands(m_myUnitId.id);

	if(commands->empty())
		return false;
	else
		return true;
}

void AAIConstructor::Idle()
{
	if(m_isBuilder)
	{
		if(m_activity.IsCarryingOutConstructionOrder() == true)
		{
			if(m_constructedUnitId.isValid() == false)
			{
				//ai->Getbt()->units_dynamic[construction_def_id].active -= 1;
				//assert(ai->Getbt()->units_dynamic[construction_def_id].active >= 0);
				ai->Getut()->UnitRequestFailed(ai->Getbt()->s_buildTree.GetUnitCategory(m_constructedDefId));

				// clear up buildmap etc. (make sure conctructor wanted to build a building and not a unit)
				if( ai->Getbt()->s_buildTree.GetMovementType(m_constructedDefId).isStatic() == true )
					ai->Getexecute()->ConstructionFailed(m_buildPos, m_constructedDefId.id);

				// free builder
				ConstructionFinished();
			}
		}
		else if(m_activity.IsDestroyed() == false)
		{
			m_activity.SetActivity(EConstructorActivity::IDLE);
			m_assistUnitId.invalidate();

			ReleaseAllAssistants();
		}
	}

	if(m_isFactory)
	{
		ConstructionFinished();

		Update();
	}
}

void AAIConstructor::Update()
{
	if(m_isFactory && m_buildqueue != nullptr)
	{
		if( (m_activity.IsConstructing() == false) && (m_buildqueue->empty() == false) )
		{
			UnitDefId constructedUnitDefId(*m_buildqueue->begin());

			// check if mobile or stationary builder
			if(ai->Getbt()->s_buildTree.GetMovementType(m_myDefId).isStatic() == true )  
			{
				// give build order
				Command c(-constructedUnitDefId.id);
				ai->Getcb()->GiveOrder(m_myUnitId.id, &c);

				m_constructedDefId = constructedUnitDefId.id;
				m_activity.SetActivity(EConstructorActivity::CONSTRUCTING);

				//if(ai->Getbt()->IsFactory(def_id))
				//	++ai->futureFactories;

				m_buildqueue->pop_front();
			}
			else
			{
				// find buildpos for the unit
				float3 pos = ai->Getexecute()->GetUnitBuildsite(m_myUnitId.id, constructedUnitDefId.id);

				if(pos.x > 0)
				{
					// give build order
					Command c(-constructedUnitDefId.id);
					c.PushPos(pos);

					ai->Getcb()->GiveOrder(m_myUnitId.id, &c);
					m_constructedDefId = constructedUnitDefId.id;
					assert(ai->Getbt()->IsValidUnitDefID(constructedUnitDefId.id));
					m_activity.SetActivity(EConstructorActivity::CONSTRUCTING); //! @todo Should be HEADING_TO_BUILDSITE

					ai->Getut()->UnitRequested(ai->Getbt()->s_buildTree.GetUnitCategory(constructedUnitDefId)); // request must be called before create to keep unit counters correct
					ai->Getut()->UnitCreated(ai->Getbt()->s_buildTree.GetUnitCategory(constructedUnitDefId));

					m_buildqueue->pop_front();
				}
			}

			return;
		}

		CheckAssistance();
	}

	if(m_isBuilder)
	{
		if(m_activity.IsCarryingOutConstructionOrder() == true)
		{
			// if building has begun, check for possible assisters
			if(m_constructedUnitId.isValid() == true)
				CheckAssistance();
			// if building has not yet begun, check if something unexpected happened (buildsite blocked)
			else if(isBusy() == false) // && !ai->Getbt()->IsValidUnitDefID(construction_unit_id))
			{
				ConstructionFailed();
			}
		}
		/*else if(task == UNIT_IDLE)
		{
			float3 pos = ai->Getcb()->GetUnitPos(unit_id);

			if(pos.x > 0)
			{
				int x = pos.x/ai->Getmap()->xSectorSize;
				int y = pos.z/ai->Getmap()->ySectorSize;

				if(x >= 0 && y >= 0 && x < ai->Getmap()->xSectors && y < ai->Getmap()->ySectors)
				{
					if(ai->Getmap()->sector[x][y].distance_to_base < 2)
					{
						pos = ai->Getmap()->sector[x][y].GetCenter();

						Command c;
						const UnitDef *def;

						def = ai->Getcb()->GetUnitDef(unit_id);
						// can this thing resurrect? If so, maybe we should raise the corpses instead of consuming them?
						if(def->canResurrect)
						{
							if(rand()%2 == 1)
								c.id = CMD_RESURRECT;
							else
								c.id = CMD_RECLAIM;
						}
						else
							c.id = CMD_RECLAIM;

						c.PushParam(pos.x);
						c.PushParam(ai->Getcb()->GetElevation(pos.x, pos.z));
						c.PushParam(pos.z);
						c.PushParam(500.0);

						//ai->Getcb()->GiveOrder(unit_id, &c);
						task = RECLAIMING;
						ai->Getexecute()->GiveOrder(&c, unit_id, "Builder::Reclaming");
					}
				}
			}
		}
		*/
	}
}

void AAIConstructor::CheckAssistance()
{
	if(m_isFactory && (m_buildqueue != nullptr))
	{
		// check if another factory of that type needed
		if(m_buildqueue->size() >= cfg->MAX_BUILDQUE_SIZE - 2 && assistants.size() >= cfg->MAX_ASSISTANTS-2)
		{
			if(ai->Getbt()->units_dynamic[m_myDefId.id].active + ai->Getbt()->units_dynamic[m_myDefId.id].requested + ai->Getbt()->units_dynamic[m_myDefId.id].under_construction  < cfg->MAX_FACTORIES_PER_TYPE)
			{
				ai->Getbt()->units_dynamic[m_myDefId.id].requested += 1;

				if(ai->Getexecute()->urgency[STATIONARY_CONSTRUCTOR] < 1.5f)
					ai->Getexecute()->urgency[STATIONARY_CONSTRUCTOR] = 1.5f;

				ai->Getbt()->ConstructorRequested(m_myDefId);
			}
		}

		// check if support needed
		if(assistants.size() < cfg->MAX_ASSISTANTS)
		{
			bool assist = false;

			if(m_buildqueue->size() > 2)
				assist = true;
			else if(m_constructedDefId.isValid() == true) 
			{
				const float buildspeed( ai->Getbt()->s_buildTree.GetBuildspeed(m_myDefId) ); 
				if (buildspeed > 0.0f) 
				{
					const float buildtime = ai->Getbt()->s_buildTree.GetBuildtime(m_constructedDefId) / buildspeed;

					if (buildtime > static_cast<float>(cfg->MIN_ASSISTANCE_BUILDTIME))
						assist = true;
				}
			}

			if(assist)
			{
				AAIConstructor* assistant = ai->Getut()->FindClosestAssistant(ai->Getcb()->GetUnitPos(m_myUnitId.id), 5, true);

				if(assistant)
				{
					assistants.insert(assistant->m_myUnitId.id);
					assistant->AssistConstruction(m_myUnitId);
				}
			}
		}
		// check if assistants are needed anymore
		else if(!assistants.empty() && m_buildqueue->empty() && (m_constructedDefId.isValid() == false))
		{
			//ai->LogConsole("factory releasing assistants");
			ReleaseAllAssistants();
		}

	}

	if(m_isBuilder && build_task)
	{
		// prevent assisting when low on ressources
		const SmoothedData& metalSurplus = ai->Getbrain()->GetSmoothedMetalSurplus();
		if(metalSurplus.GetAverageValue() < 0.5f)
		{
			if(ai->Getbt()->s_buildTree.GetUnitCategory(m_constructedDefId).isMetalMaker())
			{
				if(ai->Getexecute()->averageEnergySurplus < 0.5 * ai->Getbt()->GetUnitDef(m_constructedDefId.id).energyUpkeep)
					return;
			}
			else if(   (ai->Getbt()->s_buildTree.GetUnitCategory(m_constructedDefId).isMetalExtractor() == false) 
			        && (ai->Getbt()->s_buildTree.GetUnitCategory(m_constructedDefId).isPowerPlant() == false ) )
				return;
		}

		const float buildspeed( ai->Getbt()->s_buildTree.GetBuildspeed(m_myDefId) ); 
		if (buildspeed > 0.0f)
		{
			const float buildtime = ai->Getbt()->s_buildTree.GetBuildtime(m_constructedDefId) / buildspeed;

			if((buildtime > static_cast<float>(cfg->MIN_ASSISTANCE_BUILDTIME)) && (assistants.size() < cfg->MAX_ASSISTANTS))
			{
				// com only allowed if buildpos is inside the base
				bool commander = false;

				int x = m_buildPos.x / ai->Getmap()->xSectorSize;
				int y = m_buildPos.z / ai->Getmap()->ySectorSize;

				if(x >= 0 && y >= 0 && x < ai->Getmap()->xSectors && y < ai->Getmap()->ySectors)
				{
					if(ai->Getmap()->sector[x][y].distance_to_base == 0)
						commander = true;
				}

				AAIConstructor* assistant = ai->Getut()->FindClosestAssistant(m_buildPos, 5, commander);

				if(assistant)
				{
					assistants.insert(assistant->m_myUnitId.id);
					assistant->AssistConstruction(m_myUnitId);
				}
			}
		}
	}
}

void AAIConstructor::GiveReclaimOrder(int unit_id)
{
	if(m_assistUnitId.isValid() == true)
	{
		ai->Getut()->units[m_assistUnitId.id].cons->RemoveAssitant(m_myUnitId.id);
		m_assistUnitId.invalidate();
	}

	m_activity.SetActivity(EConstructorActivity::RECLAIMING);

	Command c(CMD_RECLAIM);
	c.PushParam(unit_id);
	//ai->Getcb()->GiveOrder(this->unit_id, &c);
	ai->Getexecute()->GiveOrder(&c, m_myUnitId.id, "Builder::GiveRelaimOrder");
}


void AAIConstructor::GiveConstructionOrder(int id_building, float3 pos, bool water)
{
	// get def and final position
	const UnitDef *def = &ai->Getbt()->GetUnitDef(id_building);

	// give order if building can be placed at the desired position (position lies within a valid sector)
	if(ai->Getexecute()->InitBuildingAt(def, &pos, water))
	{
		// check if builder was previously assisting other builders/factories
		if(m_assistUnitId.isValid() == true)
		{
			ai->Getut()->units[m_assistUnitId.id].cons->RemoveAssitant(m_myUnitId.id);
			m_assistUnitId.invalidate();
		}

		// set building as current task and order construction
		m_buildPos = pos;
		m_constructedDefId.id = id_building;
		assert(m_constructedUnitId.isValid());
		m_activity.SetActivity(EConstructorActivity::HEADING_TO_BUILDSITE);

		// order builder to construct building
		Command c(-id_building);
		c.PushPos(m_buildPos);

		ai->Getcb()->GiveOrder(m_myUnitId.id, &c);

		// increase number of active units of that type/category
		ai->Getbt()->units_dynamic[def->id].requested += 1;

		ai->Getut()->UnitRequested(ai->Getbt()->s_buildTree.GetUnitCategory(m_constructedDefId));

		if(ai->Getbt()->s_buildTree.GetUnitType(UnitDefId(id_building)).IsFactory())
			ai->Getut()->futureFactories += 1;
	}
}

void AAIConstructor::AssistConstruction(UnitId constructorUnitId)
{
	// Check if the target can be assisted at all. If not, try to repair it instead
	const UnitDef* def = ai->Getcb()->GetUnitDef(constructorUnitId.id);
	Command c(def->canBeAssisted? CMD_GUARD: CMD_REPAIR);
	c.PushParam(constructorUnitId.id);

	//ai->Getcb()->GiveOrder(unit_id, &c);
	ai->Getexecute()->GiveOrder(&c, m_myUnitId.id, "Builder::Assist");

	m_activity.SetActivity(EConstructorActivity::ASSISTING);
	m_assistUnitId = UnitId(constructorUnitId.id);
}

void AAIConstructor::TakeOverConstruction(AAIBuildTask *build_task)
{
	if(m_assistUnitId.isValid())
	{
		ai->Getut()->units[m_assistUnitId.id].cons->RemoveAssitant(m_myUnitId.id);
		m_assistUnitId.invalidate();
	}

	m_constructedDefId.id  = build_task->def_id;
	m_constructedUnitId.id = build_task->unit_id;
	assert(m_constructedDefId.isValid());
	assert(m_constructedUnitId.isValid());

	m_buildPos = build_task->build_pos;

	Command c(CMD_REPAIR);
	c.PushParam(build_task->unit_id);

	m_activity.SetActivity(EConstructorActivity::CONSTRUCTING);
	ai->Getcb()->GiveOrder(m_myUnitId.id, &c);
}

void AAIConstructor::CheckIfConstructionFailed()
{
	if( (m_activity.IsCarryingOutConstructionOrder() == true) && (m_constructedUnitId.isValid() == false))
	{
		ConstructionFailed();
	}
}


void AAIConstructor::ConstructionFailed()
{
	--ai->Getbt()->units_dynamic[m_constructedDefId.id].requested;
	ai->Getut()->UnitRequestFailed(ai->Getbt()->s_buildTree.GetUnitCategory(m_constructedDefId));

	// clear up buildmap etc.
	if(ai->Getbt()->s_buildTree.GetMovementType(m_constructedDefId).isStatic() == true)
		ai->Getexecute()->ConstructionFailed(m_buildPos, m_constructedDefId.id);

	// tells the builder construction has finished
	ConstructionFinished();
}


void AAIConstructor::ConstructionStarted(UnitId unitId, AAIBuildTask *buildTask)
{
	m_constructedUnitId = unitId;
	build_task = buildTask;
	m_activity.SetActivity(EConstructorActivity::CONSTRUCTING);
	CheckAssistance();
}

void AAIConstructor::ConstructionFinished()
{
  	m_activity.SetActivity(EConstructorActivity::IDLE);

	m_buildPos = ZeroVector;
	m_constructedUnitId.invalidate();
	m_constructedDefId.invalidate();

	build_task = nullptr;

	// release assisters
	ReleaseAllAssistants();
}

void AAIConstructor::ReleaseAllAssistants()
{
	// release assisters
	for(set<int>::iterator i = assistants.begin(); i != assistants.end(); ++i)
	{
		 if(ai->Getut()->units[*i].cons)
			 ai->Getut()->units[*i].cons->StopAssisting();
	}

	assistants.clear();
}

void AAIConstructor::StopAssisting()
{
	m_activity.SetActivity(EConstructorActivity::IDLE);
	m_assistUnitId.invalidate();

	Command c(CMD_STOP);
	//ai->Getcb()->GiveOrder(unit_id, &c);
	ai->Getexecute()->GiveOrder(&c, m_myUnitId.id, "Builder::StopAssisting");
}
void AAIConstructor::RemoveAssitant(int unit_id)
{
	assistants.erase(unit_id);
}
void AAIConstructor::Killed()
{
	// when builder was killed on the way to the buildsite, inform ai that construction
	// of building hasnt been started
	if(m_activity.IsHeadingToBuildsite() == true)
	{
		// clear up buildmap etc.
		ConstructionFailed();
	}
	else if(m_activity.IsConstructing() == true)
	{
		if(build_task)
			build_task->BuilderDestroyed();
	}
	else if(m_activity.IsAssisting() == true)
	{
			ai->Getut()->units[m_assistUnitId.id].cons->RemoveAssitant(m_myUnitId.id);
	}

	ReleaseAllAssistants();
	m_activity.SetActivity(EConstructorActivity::DESTROYED);
}

void AAIConstructor::Retreat(const AAIUnitCategory& attackedByCategory)
{
	if(m_activity.IsDestroyed() == false)
	{
		float3 pos = ai->Getcb()->GetUnitPos(m_myUnitId.id);

		int x = pos.x / ai->Getmap()->xSectorSize;
		int y = pos.z / ai->Getmap()->ySectorSize;

		// attacked by scout
		if(attackedByCategory.isScout() == true)
		{
			// dont flee from scouts in your own base
			if(x >= 0 && y >= 0 && x < ai->Getmap()->xSectors && y < ai->Getmap()->ySectors)
			{
				// builder is within base
				if(ai->Getmap()->sector[x][y].distance_to_base == 0)
					return;
				// dont flee outside of the base if health is > 50%
				else
				{
					if(ai->Getcb()->GetUnitHealth(m_myUnitId.id) > ai->Getbt()->GetUnitDef(m_myDefId.id).health / 2.0f)
						return;
				}
			}
		}
		else
		{
			if(x >= 0 && y >= 0 && x < ai->Getmap()->xSectors && y < ai->Getmap()->ySectors)
			{
				// builder is within base
				if(ai->Getmap()->sector[x][y].distance_to_base == 0)
					return;
			}
		}


		// get safe position
		pos = ai->Getexecute()->determineSafePos(m_myDefId, pos);

		if(pos.x > 0)
		{
			Command c(CMD_MOVE);
			c.PushParam(pos.x);
			c.PushParam(ai->Getcb()->GetElevation(pos.x, pos.z));
			c.PushParam(pos.z);

			ai->Getexecute()->GiveOrder(&c, m_myUnitId.id, "BuilderRetreat");
			//ai->Getcb()->GiveOrder(unit_id, &c);
		}
	}
}
