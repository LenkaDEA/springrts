// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#include "System/SafeUtil.h"
#include "AAIBuildTable.h"
#include "AAI.h"
#include "AAIBrain.h"
#include "AAIExecute.h"
#include "AAIUnitTable.h"
#include "AAIConfig.h"
#include "AAIMap.h"

#include "LegacyCpp/UnitDef.h"
#include "LegacyCpp/MoveData.h"
using namespace springLegacyAI;


AttackedByRatesPerGamePhaseAndMapType AAIBuildTable::s_attackedByRates;

AAIBuildTable::AAIBuildTable(AAI* ai)
{
	this->ai = ai;

	numOfSides = cfg->SIDES;
	sideNames.resize(numOfSides+1);
	sideNames[0] = "Neutral";

	for(int i = 0; i < numOfSides; ++i)
	{
		sideNames[i+1].assign(cfg->SIDE_NAMES[i]);
	}
}

AAIBuildTable::~AAIBuildTable(void)
{
	unitList.clear();
}

void AAIBuildTable::Init()
{
	// get number of units and alloc memory for unit list
	const int numOfUnits = ai->GetAICallback()->GetNumUnitDefs();

	// one more than needed because 0 is dummy object (so UnitDef->id can be used to adress that unit in the array)
	units_dynamic.resize(numOfUnits+1);

	for(int i = 0; i <= numOfUnits; ++i)
	{
		units_dynamic[i].active = 0;
		units_dynamic[i].requested = 0;
		units_dynamic[i].constructorsAvailable = 0;
		units_dynamic[i].constructorsRequested = 0;
	}

	// get unit defs from game
	if(unitList.empty())
	{
		//spring first unitdef id is 1, we remap it so id = is position in array
		unitList.resize(numOfUnits+1);
		ai->GetAICallback()->GetUnitDefList(&unitList[1]);
		UnitDef* tmp = new UnitDef();
		tmp->id=0;
		unitList[0] = tmp;
		#ifndef NDEBUG
		for(int i=0; i<numOfUnits; i++) {
			assert(i == GetUnitDef(i).id);
		}
		#endif
	}

	// If first instance of AAI: Try to load combat power&attacked by rates; if no stored data availble init with default values
	// (combat power and attacked by rates are both static)
	if(ai->GetAAIInstance() == 1)
	{
		if(LoadModLearnData() == false)
		{
			ai->s_buildTree.InitCombatPowerOfUnits(ai->GetAICallback());
			ai->LogConsole("New BuildTable has been created");
		}
	}
}

void AAIBuildTable::ConstructorRequested(UnitDefId constructor)
{
	for(std::list<UnitDefId>::const_iterator id = ai->s_buildTree.GetCanConstructList(constructor).begin();  id != ai->s_buildTree.GetCanConstructList(constructor).end(); ++id)
	{
		++units_dynamic[(*id).id].constructorsRequested;
	}
}

void AAIBuildTable::ConstructorFinished(UnitDefId constructor)
{
	for(std::list<UnitDefId>::const_iterator id = ai->s_buildTree.GetCanConstructList(constructor).begin();  id != ai->s_buildTree.GetCanConstructList(constructor).end(); ++id)
	{
		++units_dynamic[(*id).id].constructorsAvailable;
		--units_dynamic[(*id).id].constructorsRequested;
	}
}

void AAIBuildTable::ConstructorKilled(UnitDefId constructor)
{
	for(std::list<UnitDefId>::const_iterator id = ai->s_buildTree.GetCanConstructList(constructor).begin();  id != ai->s_buildTree.GetCanConstructList(constructor).end(); ++id)
	{
		--units_dynamic[(*id).id].constructorsAvailable;
	}
}

void AAIBuildTable::UnfinishedConstructorKilled(UnitDefId constructor)
{
	for(std::list<UnitDefId>::const_iterator id = ai->s_buildTree.GetCanConstructList(constructor).begin();  id != ai->s_buildTree.GetCanConstructList(constructor).end(); ++id)
	{
		--units_dynamic[(*id).id].constructorsRequested;
	}
}

bool AAIBuildTable::IsBuildingSelectable(UnitDefId building, bool water, bool mustBeConstructable) const
{
	const bool constructablePassed = !mustBeConstructable || (units_dynamic[building.id].constructorsAvailable > 0);
	const bool landCheckPassed     = !water    && ai->s_buildTree.GetMovementType(building.id).IsStaticLand();
	const bool seaCheckPassed      =  water    && ai->s_buildTree.GetMovementType(building.id).IsStaticSea();

	return constructablePassed && (landCheckPassed || seaCheckPassed );
}

UnitDefId AAIBuildTable::SelectPowerPlant(int side, float cost, float buildtime, float powerGeneration, bool water)
{
	UnitDefId powerPlant = SelectPowerPlant(side, cost, buildtime, powerGeneration, water, false);

	if(powerPlant.IsValid() && (units_dynamic[powerPlant.id].constructorsAvailable + units_dynamic[powerPlant.id].constructorsRequested <= 0) )
	{
		ai->Getbt()->RequestBuilderFor(powerPlant);
		powerPlant = SelectPowerPlant(side, cost, buildtime, powerGeneration, water, true);
	}

	return powerPlant;
}

UnitDefId AAIBuildTable::SelectPowerPlant(int side, float cost, float buildtime, float powerGeneration, bool water, bool mustBeConstructable) const
{
	UnitDefId selectedPowerPlant;

	float     bestRating(0.0f);

	const AAIUnitStatistics& unitStatistics  = ai->s_buildTree.GetUnitStatistics(side);
	const StatisticalData&   generatedPowers = unitStatistics.GetUnitPrimaryAbilityStatistics(EUnitCategory::POWER_PLANT);
	const StatisticalData&   buildtimes      = unitStatistics.GetUnitBuildtimeStatistics(EUnitCategory::POWER_PLANT);
	const StatisticalData&   costs           = unitStatistics.GetUnitCostStatistics(EUnitCategory::POWER_PLANT);

	for(auto powerPlant = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::POWER_PLANT, side).begin(); powerPlant != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::POWER_PLANT, side).end(); ++powerPlant)
	{
				// check if under water or ground || water = true and building under water
		if( IsBuildingSelectable(*powerPlant, water, mustBeConstructable) )
		{
			const float generatedPower = ai->s_buildTree.GetMaxRange( *powerPlant );

			float myRating =   powerGeneration * generatedPowers.GetNormalizedDeviationFromMin(generatedPower)
						     + cost            * costs.GetNormalizedDeviationFromMax(ai->s_buildTree.GetTotalCost(*powerPlant))
							 + buildtime       * buildtimes.GetNormalizedDeviationFromMax(ai->s_buildTree.GetBuildtime(*powerPlant));

			if(myRating > bestRating)
			{
				bestRating = myRating;
				selectedPowerPlant = *powerPlant;
			}
		}
	}

	return selectedPowerPlant;
}

