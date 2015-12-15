#include <set>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>

#include <opencv2/highgui/highgui.hpp>

#include "math/transform.hpp"

#include "imgproc/rastermask/cvmat.hpp"
#include "imgproc/scanconversion.hpp"

#include "../csconvertor.hpp"

#include "./merge.hpp"
#include "../io.hpp"
#include "../refineandclip.hpp"

namespace fs = boost::filesystem;

namespace vadstena { namespace vts { namespace merge {

namespace {

/** Geo coordinates to coverage mask mapping.
 * NB: result is in pixel system: pixel centers have integral indices
 */
inline math::Matrix4 geo2mask(const math::Extents2 &extents
                              , const math::Size2 &gridSize)
{
    math::Matrix4 trafo(boost::numeric::ublas::identity_matrix<double>(4));

    auto es(size(extents));

    // scales
    math::Size2f scale(gridSize.width / es.width
                       , gridSize.height / es.height);

    // scale to grid
    trafo(0, 0) = scale.width;
    trafo(1, 1) = -scale.height;

    // move to origin and also move pixel centers to integral indices
    trafo(0, 3) = -extents.ll(0) * scale.width - 0.5;
    trafo(1, 3) = extents.ur(1) * scale.height - 0.5;

    return trafo;
}

/** Coverage mask mapping to geo coordinates.
 * NB: result is in pixel system: pixel centers have integral indices
 */
inline math::Matrix4 mask2geo(const math::Extents2 &extents
                              , const math::Size2 &gridSize)
{
    math::Matrix4 trafo(boost::numeric::ublas::identity_matrix<double>(4));

    auto es(size(extents));

    // scales
    math::Size2f scale(es.width / gridSize.width
                       , es.height / gridSize.height);

    // scale to grid
    trafo(0, 0) = scale.width;
    trafo(1, 1) = -scale.height;

    // move to origin
    trafo(0, 3) = extents.ll(0) + 0.5 * scale.width;
    trafo(1, 3) = extents.ur(1) - 0.5 * scale.height;

    return trafo;
}

/** Maps external texture coordinates from parent tile into subtile.
 *  Relationship defined by tile id, parent is a root of the tree (i.e. tile id
 *  0-0-0).
 */
inline math::Matrix4 etcNCTrafo(const TileId &id)
{
    math::Matrix4 trafo(boost::numeric::ublas::identity_matrix<double>(4));

    // LOD=0 -> identity
    if (!id.lod) { return trafo; }

    double tileCount(1 << id.lod);

    // number of tiles -> scale
    trafo(0, 0) = tileCount;
    trafo(1, 1) = tileCount;

    // NB: id.x is unsigned -> must cast to double first
    trafo(0, 3) = - double(id.x);
    trafo(1, 3) = (id.y + 1) - tileCount;

    return trafo;
}

/** Maps coverage coordinate into normalized external texture coordinates.
 */
inline math::Matrix4 coverage2EtcTrafo(const math::Size2 &gridSize)
{
    math::Matrix4 trafo(boost::numeric::ublas::identity_matrix<double>(4));

    // scale to normalized range (0, 1)
    trafo(0, 0) = 1.0 / gridSize.width;
    trafo(1, 1) = -1.0 / gridSize.height;

    // shift to proper orientation and cancel halfpixel offset
    trafo(0, 3) = 0.5 / gridSize.width;
    trafo(1, 3) = 1 - 0.5 / gridSize.height;

    return trafo;
}

math::Extents2 coverageExtents(double margin = .0)
{
    const auto grid(Mesh::coverageSize());
    return math::Extents2(-.5 - margin
                          , -.5 - margin
                          , grid.width - .5 + margin
                          , grid.height - .5 + margin);
}

} // namespace

Input::Input(Id id, const TileSet::Detail &owner, const TileId &tileId
             , const NodeInfo &nodeInfo)
    : id_(id), tileId_(tileId), owner_(&owner)
    , node_(owner.findMetaNode(tileId)), nodeInfo_(&nodeInfo)
{
}

bool Input::hasMesh() const
{
    return node_ && node_->geometry();
}

bool Input::hasAtlas() const
{
    return node_ && node_->internalTextureCount;
}

bool Input::hasNavtile() const
{
    return node_ && node_->navtile();
}

const Mesh& Input::mesh() const
{
    if (!mesh_) {
        mesh_ = owner_->getMesh(tileId_, node_);
    }

    return *mesh_;
}

const opencv::RawAtlas& Input::atlas() const
{
    if (!atlas_) {
        atlas_ = boost::in_place();
        owner_->getAtlas(tileId_, *atlas_, node_);
    }

    return *atlas_;
}

const opencv::NavTile& Input::navtile() const
{
    if (!navtile_) {
        navtile_ = boost::in_place();
        owner_->getNavTile(tileId_, *navtile_, node_);
    }

    return *navtile_;
}

/** Returns mesh vertices (vector per submesh) converted to coverage space.
 */
Vertices3List inputCoverageVertices(const Input &input
                                    , const NodeInfo &nodeInfo
                                    , const CsConvertor &conv)
{
    const auto trafo(input.sd2Coverage(nodeInfo));

    const auto &mesh(input.mesh());
    Vertices3List out(mesh.submeshes.size());
    auto iout(out.begin());
    for (const auto &sm : mesh.submeshes) {
        auto &ov(*iout++);
        for (const auto &v : sm.vertices) {
            ov.push_back(transform(trafo, conv(v)));
        }
    }
    return out;
}

const math::Matrix4 Input::sd2Coverage(const NodeInfo &nodeInfo) const
{
    return geo2mask(nodeInfo.node.extents, Mesh::coverageSize());
}

const math::Matrix4 Input::coverage2Sd(const NodeInfo &nodeInfo) const
{
    return mask2geo(nodeInfo.node.extents, Mesh::coverageSize());
}

const math::Matrix4 Input::coverage2Texture() const
{
    return coverage2EtcTrafo(Mesh::coverageSize());
}

Mesh& Output::forceMesh() {
    if (!mesh) {
        mesh = boost::in_place();
    }
    return *mesh;
}

opencv::RawAtlas& Output::forceAtlas() {
    if (!atlas) {
        atlas = boost::in_place();
    }
    return *atlas;
}

namespace {

/** Build uniform source by merging current and parent sources current data have
 *  precedence only tiles with mesh are used.
*/
Input::list mergeSource(const Input::list &currentSource
                   , const Input::list &parentSource)
{
    Input::list source;
    {
        auto icurrentSource(currentSource.begin())
            , ecurrentSource(currentSource.end());
        auto iparentSource(parentSource.begin())
            , eparentSource(parentSource.end());

        // merge head and common content
        while ((icurrentSource != ecurrentSource)
               && (iparentSource != eparentSource))
        {
            const auto &ts(*icurrentSource);
            const auto &ps(*iparentSource);
            if (ts < ps) {
                if (ts.hasMesh()) { source.push_back(ts); }
                ++icurrentSource;
            } else if (ps < ts) {
                if (ps.hasMesh()) { source.push_back(ps); }
                ++iparentSource;
            } else {
                if (ts.hasMesh()) {
                    source.push_back(ts);
                } else if (ps.hasMesh()) {
                    source.push_back(ps);
                }
                ++icurrentSource;
                ++iparentSource;
            }
        }

        // copy tail (one or another)
        for (; icurrentSource != ecurrentSource; ++icurrentSource) {
            if (icurrentSource->hasMesh()) {
                source.push_back(*icurrentSource);
            }
        }

        for (; iparentSource != eparentSource; ++iparentSource) {
            if (iparentSource->hasMesh()) {
                source.push_back(*iparentSource);
            }
        }
    }
    return source;
}

void rasterize(const Mesh &mesh, const cv::Scalar &color
               , cv::Mat &coverage
               , const TileId &diff = TileId())
{
    // size of source pixel in destination pixels
    int pixelSize(1 << diff.lod);

    // offset in destination pixels
    cv::Point2i offset(diff.x * coverage.cols, diff.y * coverage.rows);

    mesh.coverageMask.forEachQuad([&](uint xstart, uint ystart, uint xsize
                                      , uint ysize, bool)
    {
        // scale
        xstart *= pixelSize;
        ystart *= pixelSize;
        xsize *= pixelSize;
        ysize *= pixelSize;

        // shift
        xstart -= offset.x;
        ystart -= offset.y;

        cv::Point2i start(xstart, ystart);
        cv::Point2i end(xstart + xsize - 1, ystart + ysize - 1);

        cv::rectangle(coverage, start, end, color, CV_FILLED, 4);
    }, Mesh::CoverageMask::Filter::white);
}

void rasterize(const Mesh &mesh, const cv::Scalar &color
               , cv::Mat &coverage
               , const NodeInfo &srcNodeInfo
               , const NodeInfo &dstNodeInfo)
{
    (void) mesh;
    (void) color;
    (void) coverage;
    (void) srcNodeInfo;
    (void) dstNodeInfo;

    LOG(warn3)
        << "Cross SRS fallback merge is unsupported so far. "
        << "Skipping fallback tile.";
}

struct Coverage {
    typedef std::int16_t pixel_type;

