#ifndef vadstena_libs_vts_basetypes_hpp_included_
#define vadstena_libs_vts_basetypes_hpp_included_

#include <string>

#include "math/geometry_core.hpp"

#include "utility/enum-io.hpp"

#include "../storage/lod.hpp"
#include "../storage/range.hpp"
#include "../registry.hpp"

namespace vadstena { namespace vts {

using storage::Lod;
using storage::Range;
using storage::LodRange;

using registry::TileRange;

typedef registry::ReferenceFrame::Division::Node RFNode;

/** Tile identifier (index in 3D space): LOD + tile index from upper-left corner
 *  in tile grid.
 */
struct TileId {
    Lod lod;
    unsigned int x;
    unsigned int y;

    bool operator<(const TileId &tid) const;
    bool operator==(const TileId &tid) const;
    bool operator!=(const TileId &tid) const { return !operator==(tid); }

    TileId(Lod lod = 0, unsigned int x = 0, unsigned int y = 0)
        : lod(lod), x(x), y(y)
    {}
};

struct Child : TileId {
    unsigned int index;

    Child(const TileId &tileId = TileId(), unsigned int index = 0)
        : TileId(tileId), index(index)
    {}

    Child(Lod lod, unsigned int x, unsigned int y, unsigned int index)
        : TileId(lod, x, y), index(index)
    {}
};

typedef std::array<Child, 4> Children;

/** Open mode
 */
enum class OpenMode {
    readOnly     //!< only getters are allowed
    , readWrite  //!< both getters and setters are allowed
};

enum class CreateMode {
    failIfExists //!< creation fails if tile set/storage already exists
    , overwrite  //!< existing tile set/storage is replace with new one
};

/** Reference frame node information.
 */
struct NodeInfo {
    /** Associated reference frame
     */
    const registry::ReferenceFrame *referenceFrame;

    /** Root of this subtree.
     */
    const RFNode *subtreeRoot;

    /** Node.
     */
    RFNode node;

    /** Creates node info from reference frame and tileId.
     *
     * Root node is found in reference frame and than current node derived.
     */
    NodeInfo(const registry::ReferenceFrame &referenceFrame
             , const TileId &tileId);

    /** Root node info.
     */
    NodeInfo(const registry::ReferenceFrame &referenceFrame)
        : referenceFrame(&referenceFrame), subtreeRoot(&referenceFrame.root())
        , node(*subtreeRoot)
    {}

    /** Root node info.
     */
    NodeInfo(const registry::ReferenceFrame &referenceFrame
             , const RFNode &node)
        : referenceFrame(&referenceFrame), subtreeRoot(&node), node(node)
    {}

    /** Node id.
     */
    RFNode::Id nodeId() const { return node.id; }

    /** Distance from root.
     */
    Lod distanceFromRoot() const { return node.id.lod - subtreeRoot->id.lod; }

    /** Returns child node. Uses same child assignment as children() functiom
     *  children() from tileop.
     */
    NodeInfo child(int childNum) const;
};

typedef std::string TilesetId;
typedef std::vector<TilesetId> TilesetIdList;

struct Glue {
    typedef TilesetIdList Id;
    Id id;
    std::string path;

    typedef std::map<Id, Glue> map;

    Glue() {}
    Glue(const Id &id, const std::string &path) : id(id), path(path) {}

    /** Returns true if glue references given tileset
     */
    bool references(const std::string &tilesetId) const;
};

// inline stuff

inline bool TileId::operator<(const TileId &tileId) const
{
    if (lod < tileId.lod) { return true; }
    else if (tileId.lod < lod) { return false; }

    if (x < tileId.x) { return true; }
    else if (tileId.x < x) { return false; }

    return y < tileId.y;
}

inline bool TileId::operator==(const TileId &tid) const
{
    return ((lod == tid.lod)
            && (x == tid.x)
            && (y == tid.y));
}

UTILITY_GENERATE_ENUM_IO(OpenMode,
    ((readOnly))
    ((readWrite))
)

UTILITY_GENERATE_ENUM_IO(CreateMode,
    ((failIfExists))
    ((overwrite))
)

} } // namespace vadstena::vts

#endif // vadstena_libs_vts_basetypes_hpp_included_