UnitDefId AAIBuildTable::SelectExtractor(int side, float cost, float extractedMetal, bool armed, bool water)
{
	UnitDefId extractor = SelectExtractor(side, cost, extractedMetal, armed, water, false);

	if(extractor.IsValid() && (units_dynamic[extractor.id].constructorsAvailable <= 0) && (units_dynamic[extractor.id].constructorsRequested <= 0) )
	{
		ai->Getbt()->RequestBuilderFor(extractor);
		extractor = SelectExtractor(side, cost, extractedMetal, armed, water, true);
	}

	return extractor;
}

UnitDefId AAIBuildTable::SelectExtractor(int side, float cost, float extractedMetal, bool armed, bool water, bool mustBeConstructable) const
{
	UnitDefId selectedExtractorDefId;
	float     bestRating(0.0f);

	const AAIUnitStatistics& unitStatistics           = ai->s_buildTree.GetUnitStatistics(side);
	const StatisticalData&   extractedMetalStatistics = unitStatistics.GetUnitPrimaryAbilityStatistics(EUnitCategory::METAL_EXTRACTOR);
	const StatisticalData&   costStatistics           = unitStatistics.GetUnitCostStatistics(EUnitCategory::METAL_EXTRACTOR);

	for(auto extractorDefId = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::METAL_EXTRACTOR, side).begin(); extractorDefId != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::METAL_EXTRACTOR, side).end(); ++extractorDefId)
	{
		// check if under water or ground || water = true and building under water
		if( IsBuildingSelectable(*extractorDefId, water, mustBeConstructable) )
		{
			const float metalExtraction = ai->s_buildTree.GetMaxRange( *extractorDefId );

			float myRating =   extractedMetal * extractedMetalStatistics.GetNormalizedDeviationFromMin(metalExtraction)
						     + cost           * costStatistics.GetNormalizedDeviationFromMax(ai->s_buildTree.GetTotalCost(*extractorDefId));

			if(armed && !GetUnitDef(extractorDefId->id).weapons.empty())
				myRating += 0.2f;

			if(myRating > bestRating)
			{
				bestRating = myRating;
				selectedExtractorDefId = *extractorDefId;
			}
		}
	}

	// 0 if no unit found (list was probably empty)
	return selectedExtractorDefId.id;
}

UnitDefId AAIBuildTable::GetLargestExtractor() const
{
	UnitDefId largestExtractor;
	int largestYardMap(0);

	for(int side = 1; side <= cfg->SIDES; ++side)
	{
		for(auto extractor = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::METAL_EXTRACTOR, side).begin(); extractor != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::METAL_EXTRACTOR, side).end(); ++extractor)
		{
			const int yardMap = GetUnitDef(extractor->id).xsize * GetUnitDef(extractor->id).zsize;
			
			if(yardMap > largestYardMap)
			{
				largestYardMap   = yardMap;
				largestExtractor = *extractor;
			}
		}
	}

	return largestExtractor;
}

UnitDefId AAIBuildTable::SelectStorage(int side, float cost, float buildtime, float metal, float energy, bool water)
{
	UnitDefId selectedStorage = SelectStorage(side, cost, buildtime, metal, energy, water, false);

	if(selectedStorage.IsValid() && (units_dynamic[selectedStorage.id].constructorsAvailable <= 0))
	{
		if(units_dynamic[selectedStorage.id].constructorsRequested <= 0)
			RequestBuilderFor(selectedStorage);

		selectedStorage = SelectStorage(side, cost, buildtime, metal, energy, water, true);
	}

	return selectedStorage;
}

UnitDefId AAIBuildTable::SelectStorage(int side, float cost, float buildtime, float metal, float energy, bool water, bool mustBeConstructable) const
{
	const AAIUnitStatistics& unitStatistics  = ai->s_buildTree.GetUnitStatistics(side);
	const StatisticalData&   costs           = unitStatistics.GetUnitCostStatistics(EUnitCategory::STORAGE);
	const StatisticalData&   buildtimes      = unitStatistics.GetUnitBuildtimeStatistics(EUnitCategory::STORAGE);
	const StatisticalData&   metalStored     = unitStatistics.GetUnitPrimaryAbilityStatistics(EUnitCategory::STORAGE);
	const StatisticalData&   energyStored    = unitStatistics.GetUnitSecondaryAbilityStatistics(EUnitCategory::STORAGE);

	UnitDefId selectedStorage;
	float bestRating(0.0f);

	for(auto storage = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STORAGE, side).begin(); storage != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STORAGE, side).end(); ++storage)
	{
		
		if( IsBuildingSelectable(storage->id, water, mustBeConstructable) )
		{
			const float myRating =    cost * costs.GetNormalizedDeviationFromMax( ai->s_buildTree.GetTotalCost(*storage) )
									+ buildtime * buildtimes.GetNormalizedDeviationFromMax( ai->s_buildTree.GetBuildtime(*storage) )
									+ metal * metalStored.GetNormalizedDeviationFromMin( ai->s_buildTree.GetMaxRange(*storage) )
									+ energy * energyStored.GetNormalizedDeviationFromMin( ai->s_buildTree.GetMaxSpeed(*storage) );

			if(myRating > bestRating)
			{
				bestRating = myRating;
				selectedStorage = *storage;
			}
		}
	}

	return selectedStorage;
}

UnitDefId AAIBuildTable::GetMetalMaker(int side, float cost, float efficiency, float metal, float urgency, bool water, bool canBuild) const
{
	UnitDefId selectedMetalMaker;

	for(auto maker = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::METAL_MAKER, side).begin(); maker != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::METAL_MAKER, side).end(); ++maker)
	{
		//! @todo reimplement selection of metal makers
	}

	return selectedMetalMaker;
}