    const TileId tileId;
    const Input::list &sources;
    cv::Mat coverage;
    bool hasHoles;
    std::vector<bool> indices;
    boost::optional<Input::Id> single;

    Coverage(const TileId &tileId, const NodeInfo &nodeInfo
             , const Input::list &sources)
        : tileId(tileId), sources(sources), hasHoles(false)
        , indices(sources.back().id() + 1, false)
    {
        generateCoverage(nodeInfo);
        analyze();
    }

    void getSources(Output &output) const {
        for (const auto &input : sources) {
            if (indices[input.id()]) {
                output.source.push_back(input);
            }
        }
    }

    const Input* getSingle() const {
        if (!single) { return nullptr; }
        auto fsources(std::find_if(sources.begin(), sources.end()
                                   , [&](const Input &input)
        {
            return input.id() == *single;
        }));
        if (fsources == sources.end()) { return nullptr; }
        return &*fsources;
    }

    bool covered(const Face &face, const math::Points3d &vertices
                 , Input::Id id) const
    {
        // TODO: repeat border for some distance to take triangles from the tile
        // margin into account
        std::vector<imgproc::Scanline> scanlines;

        const math::Point3 *tri[3] = {
            &vertices[face[0]]
            , &vertices[face[1]]
            , &vertices[face[2]]
        };

        imgproc::scanConvertTriangle
            (*tri[0], *tri[1], *tri[2], 0, coverage.rows, scanlines);

        for (const auto &sl : scanlines) {
            bool covered(false);
            imgproc::processScanline(sl, 0, coverage.cols
                                     , [&](int x, int y, float)
            {
                // TODO: early exit
                if (coverage.at<pixel_type>(y, x) == id) {
                    covered = true;
                }
            });
            if (covered) { return true; }
        }

        // do one more check in case the triangle is thinner than one pixel
        for (int i = 0; i < 3; ++i) {
            if (check((*tri[i])(0), (*tri[i])(1), id)) {
                return true;
            }
        }

        return false;
    }

