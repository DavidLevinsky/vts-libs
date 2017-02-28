#ifndef vtslibs_vts0_types_hpp_included_
#define vtslibs_vts0_types_hpp_included_

#include <opencv2/core/core.hpp>

#include "geometry/parse-obj.hpp"

#include "./basetypes.hpp"
#include "./typesfwd.hpp"

namespace vtslibs { namespace vts0 {

/** A tile: mesh + atlas.
 */
struct Tile {
    Mesh mesh;
    Atlas atlas;
    MetaNode metanode;

    typedef std::vector<Tile> list;

    Tile() {}
    Tile(const Mesh &mesh, const Atlas &atlas, const MetaNode &metanode)
        : mesh(mesh), atlas(atlas), metanode(metanode)
    {}
};

} } // namespace vtslibs::vts0

#endif // vtslibs_vts0_types_hpp_included_