void AAIBuildTable::DetermineCombatPowerWeights(MobileTargetTypeValues& combatPowerWeights, const AAIMapType& mapType) const
{
	combatPowerWeights.SetValueForTargetType(ETargetType::AIR,     0.1f + s_attackedByRates.GetAttackedByRateUntilEarlyPhase(mapType, ETargetType::AIR));
	combatPowerWeights.SetValueForTargetType(ETargetType::SURFACE, 1.0f + s_attackedByRates.GetAttackedByRateUntilEarlyPhase(mapType, ETargetType::SURFACE));
	
	if(!mapType.IsLandMap())
	{
		combatPowerWeights.SetValueForTargetType(ETargetType::FLOATER,   1.0f + s_attackedByRates.GetAttackedByRateUntilEarlyPhase(mapType, ETargetType::FLOATER));
		combatPowerWeights.SetValueForTargetType(ETargetType::SUBMERGED, 0.75f + s_attackedByRates.GetAttackedByRateUntilEarlyPhase(mapType, ETargetType::SUBMERGED));
	}
}

void AAIBuildTable::CalculateFactoryRating(FactoryRatingInputData& ratingData, const UnitDefId factoryDefId, const MobileTargetTypeValues& combatPowerWeights, const AAIMapType& mapType) const
{
	ratingData.canConstructBuilder = false;
	ratingData.canConstructScout   = false;
	ratingData.factoryDefId        = factoryDefId;

	MobileTargetTypeValues combatPowerOfConstructedUnits;
	int         combatUnits(0);

	const bool considerLand  = !mapType.IsWaterMap();
	const bool considerWater = !mapType.IsLandMap();

	//-----------------------------------------------------------------------------------------------------------------
	// go through buildoptions to determine input values for calculation of factory rating
	//-----------------------------------------------------------------------------------------------------------------

	for(auto unit = ai->s_buildTree.GetCanConstructList(factoryDefId).begin(); unit != ai->s_buildTree.GetCanConstructList(factoryDefId).end(); ++unit)
	{
		const AAICombatPower& combatPowerOfUnit = ai->s_buildTree.GetCombatPower(*unit);

		switch(ai->s_buildTree.GetUnitCategory(*unit).GetUnitCategory())
		{
			case EUnitCategory::GROUND_COMBAT:
				combatPowerOfConstructedUnits.AddValueForTargetType(ETargetType::SURFACE, combatPowerOfUnit.GetCombatPowerVsTargetType(ETargetType::SURFACE));
				combatPowerOfConstructedUnits.AddValueForTargetType(ETargetType::AIR,     combatPowerOfUnit.GetCombatPowerVsTargetType(ETargetType::AIR));
				++combatUnits;
				break;
			case EUnitCategory::AIR_COMBAT:     // same calculation as for hover
			case EUnitCategory::HOVER_COMBAT:
				combatPowerOfConstructedUnits.AddValueForTargetType(ETargetType::SURFACE, combatPowerOfUnit.GetCombatPowerVsTargetType(ETargetType::SURFACE));
				combatPowerOfConstructedUnits.AddValueForTargetType(ETargetType::AIR,     combatPowerOfUnit.GetCombatPowerVsTargetType(ETargetType::AIR));
				combatPowerOfConstructedUnits.AddValueForTargetType(ETargetType::FLOATER, combatPowerOfUnit.GetCombatPowerVsTargetType(ETargetType::FLOATER));
				++combatUnits;
				break;
			case EUnitCategory::SEA_COMBAT:
				combatPowerOfConstructedUnits.AddValueForTargetType(ETargetType::SURFACE,   combatPowerOfUnit.GetCombatPowerVsTargetType(ETargetType::SURFACE));
				combatPowerOfConstructedUnits.AddValueForTargetType(ETargetType::AIR,       combatPowerOfUnit.GetCombatPowerVsTargetType(ETargetType::AIR));
				combatPowerOfConstructedUnits.AddValueForTargetType(ETargetType::FLOATER,   combatPowerOfUnit.GetCombatPowerVsTargetType(ETargetType::FLOATER));
				combatPowerOfConstructedUnits.AddValueForTargetType(ETargetType::SUBMERGED, combatPowerOfUnit.GetCombatPowerVsTargetType(ETargetType::SUBMERGED));
				++combatUnits;
				break;
			case EUnitCategory::SUBMARINE_COMBAT:
				combatPowerOfConstructedUnits.AddValueForTargetType(ETargetType::FLOATER,   combatPowerOfUnit.GetCombatPowerVsTargetType(ETargetType::FLOATER));
				combatPowerOfConstructedUnits.AddValueForTargetType(ETargetType::SUBMERGED, combatPowerOfUnit.GetCombatPowerVsTargetType(ETargetType::SUBMERGED));
				++combatUnits;
				break;
			case EUnitCategory::MOBILE_CONSTRUCTOR:
				if( ai->s_buildTree.GetMovementType(*unit).IsSeaUnit() )
				{
					if(considerWater)
						ratingData.canConstructBuilder = true;
				}
				else if(ai->s_buildTree.GetMovementType(*unit).IsGround() )
				{
					if(considerLand)
						ratingData.canConstructBuilder = true;
				}
				else // always consider hover, air, or amphibious
				{
					ratingData.canConstructBuilder = true;
				}
				break;
			case EUnitCategory::SCOUT:
				if( ai->s_buildTree.GetMovementType(*unit).IsSeaUnit() )
				{
					if(considerWater)
						ratingData.canConstructScout = true;
				}
				else if(ai->s_buildTree.GetMovementType(*unit).IsGround() )
				{
					if(considerLand)
						ratingData.canConstructScout = true;
				}
				else // always consider hover, air, or amphibious
				{
					ratingData.canConstructScout = true;
				}
				break;
		}
	}

	//-----------------------------------------------------------------------------------------------------------------
	// calculate rating
	//-----------------------------------------------------------------------------------------------------------------
	if(combatUnits > 0)
	{
		ratingData.combatPowerRating = combatPowerOfConstructedUnits.CalculateWeightedSum(combatPowerWeights);
		ratingData.combatPowerRating /= static_cast<float>(combatUnits);
	}
}

UnitDefId AAIBuildTable::SelectStaticDefence(int side, const StaticDefenceSelectionCriteria& selectionCriteria, bool water)
{
	UnitDefId selectedDefence = SelectStaticDefence(side, selectionCriteria, water, false);

	if(selectedDefence.IsValid() && (units_dynamic[selectedDefence.id].constructorsAvailable <= 0))
	{
		if(units_dynamic[selectedDefence.id].constructorsRequested <= 0)
			RequestBuilderFor(selectedDefence);

		selectedDefence = SelectStaticDefence(side, selectionCriteria, false, true);
	}

	return selectedDefence;
}