    void fillMeshMask(Output &out) const {
        if (!out.mesh) { return; }

        // we have mesh -> fill
        auto &cm(out.mesh->coverageMask);

        if (hasHoles) {
            // start with whole black mask and set non-hole pixels
            cm.reset(false);
            for (int j(0); j < coverage.rows; ++j) {
                for (int i(0); i < coverage.cols; ++i) {
                    // non-hole -> set in mask
                    if (coverage.at<pixel_type>(j, i) >= 0) {
                        cm.set(j, i);
                    }
                }
            }
        } else {
            // fully covered
            cm.reset(true);
        }
    }

    void dump(const fs::path &dump, const TileId &tileId) const {
        const cv::Vec3b colors[10] = {
            { 0, 0, 0 }
            , { 0, 0, 255 }
            , { 0, 255, 0 }
            , { 255, 0, 0 }
            , { 0, 255, 255 }
            , { 255, 0, 255 }
            , { 255, 255, 0 }
            , { 255, 255, 255 }
        };

        cv::Mat img(coverage.rows, coverage.cols, CV_8UC3);

        std::transform(coverage.begin<pixel_type>()
                       , coverage.end<pixel_type>()
                       , img.begin<cv::Vec3b>()
                       , [&](pixel_type v) { return colors[v + 1]; });

        const auto filename
            (dump / str(boost::format("coverage-%s.png") % tileId));
        create_directories(filename.parent_path());
        imwrite(filename.string(), img);
    }

private:
    void generateCoverage(const NodeInfo &nodeInfo) {
        // prepare coverage map (set to invalid index)
        auto coverageSize(Mesh::coverageSize());
        coverage.create(coverageSize.height, coverageSize.width, CV_16S);
        coverage = cv::Scalar(-1);

        for (const auto &input : sources) {
            if (nodeInfo.node.srs == input.nodeInfo().node.srs) {
                // same SRS -> mask is rendered as is (possible scale and shift)
                rasterize(input.mesh(), input.id(), coverage
                          , local(input.tileId().lod, tileId));
            } else {
                rasterize(input.mesh(), input.id(), coverage
                          , input.nodeInfo(), nodeInfo);
            }
        }
    }

