/*
 * FactoryManager.cpp
 *
 *  Created on: Dec 1, 2014
 *      Author: rlcevg
 */

#include "module/FactoryManager.h"
#include "module/EconomyManager.h"
#include "terrain/TerrainManager.h"
#include "task/NullTask.h"
#include "task/IdleTask.h"
#include "task/static/RepairTask.h"
#include "task/static/ReclaimTask.h"
#include "CircuitAI.h"
#include "util/Scheduler.h"
#include "util/utils.h"

#include "AIFloat3.h"
#include "OOAICallback.h"
#include "Command.h"

namespace circuit {

using namespace springai;

CFactoryManager::CFactoryManager(CCircuitAI* circuit) :
		IUnitModule(circuit),
		factoryPower(.0f),
		assistDef(nullptr)
{
	CScheduler* scheduler = circuit->GetScheduler().get();
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CFactoryManager::Watchdog, this),
							FRAMES_PER_SEC * 60,
							circuit->GetSkirmishAIId() * WATCHDOG_COUNT + 1);
	const int interval = 4;
	const int offset = circuit->GetSkirmishAIId() % interval + circuit->GetSkirmishAIId() * 2;
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CFactoryManager::UpdateIdle, this), interval, offset + 1);
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CFactoryManager::UpdateAssist, this), interval, offset + 4);

	/*
	 * factory handlers
	 */
	auto factoryCreatedHandler = [this](CCircuitUnit* unit, CCircuitUnit* builder) {
		if (unit->GetTask() == nullptr) {
			unit->SetManager(this);
			nullTask->AssignTo(unit);
		}
	};
	auto factoryFinishedHandler = [this](CCircuitUnit* unit) {
		if (unit->GetTask() == nullptr) {
			unit->SetManager(this);
			idleTask->AssignTo(unit);
		} else {
			nullTask->RemoveAssignee(unit);
		}

		factoryPower += unit->GetCircuitDef()->GetUnitDef()->GetBuildSpeed();

		// check nanos around
		std::set<CCircuitUnit*> nanos;
		float radius = assistDef->GetBuildDistance();
		auto units = std::move(this->circuit->GetCallback()->GetFriendlyUnitsIn(unit->GetUnit()->GetPos(), radius));
		int nanoId = assistDef->GetId();
		int teamId = this->circuit->GetTeamId();
		for (auto nano : units) {
			if (nano == nullptr) {
				continue;
			}
			UnitDef* ndef = nano->GetDef();
			if (ndef->GetUnitDefId() == nanoId && nano->GetTeam() == teamId) {
				CCircuitUnit* ass = this->circuit->GetTeamUnit(nano->GetUnitId());
				nanos.insert(ass);

				std::set<CCircuitUnit*>& facs = assists[ass];
				if (facs.empty()) {
					factoryPower += ass->GetCircuitDef()->GetUnitDef()->GetBuildSpeed();
				}
				facs.insert(unit);
			}
			delete ndef;
		}
		utils::free_clear(units);
		factories.emplace_back(unit, nanos, 3);
	};
	auto factoryIdleHandler = [this](CCircuitUnit* unit) {
		unit->GetTask()->OnUnitIdle(unit);
	};
	auto factoryDestroyedHandler = [this](CCircuitUnit* unit, CCircuitUnit* attacker) {
		unit->GetTask()->OnUnitDestroyed(unit, attacker);  // can change task
		unit->GetTask()->RemoveAssignee(unit);  // Remove unit from IdleTask

		if (unit->GetTask() == nullTask) {  // alternative: unit->GetUnit()->IsBeingBuilt()
			return;
		}
		factoryPower -= unit->GetCircuitDef()->GetUnitDef()->GetBuildSpeed();
		for (auto it = factories.begin(); it != factories.end(); ++it) {
			if (it->unit != unit) {
				continue;
			}
			for (CCircuitUnit* ass : it->nanos) {
				std::set<CCircuitUnit*>& facs = assists[ass];
				facs.erase(unit);
				if (facs.empty()) {
					factoryPower -= ass->GetCircuitDef()->GetUnitDef()->GetBuildSpeed();
				}
			}
			factories.erase(it);
			break;
		}
	};

	/*
	 * armnanotc handlers
	 */
	auto assistCreatedHandler = [this](CCircuitUnit* unit, CCircuitUnit* builder) {
		if (unit->GetTask() == nullptr) {
			unit->SetManager(this);
			nullTask->AssignTo(unit);
		}
	};
	auto assistFinishedHandler = [this](CCircuitUnit* unit) {
		if (unit->GetTask() == nullptr) {
			unit->SetManager(this);
			idleTask->AssignTo(unit);
		} else {
			nullTask->RemoveAssignee(unit);
		}

		Unit* u = unit->GetUnit();
		const AIFloat3& assPos = u->GetPos();

		std::vector<float> params;
		params.push_back(0.0f);
		u->ExecuteCustomCommand(CMD_PRIORITY, params);

		// check factory nano belongs to
		float radius = unit->GetCircuitDef()->GetBuildDistance();
		float qradius = radius * radius;
		std::set<CCircuitUnit*>& facs = assists[unit];
		for (SFactory& fac : factories) {
			if (assPos.SqDistance2D(fac.unit->GetUnit()->GetPos()) >= qradius) {
				continue;
			}
			fac.nanos.insert(unit);
			facs.insert(fac.unit);
		}
		if (!facs.empty()) {
			factoryPower += unit->GetCircuitDef()->GetUnitDef()->GetBuildSpeed();

			bool isInHaven = false;
			for (const AIFloat3& hav : havens) {
				if (assPos.SqDistance2D(hav) < qradius) {
					isInHaven = true;
					break;
				}
			}
			if (!isInHaven) {
				havens.push_back(assPos);
				// TODO: Send HavenFinished message?
			}
		}
	};
	auto assistIdleHandler = [this](CCircuitUnit* unit) {
		unit->GetTask()->OnUnitIdle(unit);
	};
	auto assistDestroyedHandler = [this](CCircuitUnit* unit, CCircuitUnit* attacker) {
		unit->GetTask()->OnUnitDestroyed(unit, attacker);  // can change task
		unit->GetTask()->RemoveAssignee(unit);  // Remove unit from IdleTask

		if (unit->GetTask() == nullTask) {  // alternative: unit->GetUnit()->IsBeingBuilt()
			return;
		}
		const AIFloat3& assPos = unit->GetUnit()->GetPos();
		float radius = unit->GetCircuitDef()->GetBuildDistance();
		float qradius = radius * radius;
		for (SFactory& fac : factories) {
			if ((fac.nanos.erase(unit) == 0) || !fac.nanos.empty()) {
				continue;
			}
			auto it = havens.begin();
			while (it != havens.end()) {
				if (it->SqDistance2D(assPos) < qradius) {
					it = havens.erase(it);
					// TODO: Send HavenDestroyed message?
				} else {
					++it;
				}
			}
		}
		if (!assists[unit].empty()) {
			factoryPower -= unit->GetCircuitDef()->GetUnitDef()->GetBuildSpeed();
		}
		assists.erase(unit);
	};

	CCircuitAI::CircuitDefs& defs = circuit->GetCircuitDefs();
	for (auto& kv : defs) {
		CCircuitDef* cdef = kv.second;
		UnitDef* def = cdef->GetUnitDef();
		if (def->IsBuilder() && (def->GetSpeed() == 0)) {
			CCircuitDef::Id unitDefId = kv.first;
			if  (!kv.second->GetBuildOptions().empty()) {
				createdHandler[unitDefId] = factoryCreatedHandler;
				finishedHandler[unitDefId] = factoryFinishedHandler;
				idleHandler[unitDefId] = factoryIdleHandler;
				destroyedHandler[unitDefId] = factoryDestroyedHandler;
			} else {
				createdHandler[unitDefId] = assistCreatedHandler;
				finishedHandler[unitDefId] = assistFinishedHandler;
				idleHandler[unitDefId] = assistIdleHandler;
				destroyedHandler[unitDefId] = assistDestroyedHandler;
				assistDef = cdef;
			}
		}
	}

	// FIXME: EXPERIMENTAL
	/*
	 * striderhub handlers
	 */
	CCircuitDef::Id defId = circuit->GetCircuitDef("striderhub")->GetId();
	finishedHandler[defId] = [this, defId](CCircuitUnit* unit) {
		unit->SetManager(this);

		factoryPower += unit->GetCircuitDef()->GetUnitDef()->GetBuildSpeed();
		Unit* u = unit->GetUnit();
		CRecruitTask* task = new CRecruitTask(this, IUnitTask::Priority::HIGH, nullptr, ZeroVector, CRecruitTask::BuildType::FIREPOWER, unit->GetCircuitDef()->GetBuildDistance());
		unit->SetTask(task);

		// check nanos around
		std::set<CCircuitUnit*> nanos;
		float radius = assistDef->GetBuildDistance();
		auto units = std::move(this->circuit->GetCallback()->GetFriendlyUnitsIn(u->GetPos(), radius));
		int nanoId = assistDef->GetId();
		int teamId = this->circuit->GetTeamId();
		for (auto nano : units) {
			if (nano == nullptr) {
				continue;
			}
			UnitDef* ndef = nano->GetDef();
			if (ndef->GetUnitDefId() == nanoId && nano->GetTeam() == teamId) {
				CCircuitUnit* ass = this->circuit->GetTeamUnit(nano->GetUnitId());
				nanos.insert(ass);

				std::set<CCircuitUnit*>& facs = assists[ass];
				if (facs.empty()) {
					factoryPower += ass->GetCircuitDef()->GetUnitDef()->GetBuildSpeed();
				}
				facs.insert(unit);
			}
			delete ndef;
		}
		utils::free_clear(units);
		factories.emplace_back(unit, nanos, 9);

		std::vector<float> params;
		params.push_back(2.0f);
		u->ExecuteCustomCommand(CMD_PRIORITY, params);

//		u->SetRepeat(true);
		idleHandler[defId](unit);
	};
	idleHandler[defId] = [this](CCircuitUnit* unit) {
		const char* names[] = {"armcomdgun", "scorpion", "dante", "armraven", "funnelweb", "armbanth", "armorco"};
		const std::array<float, 7> prob = {.1, .3, .25, .07, .1, .15, .03};
		int choice = 0;
		float dice = rand() / (float)RAND_MAX;
		float total;
		for (int i = 0; i < prob.size(); ++i) {
			total += prob[i];
			if (dice < total) {
				choice = i;
				break;
			}
		}
		CCircuitDef* striderDef = this->circuit->GetCircuitDef(names[choice]);
		AIFloat3 pos = unit->GetUnit()->GetPos();
		pos = this->circuit->GetTerrainManager()->FindBuildSite(striderDef, pos, this->circuit->GetCircuitDef("striderhub")->GetBuildDistance(), -1);
		if (pos != -RgtVector) {
			unit->GetUnit()->Build(striderDef->GetUnitDef(), pos, -1, 0, FRAMES_PER_SEC * 10);
		}
	};
	destroyedHandler[defId] = [this](CCircuitUnit* unit, CCircuitUnit* attacker) {
		if (unit->GetUnit()->IsBeingBuilt()) {
			return;
		}
		factoryPower -= unit->GetCircuitDef()->GetUnitDef()->GetBuildSpeed();
		for (auto it = factories.begin(); it != factories.end(); ++it) {
			if (it->unit != unit) {
				continue;
			}
			for (CCircuitUnit* ass : it->nanos) {
				std::set<CCircuitUnit*>& facs = assists[ass];
				facs.erase(unit);
				if (facs.empty()) {
					factoryPower -= ass->GetCircuitDef()->GetUnitDef()->GetBuildSpeed();
				}
			}
			factories.erase(it);
			break;
		}
		delete unit->GetTask();
	};
	// FIXME: EXPERIMENTAL
}