UnitDefId AAIBuildTable::SelectStaticDefence(int side, const StaticDefenceSelectionCriteria& selectionCriteria, bool water, bool mustBeConstructable) const
{
	// get data needed for selection
	AAIUnitCategory category(EUnitCategory::STATIC_DEFENCE);
	const std::list<UnitDefId> unitList = ai->s_buildTree.GetUnitsInCategory(category, side);

	const StatisticalData& costs      = ai->s_buildTree.GetUnitStatistics(side).GetUnitCostStatistics(category);
	const StatisticalData& ranges     = ai->s_buildTree.GetUnitStatistics(side).GetUnitPrimaryAbilityStatistics(category);
	const StatisticalData& buildtimes = ai->s_buildTree.GetUnitStatistics(side).GetUnitBuildtimeStatistics(category);

	// calculate combat power
	StatisticalData combatPowerStat;		

	for(auto defence = unitList.begin(); defence != unitList.end(); ++defence)
	{
		const float defenceCombatPower = ai->s_buildTree.GetCombatPower(*defence).GetCombatPowerVsTargetType(selectionCriteria.targetType);
		combatPowerStat.AddValue(defenceCombatPower);
	}

	combatPowerStat.Finalize();

	// start with selection
	UnitDefId selectedDefence;
	float bestRating(0.0f);

	for(auto defence = unitList.begin(); defence != unitList.end(); ++defence)
	{
		if( IsBuildingSelectable(*defence, water, mustBeConstructable) )
		{
			const UnitTypeProperties& unitData = ai->s_buildTree.GetUnitTypeProperties(*defence);

			const float myCombatPower = ai->s_buildTree.GetCombatPower(*defence).GetCombatPowerVsTargetType(selectionCriteria.targetType);

			float myRating =  selectionCriteria.cost        * costs.GetNormalizedDeviationFromMax( unitData.m_totalCost )
							+ selectionCriteria.buildtime   * buildtimes.GetNormalizedDeviationFromMax( unitData.m_buildtime )
							+ selectionCriteria.range       * ranges.GetNormalizedDeviationFromMin( unitData.m_primaryAbility )
							+ selectionCriteria.combatPower * combatPowerStat.GetNormalizedDeviationFromMin( myCombatPower )
							+ 0.05f * ((float)(rand()%(selectionCriteria.randomness+1)));

			if(myRating > bestRating)
			{
				bestRating = myRating;
				selectedDefence = *defence;
			}
		}
	}

	return selectedDefence;
}

int AAIBuildTable::GetAirBase(int side, float /*cost*/, bool water, bool canBuild)
{
	float best_ranking = 0, my_ranking;
	int best_airbase = 0;

	// @todo Reactivate when detection of air bases is resolved
	/*for(auto airbase = ai->s_buildTree.GetU.begin(); airbase != units_of_category[AIR_BASE][side-1].end(); ++airbase)
	{
		// check if water
		if(canBuild && units_dynamic[*airbase].constructorsAvailable <= 0)
			my_ranking = 0;
		else if(!water && GetUnitDef(*airbase).minWaterDepth <= 0)
		{
			my_ranking = 100.f / (units_dynamic[*airbase].active + 1);
		}
		else if(water && GetUnitDef(*airbase).minWaterDepth > 0)
		{
			//my_ranking =  100 / (cost * units_static[*airbase].cost);
			my_ranking = 100.f / (units_dynamic[*airbase].active + 1);
		}
		else
			my_ranking = 0;

		if(my_ranking > best_ranking)
		{
			best_ranking = my_ranking;
			best_airbase = *airbase;
		}
	}*/
	return best_airbase;
}

UnitDefId AAIBuildTable::SelectStaticArtillery(int side, float cost, float range, bool water) const
{
	const StatisticalData& costs      = ai->s_buildTree.GetUnitStatistics(side).GetUnitCostStatistics(EUnitCategory::STATIC_ARTILLERY);
	const StatisticalData& ranges     = ai->s_buildTree.GetUnitStatistics(side).GetUnitPrimaryAbilityStatistics(EUnitCategory::STATIC_ARTILLERY);

	float bestRating(0.0f);
	UnitDefId selectedArtillery;

	for(auto artillery = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_ARTILLERY, side).begin(); artillery != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_ARTILLERY, side).end(); ++artillery)
	{
		// check if water
		if( IsBuildingSelectable(*artillery, water, false) )
		{
			const float myRating =   cost  * costs.GetNormalizedDeviationFromMax(ai->s_buildTree.GetTotalCost(*artillery))
			                       + range * ranges.GetNormalizedDeviationFromMin(ai->s_buildTree.GetMaxRange(*artillery));

			if(myRating > bestRating)
			{
				bestRating        = myRating;
				selectedArtillery = *artillery;
			}
		}
	}

	return selectedArtillery;
}

UnitDefId AAIBuildTable::SelectRadar(int side, float cost, float range, bool water)
{
	UnitDefId radar = SelectRadar(side, cost, range, water, false);

	if(radar.IsValid() && (units_dynamic[radar.id].constructorsAvailable <= 0) )
	{
		if(units_dynamic[radar.id].constructorsRequested <= 0)
			RequestBuilderFor(radar);

		radar = SelectRadar(side, cost, range, water, true);
	}

	return radar;
}
	
UnitDefId AAIBuildTable::SelectRadar(int side, float cost, float range, bool water, bool mustBeConstructable) const
{
	UnitDefId selectedRadar;
	float bestRating(0.0f);

	const StatisticalData& costs  = ai->s_buildTree.GetUnitStatistics(side).GetSensorStatistics().m_radarCosts;
	const StatisticalData& ranges = ai->s_buildTree.GetUnitStatistics(side).GetSensorStatistics().m_radarRanges;

	for(auto sensor = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_SENSOR, side).begin(); sensor != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_SENSOR, side).end(); ++sensor)
	{
		//! @todo replace by checking unit type for radar when implemented.
		if( ai->s_buildTree.GetUnitType(sensor->id).IsRadar() )
		{
			if(IsBuildingSelectable(*sensor, water, mustBeConstructable))
			{
				const float myRating =   cost * costs.GetNormalizedDeviationFromMax(ai->s_buildTree.GetTotalCost(sensor->id))
				                       + range * ranges.GetNormalizedDeviationFromMin(ai->s_buildTree.GetMaxRange(sensor->id));

				if(myRating > bestRating)
				{
					selectedRadar = *sensor;
					bestRating    = myRating;
				}
			}
		}
	}

	return selectedRadar;
}