    void analyze() {
        for (auto j(0); j < coverage.rows; ++j) {
            for (auto i(0); i < coverage.cols; ++i) {
                auto v(coverage.at<pixel_type>(j, i));
                if (v == -1) {
                    hasHoles = true;
                } else {
                    indices[v] = true;
                }
            }
        }

        int count(0);
        for (Input::Id id(0), eid(indices.size()); id != eid; ++id) {
            if (!indices[id]) { continue; }
            if (!single) {
                single = id;
            }
            ++count;
        }
        if (count > 1) { single = boost::none; }

        // convert special negative value back to no value
        if (single && (single < 0)) { single = boost::none; }
    }

    bool check(float x, float y, Input::Id id) const {
        int xx(std::round(x));
        int yy(std::round(y));
        if ((xx < 0) || (xx >= coverage.cols)) { return false; }
        if ((yy < 0) || (yy >= coverage.rows)) { return false; }
        return (coverage.at<pixel_type>(yy, xx) == id);
    }
};

class MeshFilter {
public:
    MeshFilter(const SubMesh &original, int submeshIndex
               , const math::Points3 &originalCoverage
               , const Input &input, const Coverage &coverage)
        : original_(original), submeshIndex_(submeshIndex)
        , originalCoverage_(originalCoverage)
        , input_(input)
        , mesh_(result_.mesh), coverageVertices_(result_.projected)
        , vertexMap_(original.vertices.size(), -1)
        , tcMap_(original.tc.size(), -1)
    {
        original_.cloneMetadataInto(mesh_);
        filter(coverage);
    }

    void addTo(Output &out, double uvAreaScale = 1.0);

    EnhancedSubMesh result() const { return result_; }

    operator bool() const { return mesh_.vertices.size(); }

private:
    void filter(const Coverage &coverage) {
        // each face covered at least by one pixel is added to new mesh
        for (int f(0), ef(original_.faces.size()); f != ef; ++f) {
            // only vertices
            if (coverage.covered
                (original_.faces[f], originalCoverage_, input_.id()))
            {
                addFace(f);
            }
        }
    }

    void addFace(int faceIndex) {
        const auto &of(original_.faces[faceIndex]);

        mesh_.faces.emplace_back
            (addVertex(of(0)), addVertex(of(1)), addVertex(of(2)));

        if (!original_.facesTc.empty()) {
            const auto &oftc(original_.facesTc[faceIndex]);
            mesh_.facesTc.emplace_back
                (addTc(oftc(0)), addTc(oftc(1)), addTc(oftc(2)));
        }
    }