CFactoryManager::~CFactoryManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	utils::free_clear(factoryTasks);
	utils::free_clear(deleteTasks);
	utils::free_clear(assistTasks);
	utils::free_clear(deleteAssists);
}

int CFactoryManager::UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder)
{
	auto search = createdHandler.find(unit->GetCircuitDef()->GetId());
	if (search != createdHandler.end()) {
		search->second(unit, builder);
	}

	if (builder == nullptr) {
		return 0; //signaling: OK
	}

	IUnitTask* task = builder->GetTask();
	if (task->GetType() != IUnitTask::Type::FACTORY) {
		return 0; //signaling: OK
	}

	if (unit->GetUnit()->IsBeingBuilt()) {
		CRecruitTask* taskR = static_cast<CRecruitTask*>(task);
		if (taskR->GetTarget() == nullptr) {
			taskR->SetTarget(unit);
			unfinishedUnits[unit] = taskR;
		}
	}

	return 0; //signaling: OK
}

int CFactoryManager::UnitFinished(CCircuitUnit* unit)
{
	auto iter = unfinishedUnits.find(unit);
	if (iter != unfinishedUnits.end()) {
		DoneTask(iter->second);
	}

	auto search = finishedHandler.find(unit->GetCircuitDef()->GetId());
	if (search != finishedHandler.end()) {
		search->second(unit);
	}

	return 0; //signaling: OK
}

