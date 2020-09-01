// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#ifndef AAI_MAP_H
#define AAI_MAP_H

#include "aidef.h"
#include "AAITypes.h"
#include "AAIUnitTypes.h"
#include "AAISector.h"
#include "System/float3.h"

#include <vector>
#include <list>
#include <string>
using namespace std;

class AAI;

namespace springLegacyAI {
	struct UnitDef;
}
using namespace springLegacyAI;

//! A continent is made up of  tiles of the same type (land or water) that are connected with each other
struct AAIContinent
{
	//! Continent id
	int id;

	//! Size of continent (oin number of map tiles)
	int size;

	//! Flag if its a water continent
	bool water;
};

//! Movement types that are used to describe the movement type of every unit
enum class EBuildMapTileType : uint8_t
{
	NOT_SET       = 0x00u, //!< Unknown/not set
	LAND          = 0x01u, //!< land tile
	WATER         = 0x02u, //!< water tile
	FLAT          = 0x04u, //!< flat terrain (i.e. suitable for contruction of buildings or destination to send units to))
	CLIFF         = 0x08u, //!< cliffy terrain (i.e. not suitable for contruction of building or destination to send units to)
	FREE          = 0x10u, //!< free (i.e. buildings cand be constructed here)
	OCCUPIED      = 0x20u, //!< occupied by buidling
	BLOCKED_SPACE = 0x40u, //!< tiles where no buildings shall be constructed  (e.g. exits of factory)
};

//! Contains convenience functions for tiles of th buildmap
class BuildMapTileType
{
friend AAIMap;

public:
	BuildMapTileType(EBuildMapTileType tileType) { m_tileType = static_cast<uint8_t>(tileType); }

	BuildMapTileType() : BuildMapTileType(EBuildMapTileType::NOT_SET) {}

	BuildMapTileType(EBuildMapTileType tileType1, EBuildMapTileType tileType2) { m_tileType = static_cast<uint8_t>(tileType1) | static_cast<uint8_t>(tileType2); }

	void SetTileType(EBuildMapTileType tileType) { m_tileType |= static_cast<uint8_t>(tileType); }

	bool IsTileTypeSet(BuildMapTileType tileType) const { return static_cast<bool>(m_tileType & tileType.m_tileType); }

	bool IsTileTypeNotSet(BuildMapTileType tileType) const { return !static_cast<bool>(m_tileType & tileType.m_tileType); }

	void BlockTile()
	{
		m_tileType &= ~static_cast<uint8_t>(EBuildMapTileType::FREE);
		m_tileType |= static_cast<uint8_t>(EBuildMapTileType::BLOCKED_SPACE); 
	}

	void OccupyTile()
	{
		m_tileType &= ~static_cast<uint8_t>(EBuildMapTileType::FREE);
		m_tileType |= static_cast<uint8_t>(EBuildMapTileType::OCCUPIED); 
	}

	void FreeTile()
	{ 
		m_tileType &= ~(static_cast<uint8_t>(EBuildMapTileType::OCCUPIED) + static_cast<uint8_t>(EBuildMapTileType::BLOCKED_SPACE)); 
		m_tileType |= static_cast<uint8_t>(EBuildMapTileType::FREE); 
	}

//private:
	uint8_t m_tileType;
};

class AAIMap
{
public:
	AAIMap(AAI *ai);
	~AAIMap(void);

	void Init();

	//! @brief Returns the map type
	const AAIMapType& GetMapType() const { return s_mapType; }

	//! @brief Returns max distance (in sectors) a sector can have to base
	int GetMaxSectorDistanceToBase() const { return (xSectors + ySectors - 2); }

	//! @brief Converts given position to final building position for the given unit type
	void Pos2FinalBuildPos(float3 *pos, const UnitDef *def) const;

	//! @brief Returns whether x/y specify a valid sector
	bool IsValidSector(int x, int y) const { return( (x >= 0) && (y >= 0) && (x < xSectors) && (y < ySectors) ); }

	//! @brief Returns the id of continent the cell belongs to
	int GetContinentID(int x, int y) const;

	//! @brief Returns the id of continent the given position belongs to
	int GetContinentID(const float3& pos) const;

	//! @brief Returns whether continent to which given sector mainly belongs is sea 
	bool IsSectorOnWaterContinent(const AAISector* sector) const { return continents[sector->continent].water; }