int AAIBuildTable::GetJammer(int side, float cost, float range, bool water, bool canBuild)
{
	int best_jammer = 0;
	float my_rating, best_rating = -10000;

	for(auto jammer = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_SUPPORT, side).begin(); jammer != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_SUPPORT, side).end(); ++jammer)
	{
		//! @todo Check unit type for jammer
		/*


		if(my_rating > best_rating)
		{
			if(GetUnitDef(*i).metalCost < cfg->MAX_METAL_COST)
			{
				best_jammer = *i;
				best_rating = my_rating;
			}
		}*/
	}

	return best_jammer;
}

UnitDefId AAIBuildTable::selectScout(int side, float sightRange, float cost, uint32_t movementType, int randomness, bool cloakable, bool factoryAvailable)
{
	float highestRating(0.0f);
	UnitDefId selectedScout;

	const StatisticalData& costs       = ai->s_buildTree.GetUnitStatistics(side).GetUnitCostStatistics(EUnitCategory::SCOUT);
	const StatisticalData& sightRanges = ai->s_buildTree.GetUnitStatistics(side).GetUnitPrimaryAbilityStatistics(EUnitCategory::SCOUT);

	for(auto scout = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::SCOUT, side).begin(); scout != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::SCOUT, side).end(); ++scout)
	{
		const bool movementTypeAllowed     = ai->s_buildTree.GetMovementType(scout->id).IsIncludedIn(movementType);
		const bool factoryPrerequisitesMet = !factoryAvailable || (units_dynamic[scout->id].constructorsAvailable > 0);

		if( movementTypeAllowed && factoryPrerequisitesMet )
		{
			float myRating =     sightRange * sightRanges.GetNormalizedDeviationFromMin(ai->s_buildTree.GetMaxRange(*scout))
							   +       cost * costs.GetNormalizedDeviationFromMax( ai->s_buildTree.GetTotalCost(*scout) );

			if(cloakable && GetUnitDef(scout->id).canCloak)
				myRating += 2.0f;

			myRating += (0.1f * ((float)(rand()%randomness)));

			if(myRating > highestRating)
			{
				highestRating = myRating;
				selectedScout = *scout;
			}
		}
	}
	
	return selectedScout;
}

void AAIBuildTable::CalculateCombatPowerForUnits(const std::list<UnitDefId>& unitList, const AAICombatPower& combatPowerWeights, std::vector<float>& combatPowerValues, StatisticalData& combatPowerStat, StatisticalData& combatEfficiencyStat)
{
	int i = 0;
	for(const auto& unitDefId : unitList)
	{
		const UnitTypeProperties& unitData = ai->s_buildTree.GetUnitTypeProperties(unitDefId);

		const float combatPower = combatPowerWeights.CalculateWeightedSum(ai->s_buildTree.GetCombatPower(unitDefId)); 

		const float combatEff   = combatPower / unitData.m_totalCost;

		combatPowerStat.AddValue(combatPower);
		combatEfficiencyStat.AddValue(combatEff);
		combatPowerValues[i] = combatPower;

		++i;
	}

	combatPowerStat.Finalize();
	combatEfficiencyStat.Finalize();
}

UnitDefId AAIBuildTable::SelectCombatUnit(int side, const AAITargetType& targetType, const AAICombatPower& combatPowerCriteria, const UnitSelectionCriteria& unitCriteria, const std::vector<float>& factoryUtilization, int randomness)
{
	//-----------------------------------------------------------------------------------------------------------------
	// get data needed for selection
	//-----------------------------------------------------------------------------------------------------------------
	const auto& unitList = ai->s_buildTree.GetCombatUnitsOfTargetType(targetType, side);

	const StatisticalData& costStatistics  = ai->s_buildTree.GetUnitStatistics(side).GetCombatCostStatistics(targetType);
	const StatisticalData& rangeStatistics = ai->s_buildTree.GetUnitStatistics(side).GetCombatRangeStatistics(targetType);
	const StatisticalData& speedStatistics = ai->s_buildTree.GetUnitStatistics(side).GetCombatSpeedStatistics(targetType);

	StatisticalData combatPowerStat;		               // absolute combat power
	StatisticalData combatEfficiencyStat;	               // combat power related to unit cost
	std::vector<float> combatPowerValues(unitList.size()); // values for individual units (in order of appearance in unitList)

	CalculateCombatPowerForUnits(unitList, combatPowerCriteria, combatPowerValues, combatPowerStat, combatEfficiencyStat);

	//-----------------------------------------------------------------------------------------------------------------
	// begin with selection
	//-----------------------------------------------------------------------------------------------------------------
	UnitDefId selectedUnitType;
	float highestRating(0.0f);

	int i(0);
	for(const auto& unitDefId : unitList)
	{
		float minFactoryUtilization(0.0f);

		for(const auto& factory : ai->s_buildTree.GetConstructedByList(unitDefId))
		{
			const float utilization = factoryUtilization[ai->s_buildTree.GetUnitTypeProperties(factory).m_factoryId.id];

			if(utilization > minFactoryUtilization)
				minFactoryUtilization = utilization;
		}

		//if(    (canBuild == false)
		//	|| ((canBuild == true) && (units_dynamic[unitDefId.id].constructorsAvailable > 0)) )
		if(minFactoryUtilization > 0.0f)
		{
			const UnitTypeProperties& unitData = ai->s_buildTree.GetUnitTypeProperties(unitDefId);

			const float combatEff = combatPowerValues[i] / unitData.m_totalCost;

			const float myRating =  unitCriteria.cost  * costStatistics.GetNormalizedSquaredDeviationFromMax( unitData.m_totalCost )
							+ unitCriteria.range * rangeStatistics.GetNormalizedSquaredDeviationFromMin( unitData.m_primaryAbility )
							+ unitCriteria.speed * speedStatistics.GetNormalizedSquaredDeviationFromMin( unitData.m_maxSpeed )
							+ unitCriteria.power * combatPowerStat.GetNormalizedSquaredDeviationFromMin( combatPowerValues[i] )
							+ unitCriteria.efficiency * combatEfficiencyStat.GetNormalizedSquaredDeviationFromMin( combatEff )
							+ unitCriteria.factoryUtilization * minFactoryUtilization
							+ 0.05f * ((float)(rand()%randomness));

			if(myRating > highestRating)
			{
				highestRating       = myRating;
				selectedUnitType.id = unitDefId.id;
			}
		}

		++i;
	}
	
	return selectedUnitType;
}

std::string AAIBuildTable::GetBuildCacheFileName() const
{
	return cfg->GetFileName(ai->GetAICallback(), cfg->getUniqueName(ai->GetAICallback(), true, true, false, false), MOD_LEARN_PATH, "_buildcache.txt", true);
}

