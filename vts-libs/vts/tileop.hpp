#ifndef vadstena_libs_vts_tileop_hpp_included_
#define vadstena_libs_vts_tileop_hpp_included_

#include "../entities.hpp"
#include "./basetypes.hpp"
#include "./filetypes.hpp"
#include "./properties.hpp"

namespace vadstena { namespace vts {

TileId fromAlignment(const Properties &properties, const TileId &tileId);

TileId parent(const Properties &properties, const TileId &tileId);

TileIdChildren children(const TileId &tileId);

bool isMetatile(const LodLevels &levels, const TileId &tile);

Lod deltaDown(const LodLevels &levels, Lod lod);

/** Check whether super tile is above (or exactly the same tile) as tile.
 */
bool above(const TileId &tile, const TileId &super);

int child(const TileId &tileId);

bool in(const LodRange &range, const TileId &tileId);

/** Calculates area of tile's mesh (m^2) and atlas (pixel^2).
 */
std::pair<double, double> area(const Tile &tile);

std::string asFilename(const TileId &tileId, TileFile type);

bool fromFilename(TileId &tileId, TileFile &type
                  , const std::string &str
                  , std::string::size_type offset = 0);

// inline stuff

inline TileId parent(const TileId &tileId)
{
    return TileId(tileId.lod - 1, tileId.x >> 1, tileId.y >> 1);
}

inline TileIdChildren children(const TileId &tileId)
{
    TileId base(tileId.lod + 1, tileId.x << 1, tileId.y << 1);

    // children must be from lower-left due to adapter
    return {{
        { base.lod, base.x, base.y + 1 }        // lower-left
        , { base.lod, base.x + 1, base.y + 1 }  // lower-right
        , base                                  // upper-left
        , { base.lod, base.x + 1, base.y }      // upper-right
    }};
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

inline bool above(const TileId &tile, const TileId &super)
{
    (void) tile;
    (void) super;
    // FIXME: implement me
    return false;
}

inline int child(const TileId &tileId)
{
    return (tileId.x & 1l) + ((tileId.y & 1l) << 1);
}

inline bool in(const LodRange &range, const TileId &tileId)
{
    return (tileId.lod >= range.min) && (tileId.lod <= range.max);
}

} } // namespace vadstena::vts

#endif // vadstena_libs_vts_hpp_included_