int CFactoryManager::UnitIdle(CCircuitUnit* unit)
{
	auto search = idleHandler.find(unit->GetCircuitDef()->GetId());
	if (search != idleHandler.end()) {
		search->second(unit);
	}

	return 0; //signaling: OK
}

int CFactoryManager::UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	if (unit->GetUnit()->IsBeingBuilt()) {
		auto iter = unfinishedUnits.find(unit);
		if (iter != unfinishedUnits.end()) {
			AbortTask(iter->second);
		}
	}

	auto search = destroyedHandler.find(unit->GetCircuitDef()->GetId());
	if (search != destroyedHandler.end()) {
		search->second(unit, attacker);
	}

	return 0; //signaling: OK
}

CRecruitTask* CFactoryManager::EnqueueTask(CRecruitTask::Priority priority,
										   CCircuitDef* buildDef,
										   const AIFloat3& position,
										   CRecruitTask::BuildType type,
										   float radius)
{
	CRecruitTask* task = new CRecruitTask(this, priority, buildDef, position, type, radius);
	factoryTasks.insert(task);
	return task;
}

IBuilderTask* CFactoryManager::EnqueueReclaim(IBuilderTask::Priority priority,
											  const springai::AIFloat3& position,
											  float radius,
											  int timeout)
{
	IBuilderTask* task = new CSReclaimTask(this, priority, position, .0f, timeout, radius);
	assistTasks.insert(task);
	return task;
}

