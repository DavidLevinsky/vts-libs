#ifndef vadstena_libs_vts_2d_hpp_included_
#define vadstena_libs_vts_2d_hpp_included_

#include <iostream>

#include <boost/gil/gil_all.hpp>

#include "./types2d.hpp"
#include "./mesh.hpp"
#include "./tileindex.hpp"

namespace vadstena { namespace vts {

typedef boost::gil::gray8_image_t GrayImage;

typedef boost::gil::gray8_image_t GrayImage;

GrayImage mask2d(const Mesh::CoverageMask &coverageMask
                 , const std::vector<SubMesh::SurfaceReference>
                 &surfaceReferences);

GrayImage mask2d(const MeshMask &mask);

GrayImage meta2d(const TileIndex &tileIndex, const TileId &tileId);

void saveCreditTile(std::ostream &out, const CreditTile &creditTile
                    , bool inlineCredits = true);

// inlines

inline GrayImage mask2d(const MeshMask &mask) {
    return mask2d(mask.coverageMask, mask.surfaceReferences);
}

} } // vadstena::vts

#endif // vadstena_libs_vts_2d_hpp_included_
