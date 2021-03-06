/**
 * Copyright (c) 2017 Melown Technologies SE
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * *  Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef vtslibs_tilestorage_tileop_hpp_included_
#define vtslibs_tilestorage_tileop_hpp_included_

#include <boost/optional.hpp>

#include "./basetypes.hpp"
#include "./types.hpp"
#include "../storage/filetypes.hpp"
#include "./properties.hpp"

namespace vtslibs { namespace tilestorage {

using storage::TileFile;

bool operator==(const TileId &lhs, const TileId &rhs);

bool operator!=(const TileId &lhs, const TileId &rhs);

long tileSize(long baseTileSize, Lod lod);

long tileSize(const Properties &properties, Lod lod);

Extents tileExtents(long baseTileSize, const TileId &tile);

Extents tileExtents(const Properties &properties, const TileId &tile);

TileId fromAlignment(const Properties &properties, const TileId &tileId);

TileId parent(const Properties &properties, const TileId &tileId);

TileId parent(const Alignment &alignment, long baseTileSize
              , const TileId &tileId);

TileIdChildren children(long baseTileSize, const TileId &tileId);

IndexChildren children(const Index &index);

bool isMetatile(const LodLevels &levels, const TileId &tile);

Lod deltaDown(const LodLevels &levels, Lod lod);

/** Check whether super tile is above (or exactly the same tile) as tile.
 */
bool above(long baseTileSize, const TileId &tile, const TileId &super);

int child(const Index &index);

bool in(const LodRange &range, const TileId &tileId);

/** Calculates area of tile's mesh (m^2) and atlas (pixel^2).
 */
std::pair<double, double> area(const Tile &tile);

/** Calculates tileIndex of given tile in tiles from alignment.
 */
Index tileIndex(const Alignment &alignment, long baseTileSize
                , const TileId &tileId);

std::string asFilename(const TileId &tileId, TileFile type);

bool fromFilename(TileId &tileId, TileFile &type
                  , const std::string &str
                  , std::string::size_type offset = 0);

void misaligned(const Alignment &alignment, long baseTileSize
                , const TileId &tileId);

TileId findMetatile(const Properties &properties, TileId tileId
                    , bool useFoat = true);

// inline stuff

inline long tileSize(long baseTileSize, Lod lod)
{
    return (baseTileSize >> lod);
}

inline long tileSize(const Properties &properties, Lod lod)
{
    return (properties.baseTileSize >> lod);
}

inline Extents tileExtents(const Properties &properties, const TileId &tile)
{
    auto ts(tileSize(properties, tile.lod));
    return { tile.easting, tile.northing
            , tile.easting + ts, tile.northing + ts };
}

inline Extents tileExtents(long baseTileSize, const TileId &tile)
{
    auto ts(tileSize(baseTileSize, tile.lod));
    return { tile.easting, tile.northing
            , tile.easting + ts, tile.northing + ts };
}

inline TileId fromAlignment(const Properties &properties, const TileId &tileId)
{
    return { tileId.lod, tileId.easting - properties.alignment(0)
            , tileId.northing - properties.alignment(1) };
}

inline Index tileIndex(const Alignment &alignment, long baseTileSize
                       , const TileId &tileId)
{
    auto ts(tileSize(baseTileSize, tileId.lod));
    auto e(std::ldiv(tileId.easting - alignment(0), ts));
    if (e.rem) { misaligned(alignment, baseTileSize, tileId); }
    auto n(std::ldiv(tileId.northing - alignment(1), ts));
    if (n.rem) { misaligned(alignment, baseTileSize, tileId); }
    return { tileId.lod, e.quot, n.quot };
}

inline TileId parent(const Properties &properties, const TileId &tileId)
{
    return parent(properties.alignment, properties.baseTileSize, tileId);
}

inline TileId parent(const Alignment &alignment, long baseTileSize
                     , const TileId &tileId)
{
    auto ts(tileSize(baseTileSize, tileId.lod));
    Point2l tiled((tileId.easting - alignment(0)) / ts
                  , (tileId.northing - alignment(1)) / ts);

    constexpr auto mask(~(static_cast<decltype(tileId.easting)>(1)));

    return {
        Lod(tileId.lod - 1)
        , alignment(0) + (tiled(0) & mask) * ts
        , alignment(1) + (tiled(1) & mask) * ts
    };
}

inline TileIdChildren children(long baseTileSize, const TileId &tileId)
{
    Lod lod(tileId.lod + 1);
    auto ts(tileSize(baseTileSize, lod));

    return {{
        { lod, tileId.easting, tileId.northing }               // lower-left
        , { lod, tileId.easting + ts, tileId.northing }        // lower-right
        , { lod, tileId.easting, tileId.northing + ts }        // upper-left
        , { lod, tileId.easting + ts, tileId.northing +  ts }  // upper-right
    }};
}

inline IndexChildren children(const Index &index)
{
    Index base(index.lod + 1, index.easting << 1, index.northing << 1);

    return {{
        base                                                   // lower-left
        , { base.lod, base.easting + 1, base.northing }        // lower-right
        , { base.lod, base.easting, base.northing + 1 }        // upper-left
        , { base.lod, base.easting + 1, base.northing + 1 }    // upper-right
    }};
}

inline bool operator==(const TileId &lhs, const TileId &rhs)
{
    return ((lhs.lod == rhs.lod)
            && (lhs.easting == rhs.easting)
            && (lhs.northing == rhs.northing));
}

inline bool operator!=(const TileId &lhs, const TileId &rhs)
{
    return !(lhs == rhs);
}

inline bool isMetatile(const LodLevels &levels, const TileId &tile)
{
    return !(std::abs(tile.lod - levels.lod) % levels.delta);
}

inline Lod deltaDown(const LodLevels &levels, Lod lod)
{
    Lod res(lod + 1);
    while (std::abs(res - levels.lod) % levels.delta) {
        ++res;
    }
    return res;
}

inline bool above(long baseTileSize, const TileId &tile, const TileId &super)
{
    // tile cannot be above super tile
    if (tile.lod < super.lod) { return false; }

    auto te(tileExtents(baseTileSize, tile));
    auto se(tileExtents(baseTileSize, super));

    return ((te.ll(0) >= se.ll(0))
            && (te.ll(1) >= se.ll(1))
            && (te.ur(0) <= se.ur(0))
            && (te.ur(1) <= se.ur(1)));
}

inline int child(const Index &index)
{
    return (index.easting & 1l) + ((index.northing & 1l) << 1);
}

inline bool in(const LodRange &range, const TileId &tileId)
{
    return (tileId.lod >= range.min) && (tileId.lod <= range.max);
}

inline TileId findMetatile(const Properties &properties, TileId tileId
                           , bool useFoat)
{
    // topmost tile is either foat or 0-0-0
    TileId top((useFoat) ? (properties.foat) : TileId());

    while ((tileId != top) && !isMetatile(properties.metaLevels, tileId)) {
        tileId = tilestorage::parent(properties.alignment
                                     , properties.baseTileSize
                                     , tileId);
    }
    return tileId;
}

} } // namespace vtslibs::tilestorage

#endif // vtslibs_tilestorage_tileop_hpp_included_
