/*
    Copyright (C) 2011  Tiziano <tizbac2@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef TILESTORAGE_H
#define TILESTORAGE_H
#include <list>
#include <map>
#include <stdint.h>
#include <stdio.h>
#include <vector>

class InvalidTileIndexException
{
};
class InvalidTileDataPointerException
{
};
enum CompressLevels {
	COMPRESS_SHITTY = 1,
	COMPRESS_REASONABLE = 2,
	COMPRESS_INSANE = 3,
	COMPRESS_REASONABLE_BESTQUALITY = 4


};
class TileStorage
{

public:
	TileStorage();
	virtual ~TileStorage();
	uint64_t AddTile(uint8_t* data); // 32x32 RGBA
	void WriteToFile(FILE* f, std::vector<uint64_t>& tile_order);
	uint64_t AddTileOrGetSimiliar(uint8_t* data, float th, int compresslevel);
	uint32_t GetTileCount();
	void SetDictSize(uint32_t s);
	void Reset();

private:
	void CompressAll();
	void CompressTile(uint64_t uid);
	std::map<uint64_t, uint8_t*> m_tiles;
	std::map<uint64_t, uint8_t*> m_tiles_compressed;
	std::list<uint64_t> m_lasttiles;
	unsigned int m_dictcount;
};

#endif // TILESTORAGE_H