IBuilderTask* CFactoryManager::EnqueueRepair(IBuilderTask::Priority priority,
											 CCircuitUnit* target)
{
	IBuilderTask* task = new CSRepairTask(this, priority, target);
	assistTasks.insert(task);
	return task;
}

void CFactoryManager::DequeueTask(IUnitTask* task, bool done)
{
	// TODO: Convert CRecruitTask into IBuilderTask
	auto itf = factoryTasks.find(static_cast<CRecruitTask*>(task));
	if (itf != factoryTasks.end()) {
		unfinishedUnits.erase(static_cast<CRecruitTask*>(task)->GetTarget());
		factoryTasks.erase(itf);
		task->Close(done);
		deleteTasks.insert(static_cast<CRecruitTask*>(task));
		return;
	}
	auto ita = assistTasks.find(static_cast<IBuilderTask*>(task));
	if (ita != assistTasks.end()) {
		assistTasks.erase(ita);
		task->Close(done);
		deleteAssists.insert(static_cast<IBuilderTask*>(task));
	}
}

void CFactoryManager::AssignTask(CCircuitUnit* unit)
{
	if (unit->GetCircuitDef() == assistDef) {  // FIXME: Check Id instead pointers?
		IBuilderTask* task = circuit->GetEconomyManager()->CreateAssistTask(unit);
		if (task != nullptr) {  // if nullptr then continue to Wait (or Idle)
			task->AssignTo(unit);
		}

	} else {

		CRecruitTask* task = nullptr;
		decltype(factoryTasks)::iterator iter = factoryTasks.begin();
		for (; iter != factoryTasks.end(); ++iter) {
			if ((*iter)->CanAssignTo(unit)) {
				task = static_cast<CRecruitTask*>(*iter);
				break;
			}
		}

		if (task == nullptr) {
			task = circuit->GetEconomyManager()->CreateFactoryTask(unit);

//			iter = factoryTasks.begin();
		}

		task->AssignTo(unit);
//		if (task->IsFull()) {
//			factoryTasks.splice(factoryTasks.end(), factoryTasks, iter);  // move task to back
//		}
	}
}