bool AAIBuildTable::LoadModLearnData()
{
	// load data
	const std::string filename = GetBuildCacheFileName();
	// load units if file exists
	FILE *inputFile = fopen(filename.c_str(), "r");

	if(inputFile)
	{
		char buffer[1024];
		// check if correct version
		fscanf(inputFile, "%s", buffer);

		if(strcmp(buffer, MOD_LEARN_VERSION))
		{
			ai->LogConsole("Buildtable version out of date - creating new one");
			return false;
		}

		// load attacked_by table
		for(AAIMapType mapType(AAIMapType::first); mapType.End() == false; mapType.Next())
		{
			for(GamePhase gamePhase(0); gamePhase.End() == false; gamePhase.Next())
			{
				for(const auto& targetType : AAITargetType::m_mobileTargetTypes)
				{
					float atackedByRate;
					fscanf(inputFile, "%f ", &atackedByRate);
					s_attackedByRates.SetAttackedByRate(mapType, gamePhase, targetType, atackedByRate);
				}
			}
		}

		const bool combatPowerLoaded = ai->s_buildTree.LoadCombatPowerOfUnits(inputFile);
		fclose(inputFile);
		return combatPowerLoaded;
	}
	
	return false;
}

void AAIBuildTable::SaveModLearnData(const GamePhase& gamePhase, const AttackedByRatesPerGamePhase& attackedByRates, const AAIMapType& mapType) const
{
	const std::string filename = GetBuildCacheFileName();
	FILE *saveFile = fopen(filename.c_str(), "w+");

	// file version
	fprintf(saveFile, "%s \n", MOD_LEARN_VERSION);

	// update attacked_by values
	AttackedByRatesPerGamePhase& updateRates = s_attackedByRates.GetAttackedByRates(mapType);
	updateRates = attackedByRates;
	updateRates.DecreaseByFactor(gamePhase, 0.7f);

	// save attacked_by table
	for(AAIMapType mapTypeIterator(AAIMapType::first); mapTypeIterator.End() == false; mapTypeIterator.Next())
	{
		for(GamePhase gamePhaseIterator(0); gamePhaseIterator.End() == false; gamePhaseIterator.Next())
		{
			for(const auto& targetType : AAITargetType::m_mobileTargetTypes)
			{
				fprintf(saveFile, "%f ", s_attackedByRates.GetAttackedByRate(mapTypeIterator, gamePhaseIterator, targetType));
			}
			fprintf(saveFile, "\n");
		}
	}

	ai->s_buildTree.SaveCombatPowerOfUnits(saveFile);

	fclose(saveFile);
}

void AAIBuildTable::BuildFactoryFor(int unit_def_id)
{
	int constructor = 0;
	float best_rating = -100000.0f, my_rating;

	float cost = 1.0f;
	float buildspeed = 1.0f;

	// determine max values
	float max_buildtime = 0;
	float max_buildspeed = 0;
	float max_cost = 0;

	for(std::list<UnitDefId>::const_iterator factory = ai->s_buildTree.GetConstructedByList(UnitDefId(unit_def_id)).begin();  factory != ai->s_buildTree.GetConstructedByList(UnitDefId(unit_def_id)).end(); ++factory)
	{
		if(ai->s_buildTree.GetTotalCost(*factory) > max_cost)
			max_cost = ai->s_buildTree.GetTotalCost(*factory);

		if(GetUnitDef((*factory).id).buildTime > max_buildtime)
			max_buildtime = GetUnitDef((*factory).id).buildTime;

		if(GetUnitDef((*factory).id).buildSpeed > max_buildspeed)
			max_buildspeed = GetUnitDef((*factory).id).buildSpeed;
	}

	// look for best builder to do the job
	for(std::list<UnitDefId>::const_iterator factory = ai->s_buildTree.GetConstructedByList(UnitDefId(unit_def_id)).begin();  factory != ai->s_buildTree.GetConstructedByList(UnitDefId(unit_def_id)).end(); ++factory)
	{
		if(units_dynamic[(*factory).id].active + units_dynamic[(*factory).id].requested + units_dynamic[(*factory).id].under_construction < cfg->MAX_FACTORIES_PER_TYPE)
		{
			my_rating = buildspeed * (GetUnitDef((*factory).id).buildSpeed / max_buildspeed)
				- (GetUnitDef((*factory).id).buildTime / max_buildtime)
				- cost * (ai->s_buildTree.GetTotalCost(*factory) / max_cost);

			// prefer builders that can be built atm
			if(units_dynamic[(*factory).id].constructorsAvailable > 0)
				my_rating += 2.0f;

			// prevent AAI from requesting factories that cannot be built within the current base
			if(ai->s_buildTree.GetMovementType(*factory).IsStaticLand() == true)
			{
				if(ai->Getbrain()->GetBaseFlatLandRatio() > 0.1f)
					my_rating *= ai->Getbrain()->GetBaseFlatLandRatio();
				else
					my_rating = -100000.0f;
			}
			else if(ai->s_buildTree.GetMovementType(*factory).IsStaticSea() == true)
			{
				if(ai->Getbrain()->GetBaseWaterRatio() > 0.1f)
					my_rating *= ai->Getbrain()->GetBaseWaterRatio();
				else
					my_rating = -100000.0f;
			}

			if(my_rating > best_rating)
			{
				best_rating = my_rating;
				constructor = (*factory).id;
			}
		}
	}

	if(constructor && units_dynamic[constructor].requested + units_dynamic[constructor].under_construction <= 0)
	{
		ConstructorRequested(UnitDefId(constructor));

		m_factoryBuildqueue.push_back(UnitDefId(constructor));

		// factory requested
		if( ai->s_buildTree.GetUnitCategory(UnitDefId(constructor)).IsStaticConstructor() )
		{
			units_dynamic[constructor].requested += 1;

			if(units_dynamic[constructor].constructorsAvailable + units_dynamic[constructor].constructorsRequested <= 0)
			{
				ai->Log("BuildFactoryFor(%s) is requesting builder for %s\n", ai->s_buildTree.GetUnitTypeProperties(UnitDefId(unit_def_id)).m_name.c_str(), ai->s_buildTree.GetUnitTypeProperties(UnitDefId(constructor)).m_name.c_str());
				RequestBuilderFor(UnitDefId(constructor));
			}

			// debug
			ai->Log("BuildFactoryFor(%s) requested %s\n", ai->s_buildTree.GetUnitTypeProperties(UnitDefId(unit_def_id)).m_name.c_str(), ai->s_buildTree.GetUnitTypeProperties(UnitDefId(constructor)).m_name.c_str());
		}
		// mobile constructor requested
		else
		{
			// only mark as urgent (unit gets added to front of buildqueue) if no constructor of that type already exists
			const BuildQueuePosition queuePosition = (units_dynamic[constructor].active > 0) ? BuildQueuePosition::SECOND : BuildQueuePosition::FRONT;

			if(ai->Getexecute()->AddUnitToBuildqueue(UnitDefId(constructor), 1, queuePosition, true))
			{
				// increase counter if mobile factory is a builder as well
				if(ai->s_buildTree.GetUnitType(UnitDefId(constructor)).IsBuilder())
					ai->Getut()->futureBuilders += 1;

				if(units_dynamic[constructor].constructorsAvailable + units_dynamic[constructor].constructorsRequested <= 0)
				{
					ai->Log("BuildFactoryFor(%s) is requesting factory for %s\n", ai->s_buildTree.GetUnitTypeProperties(UnitDefId(unit_def_id)).m_name.c_str(), ai->s_buildTree.GetUnitTypeProperties(UnitDefId(constructor)).m_name.c_str());
					BuildFactoryFor(constructor);
				}

				// debug
				ai->Log("BuildFactoryFor(%s) requested %s\n", ai->s_buildTree.GetUnitTypeProperties(UnitDefId(unit_def_id)).m_name.c_str(), ai->s_buildTree.GetUnitTypeProperties(UnitDefId(constructor)).m_name.c_str());
			}
			else
			{
				//something went wrong -> decrease values
				units_dynamic[constructor].requested -= 1;

				// decrease "contructor requested" counters of buildoptions 
				UnfinishedConstructorKilled(UnitDefId(constructor));
			}
		}
	}
}

