#ifndef vadstena_libs_vts_tileop_hpp_included_
#define vadstena_libs_vts_tileop_hpp_included_

#include <new>

#include "../storage/filetypes.hpp"

#include "./basetypes.hpp"
#include "./nodeinfo.hpp"
#include "./types.hpp"

namespace vadstena { namespace vts {

using storage::TileFile;
using storage::File;

TileId parent(const TileId &tileId, Lod diff = 1);
TileRange::point_type parent(const TileRange::point_type &point, Lod diff = 1);
TileRange parent(const TileRange &tileRange, Lod diff = 1);
TileRange tileRange(const TileId &tileId);

Children children(const TileId &tileId);
TileId lowestChild(const TileId &tileId, Lod diff = 1);
TileRange::point_type lowestChild(const TileRange::point_type &point
                                  , Lod diff = 1);
TileRange childRange(const TileRange &tileRange, Lod diff = 1);

TileRange shiftRange(Lod srcLod, const TileRange &tileRange, Lod dstLod);

/** Check whether super tile is above (or exactly the same tile) as tile.
 */
bool above(const TileId &tile, const TileId &super);

int child(const TileId &tileId);

bool in(const LodRange &range, const TileId &tileId);

/** Finds lowest common ancestor of two tiles defined as a lod and a range.
 */
TileId commonAncestor(Lod lod, TileRange range);

/** Calculates area of tile's mesh (m^2) and atlas (pixel^2).
 */
std::pair<double, double> area(const Tile &tile);

std::string asFilename(const TileId &tileId, TileFile type);

bool fromFilename(TileId &tileId, TileFile &type, unsigned int &subTileIndex
                  , const std::string &str
                  , std::string::size_type offset = 0
                  , bool *raw = nullptr);

std::string fileTemplate(TileFile type
                         , boost::optional<unsigned int> revision
                         = boost::none);

std::size_t tileCount(Lod lod);

math::Size2f tileSize(const math::Extents2 &rootExtents, Lod lod);

const RFNode::Id& rfNodeId(const TileId &tileId);
const TileId& tileId(const RFNode::Id &rfNodeId);
const TileId& tileId(const NodeInfo &nodeInfo);

TileId local(Lod rootLod, const TileId &tileId);

TileRange::point_type point(const TileId &tileId);
TileId tileId(Lod lod, const TileRange::point_type &point);
TileRange::point_type point(const NodeInfo &nodeInfo);

bool tileRangesOverlap(const TileRange &a, const TileRange &b);
TileRange tileRangesIntersect(const TileRange &a, const TileRange &b);

/** Non-throwing version of tileRangesIntersect(a, b): return invalid TileRange
 *  in case of no overlap.
 */
TileRange tileRangesIntersect(const TileRange &a, const TileRange &b
                              , const std::nothrow_t&);
math::Size2_<TileRange::value_type> tileRangesSize(const TileRange &tr);

// inline stuff

inline TileId parent(const TileId &tileId, Lod diff)
{
    // do not let new id to go above root
    if (diff > tileId.lod) { return {}; }
    return TileId(tileId.lod - diff, tileId.x >> diff, tileId.y >> diff);
}

inline TileRange::point_type parent(const TileRange::point_type &point
                                    , Lod diff)
{
    return { point(0) >> diff, point(1) >> diff };
}

inline TileRange parent(const TileRange &tileRange, Lod diff)
{
    return { parent(tileRange.ll, diff), parent(tileRange.ur, diff) };
}

inline TileRange tileRange(const TileId &tileId)
{
    return { tileId.x, tileId.y, tileId.x, tileId.y };
}

inline Children children(const TileId &tileId)
{
    TileId base(tileId.lod + 1, tileId.x << 1, tileId.y << 1);

    return {{
        { base, 0 }                                  // upper-left
        , { base.lod, base.x + 1, base.y, 1 }      // upper-right
        , { base.lod, base.x, base.y + 1, 2 }        // lower-left
        , { base.lod, base.x + 1, base.y + 1, 3}  // lower-right
    }};
}

inline TileId lowestChild(const TileId &tileId, Lod diff)
{
    return TileId(tileId.lod + diff, tileId.x << diff, tileId.y << diff);
}

inline TileRange::point_type lowestChild(const TileRange::point_type &point
                                         , Lod diff)
{
    return { point(0) << diff, point(1) << diff };
}

inline TileRange childRange(const TileRange &tileRange, Lod diff)
{
    // lowest child of lower left point + original upper right point
    TileRange tr(lowestChild(tileRange.ll, diff), tileRange.ur);
    // move upper right one tile away
    ++tr.ur(0); ++tr.ur(1);

    // calculate lowest child of this point
    tr.ur = lowestChild(tr.ur, diff);

    // and move back inside original tile
    --tr.ur(0); --tr.ur(1);

    // fine
    return tr;
}

inline TileRange shiftRange(Lod srcLod, const TileRange &tileRange, Lod dstLod)
{
    if (srcLod == dstLod) {
        // no-op
        return tileRange;
    }

    if (dstLod > srcLod) {
        // child range
        return childRange(tileRange, dstLod - srcLod);
    }

    // parent range
    return { parent(tileRange.ll, srcLod - dstLod)
            , parent(tileRange.ur, srcLod - dstLod) };

}

inline int child(const TileId &tileId)
{
    return (tileId.x & 1l) + ((tileId.y & 1l) << 1);
}

inline bool in(const LodRange &range, const TileId &tileId)
{
    return (tileId.lod >= range.min) && (tileId.lod <= range.max);
}

inline std::size_t tileCount(Lod lod)
{
    return std::size_t(1) << lod;
}

inline const RFNode::Id& rfNodeId(const TileId &tileId)
{
    return tileId;
}

inline math::Size2f tileSize(const math::Extents2 &rootExtents, Lod lod)
{
    auto tc(tileCount(lod));
    auto rs(math::size(rootExtents));
    return { rs.width / tc, rs.height / tc };
}

inline const TileId& tileId(const RFNode::Id &rfNodeId)
{
    return rfNodeId;
}

inline const TileId& tileId(const NodeInfo &nodeInfo)
{
    return nodeInfo.nodeId();
}

inline TileId local(Lod rootLod, const TileId &tileId)
{
    if (rootLod >= tileId.lod) { return {}; }

    const auto ldiff(tileId.lod - rootLod);
    const auto mask((1 << ldiff) - 1);
    return TileId(ldiff, tileId.x & mask, tileId.y & mask);
}

inline TileRange::point_type point(const TileId &tileId)
{
    return { tileId.x, tileId.y };
}

inline TileId tileId(Lod lod, const TileRange::point_type &point)
{
    return { lod, point(0), point(1) };
}

inline TileRange::point_type point(const NodeInfo &nodeInfo)
{
    return point(nodeInfo.nodeId());
}

inline bool tileRangesOverlap(const TileRange &a, const TileRange &b)
{
    // range is inclusive -> operator<=
    return ((a.ll(0) <= b.ur(0)) && (b.ll(0) <= a.ur(0))
             && (a.ll(1) <= b.ur(1)) && (b.ll(1) <= a.ur(1)));
}

inline TileRange tileRangesIntersect(const TileRange &a, const TileRange &b)
{
    if (!tileRangesOverlap(a, b)) {
        throw math::NoIntersectError
            ("Tile ranges do not overlap, cannot compute intersection");
    }

    return TileRange(TileRange::point_type
                     (std::max(a.ll[0], b.ll[0])
                      , std::max(a.ll[1], b.ll[1]))
                     , TileRange::point_type
                     (std::min(a.ur[0], b.ur[0])
                      , std::min(a.ur[1], b.ur[1])));
}

inline TileRange tileRangesIntersect(const TileRange &a, const TileRange &b
                                     , const std::nothrow_t&)
{
    if (!tileRangesOverlap(a, b)) {
        return TileRange(math::InvalidExtents{});
    }

    return TileRange(TileRange::point_type
                     (std::max(a.ll[0], b.ll[0])
                      , std::max(a.ll[1], b.ll[1]))
                     , TileRange::point_type
                     (std::min(a.ur[0], b.ur[0])
                      , std::min(a.ur[1], b.ur[1])));
}

inline math::Size2_<TileRange::value_type> tileRangesSize(const TileRange &tr)
{
    return math::Size2_<TileRange::value_type>
        (tr.ur(0) - tr.ll(0) + 1, tr.ur(1) - tr.ll(1) + 1);
}

} } // namespace vadstena::vts

#endif // vadstena_libs_vts_tileop_hpp_included_