void CFactoryManager::AbortTask(IUnitTask* task)
{
	DequeueTask(task, false);
}

void CFactoryManager::DoneTask(IUnitTask* task)
{
	DequeueTask(task, true);
}

void CFactoryManager::SpecialCleanUp(CCircuitUnit* unit)
{
}

void CFactoryManager::SpecialProcess(CCircuitUnit* unit)
{
}

void CFactoryManager::FallbackTask(CCircuitUnit* unit)
{
}

CCircuitUnit* CFactoryManager::NeedUpgrade()
{
	// TODO: Wrap into predicate
	if (factories.empty()) {
		return nullptr;
	}
	int facSize = factories.size();
	for (auto it = factories.rbegin(); it != factories.rend(); ++it) {
		SFactory& fac = *it;
		if (fac.nanos.size() < facSize * fac.weight) {
			return fac.unit;
		}
	}
	return nullptr;
}

CCircuitUnit* CFactoryManager::GetRandomFactory(CCircuitDef* buildDef)
{
	std::list<CCircuitUnit*> facs;
	for (SFactory& fac : factories) {
		if (fac.unit->GetCircuitDef()->CanBuild(buildDef)) {
			facs.push_back(fac.unit);
		}
	}
	if (facs.empty()) {
		return nullptr;
	}
	auto it = facs.begin();
	std::advance(it, rand() % facs.size());
	return *it;
}

AIFloat3 CFactoryManager::GetClosestHaven(CCircuitUnit* unit) const
{
	if (havens.empty()) {
		return -RgtVector;
	}
	float metric = std::numeric_limits<float>::max();
	const AIFloat3& position = unit->GetUnit()->GetPos();
	CTerrainManager* terrain = circuit->GetTerrainManager();
	auto it = havens.begin(), havIt = havens.end();
	for (; it != havens.end(); ++it) {
		if (!terrain->CanMoveToPos(unit->GetArea(), *it)) {
			continue;
		}
		float qdist = it->SqDistance2D(position);
		if (qdist < metric) {
			havIt = it;
			metric = qdist;
		}
	}
	return (havIt != havens.end()) ? *havIt : AIFloat3(-RgtVector);
}

void CFactoryManager::Watchdog()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	auto checkIdler = [this](CCircuitUnit* unit) {
		auto commands = std::move(unit->GetUnit()->GetCurrentCommands());
		if (commands.empty()) {
			UnitIdle(unit);
		}
		utils::free_clear(commands);
	};

	for (auto& fac : factories) {
		checkIdler(fac.unit);
	}

	for (auto& kv : assists) {
		checkIdler(kv.first);
	}
}

void CFactoryManager::UpdateIdle()
{
	idleTask->Update();
}

void CFactoryManager::UpdateAssist()
{
	utils::free_clear(deleteTasks);
	if (!deleteAssists.empty()) {
		for (auto task : deleteAssists) {
			updateAssists.erase(task);
			delete task;
		}
		deleteAssists.clear();
	}

	auto it = updateAssists.begin();
	unsigned int i = 0;
	while (it != updateAssists.end()) {
		(*it)->Update();

		it = updateAssists.erase(it);
		if (++i >= updateSlice) {
			break;
		}
	}

	if (updateAssists.empty()) {
		updateAssists = assistTasks;
		updateSlice = updateAssists.size() / TEAM_SLOWUPDATE_RATE;
	}
}

} // namespace circuit