void AAIBuildTable::RequestBuilderFor(UnitDefId building)
{
	//-----------------------------------------------------------------------------------------------------------------
	// determine criteria for selection of construction unit
	//-----------------------------------------------------------------------------------------------------------------
	float cost = 1.0f;
	float buildtime = 0.5f;
	float buildpower = 1.0f;
	float constructableBuilderBonus = 2.0f;

	if(units_dynamic[building.id].constructorsAvailable == 0)
	{
		buildtime                 = 2.0f;
	}
	else if(units_dynamic[building.id].constructorsAvailable < 2)
	{
		buildtime                 = 1.0f;
	}

	//-----------------------------------------------------------------------------------------------------------------
	// determine statistical data needed for selection of contruction unit for given building
	//-----------------------------------------------------------------------------------------------------------------
	StatisticalData costStatistics;
	StatisticalData buildtimeStatistics;
	StatisticalData buildpowerStatistics;

	for(auto builder = ai->s_buildTree.GetConstructedByList(building).begin();  builder != ai->s_buildTree.GetConstructedByList(building).end(); ++builder)
	{
		costStatistics.AddValue( ai->s_buildTree.GetTotalCost(*builder) );
		buildtimeStatistics.AddValue( ai->s_buildTree.GetBuildtime(*builder) );
		buildpowerStatistics.AddValue( ai->s_buildTree.GetBuildspeed(*builder) );
	}

	costStatistics.Finalize();
	buildtimeStatistics.Finalize();
	buildpowerStatistics.Finalize();

	//-----------------------------------------------------------------------------------------------------------------
	// select builder according to determined criteria
	//-----------------------------------------------------------------------------------------------------------------
	float highestRating(0.0f);
	UnitDefId selectedBuilder;

	for(auto builder = ai->s_buildTree.GetConstructedByList(building).begin();  builder != ai->s_buildTree.GetConstructedByList(building).end(); ++builder)
	{
		// prevent ai from ordering too many builders of the same type/commanders/builders that cant be built atm
		if(units_dynamic[(*builder).id].active + units_dynamic[(*builder).id].under_construction + units_dynamic[(*builder).id].requested < cfg->MAX_BUILDERS_PER_TYPE)
		{
			float myRating = cost       * costStatistics.GetNormalizedDeviationFromMax( ai->s_buildTree.GetTotalCost(*builder) )
			               + buildtime  * buildtimeStatistics.GetNormalizedDeviationFromMax( ai->s_buildTree.GetBuildtime(*builder) )
				           + buildpower * buildpowerStatistics.GetNormalizedDeviationFromMin( ai->s_buildTree.GetBuildspeed(*builder) );

			if(units_dynamic[(*builder).id].constructorsAvailable > 0)
				myRating += constructableBuilderBonus;

			if(myRating > highestRating)
			{
				highestRating   = myRating;
				selectedBuilder = *builder;
			}
		}
	}

	//-----------------------------------------------------------------------------------------------------------------
	// order construction if valid builder found and check if factory needs to ordered
	//-----------------------------------------------------------------------------------------------------------------
	if( selectedBuilder.IsValid() && (units_dynamic[selectedBuilder.id].under_construction + units_dynamic[selectedBuilder.id].requested <= 0) )
	{
		// build factory if necessary
		if(units_dynamic[selectedBuilder.id].constructorsAvailable + units_dynamic[selectedBuilder.id].constructorsRequested <= 0)
		{
			ai->Log("RequestBuilderFor(%s) is requesting factory for %s\n", ai->s_buildTree.GetUnitTypeProperties(building).m_name.c_str(), ai->s_buildTree.GetUnitTypeProperties(selectedBuilder).m_name.c_str());
			BuildFactoryFor(selectedBuilder.id);
		}

		// mark as urgent (unit gets added to front of buildqueue) if no/only one constructor of that type already exists
		const BuildQueuePosition queuePosition = (units_dynamic[selectedBuilder.id].active > 1) ? BuildQueuePosition::SECOND : BuildQueuePosition::FRONT;

		if(ai->Getexecute()->AddUnitToBuildqueue(selectedBuilder, 1, queuePosition, true))
		{
			ai->Getut()->futureBuilders += 1;
			ConstructorRequested(selectedBuilder);

			ai->Log("RequestBuilderFor(%s) requested %s\n", ai->s_buildTree.GetUnitTypeProperties(building).m_name.c_str(), ai->s_buildTree.GetUnitTypeProperties(selectedBuilder).m_name.c_str());
		}
	}
}


