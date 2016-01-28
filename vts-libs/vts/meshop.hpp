#ifndef vadstena_libs_vts_meshop_hpp
#define vadstena_libs_vts_meshop_hpp

#include <functional>

#include "./mesh.hpp"
#include "./basetypes.hpp"

namespace vadstena { namespace vts {

/** Mesh enhanced with projected data.
 */
struct EnhancedSubMesh {
    SubMesh mesh;
    math::Points3d projected;

    operator bool() const {
        return !mesh.vertices.empty();
    }
};

typedef std::vector<bool> VertexMask;

/** Converts projected vertex (whatever it means) to real world coordinates
 *  and to (normalized) external texture coordinates.
 */
struct MeshVertexConvertor {
    /** Convert vertex from projected space to physical space.
     */
    virtual math::Point3d vertex(const math::Point3d &v) const = 0;

    /** Convert vertex from projected space to external texture coordinates.
     */
    virtual math::Point2d etc(const math::Point3d &v) const = 0;

    /** Convert original external texture coordinates to proper texture
     *  coordinates.
     */
    virtual math::Point2d etc(const math::Point2d &v) const = 0;

    virtual ~MeshVertexConvertor() {}
};

typedef std::function<math::Point3d(const math::Point3d&)> MeshUnproject;

/** Clips and refines mesh that is in physical coordinates and provides means to
 *  work in projected space (i.e. spatial division system).
 *
 *  TODO: refinement is not implemented yet -- implement
 *
 * \param mesh enhanced mesh (mesh + projected vertices)
 * \param projectedExtents extents in projected space
 * \param lodDiff LOD difference (used as refinement factor)
 * \param convertor vertex convertor (for new vertex generation)
 * \param mask optional vertex mask (masked out vertices are removed)
 * \return clipped and refined mesh
 */
EnhancedSubMesh
refineAndClip(const EnhancedSubMesh &mesh
              , const math::Extents2 &projectedExtents, Lod lodDiff
              , const MeshVertexConvertor &convertor
              , const VertexMask &mask = VertexMask());

/** Simple interface to clip mesh that is in projected space (i.e. spatial
 *  division system).
 *
 * \param projectedMesh mesh in projected space
 * \param projectedExtents extents in projected space
 * \param mask optional vertex mask (masked out vertices are removed)
 * \return clipped mesh
 */
SubMesh clip(const SubMesh &projectedMesh
             , const math::Extents2 &projectedExtents
             , const VertexMask &mask = VertexMask());

} } // namespace vadstena::vts

#endif // vadstena_libs_vts_meshop_hpp