	//! @brief Returns whether the position is located on a small continent (meant to detect "ponds" or "small islands")
	bool LocatedOnSmallContinent(const float3& pos) { return (continents[GetContinentID(pos)].size < (avg_land_continent_size + avg_water_continent_size)/4); }

	//! @brief Returns continent id with respect to the unit's movement type (e.g. ground (=non amphibious) unit being in shallow water will return id of nearest land continent)
	int getSmartContinentID(float3 *pos, const AAIMovementType& moveType) const;

	//! @brief Returns a bitmask storing which movement types are suitable for the map type
	uint32_t GetSuitableMovementTypesForMap() const { return GetSuitableMovementTypes(s_mapType); }

	//! @brief Returns whether the given sector is already occupied by another AAI player of the same team
	bool IsAlreadyOccupiedByOtherAAI(const AAISector* sector) const { return (team_sector_map[sector->x][sector->y] != -1) && (team_sector_map[sector->x][sector->y] != m_myTeamId); }

	//! @brief Returns the sector in which the given position lies (nullptr if out of sector map -> e.g. aircraft flying outside of the map) 
	AAISector* GetSectorOfPos(const float3& pos);

	float GetEdgeDistance(float3 *pos);

	//! @brief Returns the maximum number of units lost in any sector of the map
	float GetMaximumNumberOfLostUnits() const;

	// returns buildsites for normal and defence buildings
	float3 GetCenterBuildsite(const UnitDef *def, int xStart, int xEnd, int yStart, int yEnd, bool water = false);
	float3 GetRandomBuildsite(const UnitDef *def, int xStart, int xEnd, int yStart, int yEnd, int tries, bool water = false);
	float3 GetBuildSiteInRect(const UnitDef *def, int xStart, int xEnd, int yStart, int yEnd, bool water = false) const;

	// prefer buildsites that are on plateus and not too close to the edge of the map
	float3 GetRadarArtyBuildsite(const UnitDef *def, int xStart, int xEnd, int yStart, int yEnd, float range, bool water);

	// return rating of a the best buidliste fpr a def. building vs category within specified rect (and stores pos in pointer)
	float GetDefenceBuildsite(float3 *buildPos, const UnitDef *def, int xStart, int xEnd, int yStart, int yEnd, const AAITargetType& targetType, float terrainModifier, bool water) const;

	//! @brief Updates the buildmap: (un)block cells + insert/remove spaces (factory exits get some extra space)
	void UpdateBuildMap(const float3& buildPos, const UnitDef *def, bool block);

	// returns number of cells with big slope
	int GetCliffyCells(int xPos, int yPos, int xSize, int ySize) const;

	// updates spotted ennemy/ally buildings/units on the map
	void UpdateEnemyUnitsInLOS();

	void UpdateFriendlyUnitsInLos();

	// updates enemy buildings/enemy stat. combat strength in sectors based on scouted_buildings_map
	void UpdateEnemyScoutingData();

	void UpdateSectors();

	//! @brief Adds a defence buidling to the defence map
	void AddStaticDefence(const float3& position, UnitDefId defence);

	//! @brief Removes a defence buidling from the defence map
	void RemoveDefence(const float3& pos, UnitDefId defence);

	// sectors
	vector<vector<AAISector> > sector;

	// used for scouting, used to get all friendly/enemy units in los
	vector<int> units_in_los;
	static int xMapSize, yMapSize;				// x and y size of the map (map coordinates)
	static int xSectors, ySectors;				// number of sectors
	static int xSectorSize, ySectorSize;		// size of sectors (in unit pos coordinates)
	static int xSectorSizeMap, ySectorSizeMap;	// size of sectors (in map coodrinates = 1/8 xSize)
	static float land_ratio;
	static int water_metal_spots;
	static int land_metal_spots;
	static bool metalMap;
	static float water_ratio;

	static vector< vector<int> > team_sector_map;	// stores the number of ai player which has taken that sector (-1 if none)
											// this helps preventing aai from expanding into sectors of other aai players

	//! The buildmap stores the type/occupation status of every cell;
	static std::vector<BuildMapTileType> m_buildmap;

	static vector<AAIContinent> continents;
	static int avg_water_continent_size;

	static constexpr int ignoreContinentID = -1;

private:

	// defence maps
	vector<float> defence_map;	//  ground/sea defence map has 1/2 of resolution of blockmap/buildmap
	vector<float> air_defence_map; // air defence map has 1/2 of resolution of blockmap/buildmap
	vector<float> submarine_defence_map; // submarine defence map has 1/2 of resolution of blockmap/buildmap
	
	//! @brief Converts the given position (in map coordinates) to a position in buildmap coordinates
	void Pos2BuildMapPos(float3* position, const UnitDef* def) const;

	//! Stores the defId of the building or combat unit placed on that cell (0 if none), same resolution as los map
	std::vector<unsigned short> m_scoutedEnemyUnitsMap;

	//! Stores the frame of the last update of a cell (same resolution as los map)
	std::vector<int>            m_lastLOSUpdateInFrameMap;

	// krogothe's metal spot finder
	void DetectMetalSpots();

	// determines type of map (land, land/water or water map)
	void DetectMapType();

	//! @brief Returns which movement types are suitable for the given map type
	uint32_t GetSuitableMovementTypes(const AAIMapType& mapType) const;

	void CalculateWaterRatio();

	// calculates which parts of the are connected
	void CalculateContinentMaps();

	// determines water, high slopes, defence map
	void AnalyseMap();

	// calculates learning effect
	void Learn();

	//! @brief Read the learning data for this map (or initialize with defualt data if none are available)
	void readMapLearnFile();

	// reads continent cache file (and creates new one if necessary)
	void ReadContinentFile();

	// reads map cache file (and creates new one if necessary)
	// loads mex spots, cliffs etc. from file or creates new one
	void ReadMapCacheFile();

	//! @brief returns true if buildmap allows construction
	bool CanBuildAt(int xPos, int yPos, int xSize, int ySize, bool water = false) const;

	//! @brief Returns descriptor for map type (used to save map type)
	const char* GetMapTypeString(const AAIMapType& mapType) const;

	//! @brief Blocks/unblocks map tiles (to prevent AAI from packing buildings too close to each other)
	//!        Automatically clamps given values to map size (avoids running over any map edges)
	void BlockTiles(int xPos, int yPos, int width, int height, bool block);

	//! @brief Prevents AAI from building too many buildings in a row by adding blocking spaces if necessary
	void CheckRows(int xPos, int yPos, int xSize, int ySize, bool add);

	//! @brief Returns the size which shall be blocked for this building (building size + exit for factories)
	void GetSize(const UnitDef *def, int *xSize, int *ySize) const;

	//! @brief Returns distance to closest edge of the map (in build_map coordinates)
	int GetEdgeDistance(int xPos, int yPos) const;

	//! @brief Occupies/frees the given cells of the buildmap
	void ChangeBuildMapOccupation(int xPos, int yPos, int xSize, int ySize, bool occupy);

public:
	void BuildMapPos2Pos(float3 *pos, const UnitDef* def) const;

private:
	std::string LocateMapLearnFile() const;
	std::string LocateMapCacheFile() const;

	AAI *ai;

	//! Id of the team (not ally team) of the AAI instance
	int m_myTeamId;

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// static (shared with other ai players)
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	//! The map type
	static AAIMapType s_mapType;

	static int aai_instances;	// how many AAI instances have been initialized

	static int xSize, ySize;					// x and y size of the map (unit coordinates)
	static int losMapRes;				// resolution of the LOS map
	static int xLOSMapSize, yLOSMapSize;		// x and y size of the LOS map
	static int xDefMapSize, yDefMapSize;		// x and y size of the defence maps (1/4 resolution of map)
	static int xContMapSize, yContMapSize;		// x and y size of the continent maps (1/4 resolution of map)
	static std::list<AAIMetalSpot> metal_spots;
	static float flat_land_ratio;
	static vector<int> blockmap;		// number of buildings which ordered a cell to blocked
	static vector<float> plateau_map;	// positive values indicate plateaus, same resolution as continent map 1/4 of resolution of blockmap/buildmap
	static vector<int> continent_map;	// id of continent a cell belongs to

	static vector<int> ship_movement_map;	// movement maps for different categories, 1/4 of resolution of blockmap/buildmap
	static vector<int> kbot_movement_map;
	static vector<int> vehicle_movement_map;
	static vector<int> hover_movement_map;
	static int land_continents;
	static int water_continents;


	static int avg_land_continent_size;
	static int max_land_continent_size;
	static int max_water_continent_size;
	static int min_land_continent_size;
	static int min_water_continent_size;
};

#endif