/*void AAIBuildTable::AddAssistant(uint32_t allowedMovementTypes, bool canBuild)
{
	int builder = 0;
	float best_rating = -10000, my_rating;

	int side = ai->GetSide();

	float cost = 1.0f;
	float buildspeed = 2.0f;
	float urgency = 1.0f;

	for(auto unit = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::MOBILE_CONSTRUCTOR, side).begin();  unit != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::MOBILE_CONSTRUCTOR, side).end(); ++unit)
	{
		if(ai->s_buildTree.GetMovementType(UnitDefId(*unit)).isIncludedIn(allowedMovementTypes) == true)
		{
			if( (!canBuild || units_dynamic[unit->id].constructorsAvailable > 0)
				&& units_dynamic[unit->id].active + units_dynamic[unit->id].under_construction + units_dynamic[unit->id].requested < cfg->MAX_BUILDERS_PER_TYPE)
			{
				if( GetUnitDef(unit->id).buildSpeed >= (float)cfg->MIN_ASSISTANCE_BUILDTIME && GetUnitDef(unit->id).canAssist)
				{
					my_rating = cost * (ai->s_buildTree.GetTotalCost(UnitDefId(unit->id)) / max_cost[MOBILE_CONSTRUCTOR][ai->GetSide()-1])
								+ buildspeed * (GetUnitDef(unit->id).buildSpeed / max_value[MOBILE_CONSTRUCTOR][ai->GetSide()-1])
								- urgency * (GetUnitDef(unit->id).buildTime / max_buildtime[MOBILE_CONSTRUCTOR][ai->GetSide()-1]);

					if(my_rating > best_rating)
					{
						best_rating = my_rating;
						builder = unit->id;
					}
				}
			}
		}
	}

	if(builder && units_dynamic[builder].under_construction + units_dynamic[builder].requested < 1)
	{
		// build factory if necessary
		if(units_dynamic[builder].constructorsAvailable <= 0)
			BuildFactoryFor(builder);

		if(ai->Getexecute()->AddUnitToBuildqueue(UnitDefId(builder), 1, true))
		{
			units_dynamic[builder].requested += 1;
			ai->Getut()->futureBuilders += 1;
			ai->Getut()->UnitRequested(AAIUnitCategory(EUnitCategory::MOBILE_CONSTRUCTOR));

			// increase number of requested builders of all buildoptions
			ConstructorRequested(UnitDefId(builder));

			//ai->Log("AddAssister() requested: %s %i \n", GetUnitDef(builder).humanName.c_str(), units_dynamic[builder].requested);
		}
	}
}*/


bool AAIBuildTable::IsArty(int id)
{
	if(!GetUnitDef(id).weapons.empty())
	{
		float max_range = 0;
//		const WeaponDef *longest = 0;

		for(vector<UnitDef::UnitDefWeapon>::const_iterator weapon = GetUnitDef(id).weapons.begin(); weapon != GetUnitDef(id).weapons.end(); ++weapon)
		{
			if(weapon->def->range > max_range)
			{
				max_range = weapon->def->range;
//				longest = weapon->def;
			}
		}

		// veh, kbot, hover or ship
		if(GetUnitDef(id).movedata)
		{
			if(GetUnitDef(id).movedata->moveFamily == MoveData::Tank || GetUnitDef(id).movedata->moveFamily == MoveData::KBot)
			{
				if(max_range > cfg->GROUND_ARTY_RANGE)
					return true;
			}
			else if(GetUnitDef(id).movedata->moveFamily == MoveData::Ship)
			{
				if(max_range > cfg->SEA_ARTY_RANGE)
					return true;
			}
			else if(GetUnitDef(id).movedata->moveFamily == MoveData::Hover)
			{
				if(max_range > cfg->HOVER_ARTY_RANGE)
					return true;
			}
		}
		else // aircraft
		{
			if(cfg->AIR_ONLY_MOD)
			{
				if(max_range > cfg->GROUND_ARTY_RANGE)
					return true;
			}
		}

		if(GetUnitDef(id).highTrajectoryType == 1)
			return true;
	}

	return false;
}

bool AAIBuildTable::IsAttacker(int id)
{
	for(list<int>::iterator i = cfg->ATTACKERS.begin(); i != cfg->ATTACKERS.end(); ++i)
	{
		if(*i == id)
			return true;
	}

	return false;
}


bool AAIBuildTable::IsTransporter(int id)
{
	for(list<int>::iterator i = cfg->TRANSPORTERS.begin(); i != cfg->TRANSPORTERS.end(); ++i)
	{
		if(*i == id)
			return true;
	}

	return false;
}

bool AAIBuildTable::AllowedToBuild(int id)
{
	for(list<int>::iterator i = cfg->DONT_BUILD.begin(); i != cfg->DONT_BUILD.end(); ++i)
	{
		if(*i == id)
			return false;
	}

	return true;
}

bool AAIBuildTable::IsMetalMaker(int id)
{
	for(list<int>::iterator i = cfg->METAL_MAKERS.begin(); i != cfg->METAL_MAKERS.end(); ++i)
	{
		if(*i == id)
			return true;
	}

	return false;
}

bool AAIBuildTable::IsMissileLauncher(int def_id)
{
	for(vector<UnitDef::UnitDefWeapon>::const_iterator weapon = GetUnitDef(def_id).weapons.begin(); weapon != GetUnitDef(def_id).weapons.end(); ++weapon)
	{
		if(weapon->def->stockpile)
			return true;
	}

	return false;
}

bool AAIBuildTable::IsDeflectionShieldEmitter(int def_id)
{
	for(vector<UnitDef::UnitDefWeapon>::const_iterator weapon = GetUnitDef(def_id).weapons.begin(); weapon != GetUnitDef(def_id).weapons.end(); ++weapon)
	{
		if(weapon->def->isShield)
			return true;
	}

	return false;
}

AAIUnitCategory AAIBuildTable::GetUnitCategoryOfCombatUnitIndex(int index) const
{
	//! @todo Use array instead of switch (is only called during initialization, thus not performance critical)
	switch(index)
	{
		case 0:
			return AAIUnitCategory(EUnitCategory::GROUND_COMBAT);
		case 1:
			return AAIUnitCategory(EUnitCategory::AIR_COMBAT);
		case 2:
			return AAIUnitCategory(EUnitCategory::HOVER_COMBAT);
		case 3:
			return AAIUnitCategory(EUnitCategory::SEA_COMBAT);
		case 4:
			return AAIUnitCategory(EUnitCategory::SUBMARINE_COMBAT);
		case 5:
			return AAIUnitCategory(EUnitCategory::STATIC_DEFENCE);
		default:
			return AAIUnitCategory(EUnitCategory::UNKNOWN);
	}
}
