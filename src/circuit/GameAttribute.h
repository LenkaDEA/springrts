/*
 * GameAttribute.h
 *
 *  Created on: Aug 12, 2014
 *      Author: rlcevg
 */

#ifndef GAMEATTRIBUTE_H_
#define GAMEATTRIBUTE_H_

#include "RagMatrix.h"

#include <memory>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <string.h>

namespace springai {
	class GameRulesParam;
	class UnitDef;
	class Pathing;
}

namespace circuit {

class CCircuitAI;
class CSetupManager;
class CMetalManager;
class CScheduler;
class CGameTask;

class CGameAttribute {
private:
	struct cmp_str {
	   bool operator()(char const *a, char const *b) {
	      return strcmp(a, b) < 0;
	   }
	};

public:
	enum class StartPosType: char {METAL_SPOT = 0, MIDDLE = 1, RANDOM = 2};
	using UnitDefs = std::map<const char*, springai::UnitDef*, cmp_str>;

public:
	CGameAttribute();
	virtual ~CGameAttribute();

	void SetGameEnd(bool value);
	bool IsGameEnd();
	void RegisterAI(CCircuitAI* circuit);
	void UnregisterAI(CCircuitAI* circuit);

	void ParseSetupScript(const char* setupScript, int width, int height);
	bool HasStartBoxes(bool checkEmpty = true);
	bool CanChooseStartPos();
	void PickStartPos(CCircuitAI* circuit, StartPosType type);
	CSetupManager& GetSetupManager();

	void ParseMetalSpots(const char* metalJson);
	void ParseMetalSpots(const std::vector<springai::GameRulesParam*>& metalParams);
	bool HasMetalSpots(bool checkEmpty = true);
	bool HasMetalClusters();
	void ClusterizeMetalFirst(std::shared_ptr<CScheduler> scheduler, float maxDistance, int pathType, springai::Pathing* pathing);
	void ClusterizeMetal(std::shared_ptr<CScheduler> scheduler, float maxDistance, int pathType, springai::Pathing* pathing);
	CMetalManager& GetMetalManager();

	void InitUnitDefs(std::vector<springai::UnitDef*>&& unitDefs);
	bool HasUnitDefs();
	springai::UnitDef* GetUnitDefByName(const char* name);
	springai::UnitDef* GetUnitDefById(int unitDefId);
	UnitDefs& GetUnitDefs();

private:
	bool gameEnd;
	std::unordered_set<CCircuitAI*> circuits;
	std::shared_ptr<CSetupManager> setupManager;
	std::shared_ptr<CMetalManager> metalManager;

	UnitDefs defsByName;  // owner
	std::unordered_map<int, springai::UnitDef*> defsById;

	struct {
		int i;
		std::shared_ptr<CRagMatrix> matrix;
		springai::Pathing* pathing;
		int pathType;
		std::weak_ptr<CScheduler> schedWeak;
		float maxDistance;
		std::shared_ptr<CGameTask> task;
	} tmpDistStruct;
	void FillDistMatrix();
};

} // namespace circuit

#endif // GAMEATTRIBUTE_H_