    int addVertex(int i) {
        auto &m(vertexMap_[i]);
        if (m < 0) {
            // new vertex
            m = mesh_.vertices.size();
            mesh_.vertices.push_back(original_.vertices[i]);
            if (!original_.etc.empty()) {
                mesh_.etc.push_back(original_.etc[i]);
            }

            // coverage vertices (if needed)
            coverageVertices_.push_back(originalCoverage_[i]);
        }
        return m;
    }

    int addTc(int i) {
        auto &m(tcMap_[i]);
        if (m < 0) {
            // new vertex
            m = mesh_.tc.size();
            mesh_.tc.push_back(original_.tc[i]);
        }
        return m;
    }

    const SubMesh &original_;
    const int submeshIndex_;
    const math::Points3 originalCoverage_;
    const Input &input_;

    EnhancedSubMesh result_;
    SubMesh &mesh_;
    math::Points3 &coverageVertices_;
    std::vector<int> vertexMap_;
    std::vector<int> tcMap_;
};

namespace {

SubMesh::list::iterator firstNonTextured(SubMesh::list &submeshes)
{
    return std::find_if(submeshes.begin(), submeshes.end()
                        , [](const SubMesh &sm)
    {
        return sm.tc.empty();
    });
}

} // namespace

void addInputToOutput(Output &out, const Input &input
                      , SubMesh mesh
                      , int submeshIndex
                      , double uvAreaScale)
{
    bool textured(false);
    if (input.hasAtlas() && input.atlas().valid(submeshIndex)) {
        out.forceAtlas().add(input.atlas().get(submeshIndex));
        textured = true;
    }

    // find place where to put new submesh
    auto &submeshes(out.forceMesh().submeshes);
    auto fsubmeshes(!textured ? submeshes.end()
                    : firstNonTextured(submeshes));
    // insert submesh
    fsubmeshes = submeshes.insert(fsubmeshes, mesh);

    // update scale (TODO: multiply?)
    fsubmeshes->uvAreaScale = uvAreaScale;
}

void MeshFilter::addTo(Output &out, double uvAreaScale)
{
    addInputToOutput(out, input_, mesh_, submeshIndex_, uvAreaScale);
}

class SdMeshConvertor : public MeshVertexConvertor {
public:
    SdMeshConvertor(const Input &input, const NodeInfo &nodeInfo
                    , const TileId &tileId)
        : geoTrafo_(input.coverage2Sd(nodeInfo))
        , geoConv_(nodeInfo.node.srs
                   , nodeInfo.referenceFrame->model.physicalSrs)
        , etcNCTrafo_(etcNCTrafo(tileId))
        , coverage2Texture_(input.coverage2Texture())
    {}

    virtual math::Point3d vertex(const math::Point3d &v) const {
        // point is in node SD SRS
        return geoConv_(transform(geoTrafo_, v));
    }

    virtual math::Point2d etc(const math::Point3d &v) const {
        // point is in projected space (i.e. in coverage raster)
        return transform(coverage2Texture_, v);
    }

    virtual math::Point2d etc(const math::Point2d &v) const {
        // point is in the input's texture coordinates system
        return transform(etcNCTrafo_, v);
    }

private:
    /** Linear transformation from local coverage coordinates to node's SD SRS.
     */
    math::Matrix4 geoTrafo_;

    /** Convertor between node's SD SRS and reference frame's physical SRS.
     */
    CsConvertor geoConv_;

    /** Converts external texture coordinates between fallback tile and current
     *  tile.
     */
    math::Matrix4 etcNCTrafo_;

    /** Converts between coverage coordinates and normalized external texture
     *  cooridnates.
     */
    math::Matrix4 coverage2Texture_;
};

Output singleSourced(const TileId &tileId, const NodeInfo &nodeInfo
                     , CsConvertor phys2sd, const Input &input)
{
    Output result(tileId, input);
    if (input.tileId().lod == tileId.lod) {
        // as is -> copy
        result.mesh = input.mesh();
        if (input.hasAtlas()) { result.atlas = input.atlas(); }
        if (input.hasNavtile()) { result.navtile = input.navtile(); }
        return result;
    }

    const auto localId(local(input.tileId().lod, tileId));

    // clip source mesh/navtile

    const auto coverageVertices
        (inputCoverageVertices(input, nodeInfo, phys2sd));
    SdMeshConvertor sdmc(input, nodeInfo, localId);

    std::size_t smIndex(0);
    for (const auto &sm : input.mesh()) {
        auto refined
            (refineAndClip({ sm, coverageVertices[smIndex] }
                           , coverageExtents(1.), localId.lod, sdmc));

        if (refined) {
            addInputToOutput(result, input, refined.mesh, smIndex
                             , (1 << localId.lod));
        }

        ++smIndex;
    }

    // cut coverage mask from original mesh
    result.forceMesh().coverageMask
        = input.mesh().coverageMask.subTree
        (Mesh::coverageSize(), localId.lod, localId.x, localId.y);

    // done
    return result;
}

} // namespace

Output mergeTile(const TileId &tileId
                 , const NodeInfo &nodeInfo
                 , const Input::list &currentSource
                 , const Input::list &parentSource)
{
    auto source(mergeSource(currentSource, parentSource));

    // from here, all input tiles have geometry -> no need to check for mesh
    // presence

    // merge result
    Output result(tileId);

    if (source.empty()) { return result; }

    CsConvertor phys2sd(nodeInfo.referenceFrame->model.physicalSrs
                        , nodeInfo.node.srs);

    if ((source.size() == 1)) {
        // just one source
        return singleSourced(tileId, nodeInfo, phys2sd, source.back());
    }

    // analyze coverage
    Coverage coverage(tileId, nodeInfo, source);

    if (const auto *dumpDir = ::getenv("MERGE_MASK_DUMP_DIR")) {
        coverage.dump(dumpDir, tileId);
    }

    // get contributing tile sets
    coverage.getSources(result);

    if (coverage.single) {
        // single source
        if (*coverage.single < 0) {
            // nothing at all
            return result;
        }

        // just one source
        if (const auto input = coverage.getSingle()) {
            return singleSourced(tileId, nodeInfo, phys2sd, *input);
        }

        // OK
        return result;
    }

    // TODO: merge navtile based on navtile coverage

    // process all input tiles from result source (i.e. only those contributing
    // to the tile)
    for (const auto &input : result.source) {
        const auto &mesh(input.mesh());

        // get current mesh vertices converted to coverage coordinate system
        const auto coverageVertices
            (inputCoverageVertices(input, nodeInfo, phys2sd));

        auto icoverageVertices(coverageVertices.begin());

        // localize tileId -> used to map fallback content into this tile
        const auto localId(local(input.tileId().lod, tileId));

        // traverse all submeshes
        for (int m(0), em(mesh.submeshes.size()); m != em; ++m) {
            // accumulate new mesh
            MeshFilter mf(mesh[m], m, *icoverageVertices++, input, coverage);
            if (!mf) {
                // empty result mesh -> nothing to do
                continue;
            }

            if (!localId.lod) {
                // add as is
                mf.addTo(result);
                continue;
            }

            // fallback mesh: we have to clip and refine accumulated
            // mesh and process again
            auto refined
                (refineAndClip(mf.result(), coverageExtents(1.), localId.lod
                               , SdMeshConvertor(input, nodeInfo, localId)));

            MeshFilter rmf(refined.mesh, m, refined.projected
                           , input, coverage);
            if (rmf) {
                // add refined
                rmf.addTo(result, (1 << localId.lod));
            }
        }
    }

    // generate coverage mask
    coverage.fillMeshMask(result);

    return result;
}

} } } // namespace vadstena::vts::merge
