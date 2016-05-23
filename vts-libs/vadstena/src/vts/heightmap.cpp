#include <cmath>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>

#include <opencv2/highgui/highgui.hpp>

#include "dbglog/dbglog.hpp"

#include "utility/format.hpp"

#include "imgproc/filtering.hpp"
#include "imgproc/reconstruct.hpp"

#include "geo/geodataset.hpp"

#include "vts-libs/vts/io.hpp"
#include "vts-libs/vts/tileop.hpp"
#include "vts-libs/vts/csconvertor.hpp"
#include "vts-libs/vts/opencv/navtile.hpp"

#include "./heightmap.hpp"

typedef vts::opencv::NavTile::DataType NTDataType;

namespace def {

constexpr NTDataType Infinity(std::numeric_limits<NTDataType>::max());
constexpr NTDataType InvalidHeight(Infinity);

const auto *DumpDir(::getenv("HEIGHTMAP_DUMP_DIR"));

} // namespace def

HeightMap::Accumulator::Accumulator(vts::Lod lod)
    : lod_(lod)
    , tileSize_(vts::NavTile::size())
    , tileRange_(math::InvalidExtents{})
{
}

cv::Mat& HeightMap::Accumulator::tile(const vts::TileId &tileId)
{
    if (tileId.lod != lod_) {
        LOGTHROW(err2, std::runtime_error)
            << "Unexpected tile ID " << tileId
            << ": this heightmap accumulator expects tiles at LOD "
            << lod_ << ".";
    }

    const Index index(tileId.x, tileId.y);

    const auto ftiles(tiles_.find(index));
    if (ftiles != tiles_.end()) {
        return ftiles->second;
    }

    // create new tile (all values set to +oo
    auto res
        (tiles_.insert
         (Tiles::value_type
          (index, cv::Mat
           (tileSize_.height, tileSize_.width, vts::opencv::NavTile::CvDataType
            , cv::Scalar(def::InvalidHeight)))));
    update(tileRange_, index);

    return res.first->second;
}

namespace {

template <typename Operator>
class Morphology {
public:
    Morphology(cv::Mat &data, cv::Mat &tmp, int kernelSize)
        : in_(data), tmp_(tmp)
    {
        tmp_ = cv::Scalar(def::InvalidHeight);
        run(kernelSize);
        std::swap(in_, tmp_);
    }

private:
    void run(int kernelSize);

    cv::Mat &in_;
    cv::Mat &tmp_;
};

template <typename Operator>
void Morphology<Operator>::run(int kernelSize)
{
    kernelSize /= 2;

    for (int y(0); y != in_.rows; ++y) {
        for (int x(0); x != in_.cols; ++x) {
            // skip invalid data
            if (in_.template at<NTDataType>(y, x) == def::InvalidHeight) {
                continue;
            }

            Operator op;

            for (int j = -kernelSize; j <= kernelSize; ++j) {
                const int yy(y + j);
                if ((yy < 0) || (yy >= in_.rows)) { continue; }

                for (int i = -kernelSize; i <= kernelSize; ++i) {
                    const int xx(x + i);
                    if ((xx < 0) || (xx >= in_.cols)) { continue; }

                    op(in_.template at<NTDataType>(yy, xx));
                }
            }

            // store only valid value
            if (!op.valid()) { continue; }

            // store
            tmp_.template at<NTDataType>(y, x) = op.get();
        }
    }
}

class Erosion {
public:
    Erosion() : value_(def::Infinity) {}

    inline void operator()(NTDataType value) {
        if (value != def::InvalidHeight) {
            value_ = std::min(value_, value);
        }
    }

    inline bool valid() const { return value_ != def::Infinity; }
    inline NTDataType get() const { return value_; }

private:
    NTDataType value_;
};

class Dilation {
public:
    inline Dilation() : value_(-def::Infinity) {}

    inline void operator()(NTDataType value) {
        if (value != def::InvalidHeight) {
            value_ = std::max(value_, value);
        }
    }

    inline bool valid() const { return value_ != -def::Infinity; }
    inline NTDataType get() const { return value_; }

private:
    NTDataType value_;
};

void dtmize(cv::Mat &pane, int count)
{
    LOG(info3) << "Generating DTM from heightmap ("
               << pane.cols << "x" << pane.rows << " pixels).";

    cv::Mat tmp(pane.rows, pane.cols, pane.type());

    LOG(info2) << "Eroding heightmap (" << count << " iterations).";
    for (int c(0); c < count; ++c) {
        LOG(info1) << "Erosion iteration " << c << ".";
        Morphology<Erosion>(pane, tmp, 2);
    }

    LOG(info2) << "Dilating heightmap (" << count << " iterations).";
    for (int c(0); c < count; ++c) {
        LOG(info1) << "Dilation iteration " << c << ".";
        Morphology<Dilation>(pane, tmp, 2);
    }
}

template <typename ...Args>
void debugDump(const HeightMap &hm, const std::string &format, Args &&...args)
{
    if (!def::DumpDir || hm.empty()) { return; }

    hm.dump(boost::filesystem::path(def::DumpDir)
            / utility::format(format, std::forward<Args>(args)...));
}

math::Extents2 worldExtents(vts::Lod lod, const vts::TileRange &tileRange
                            , const vr::ReferenceFrame &referenceFrame
                            , std::string &srs)
{
    if (!valid(tileRange)) { return math::Extents2(math::InvalidExtents{}); }

    vts::NodeInfo nodeLl
        (referenceFrame
         , vts::TileId(lod, tileRange.ll(0), tileRange.ur(1)));
    vts::NodeInfo nodeUr
        (referenceFrame
         , vts::TileId(lod, tileRange.ur(0), tileRange.ll(1)));

    if (!compatible(nodeLl, nodeUr)) {
        LOGTHROW(err2, std::runtime_error)
            << "Heightmap can be constructed from nodes in the "
            "same node subtree.";
    }
    srs = nodeLl.node.srs;

    return math::Extents2(nodeLl.node.extents.ll, nodeUr.node.extents.ur);
}

math::Size2 calculateSizeInTiles(const vts::TileRange &tileRange)
{
    if (!valid(tileRange)) { return {}; }

    math::Size2 s(math::size(tileRange));
    ++s.width;
    ++s.height;
    return s;
}

math::Size2 calculateSizeInPixels(const math::Size2 &tileGrid
                                  , const math::Size2 &sizeInTiles)
{
    if (empty(sizeInTiles)) { return {}; }
    return { 1 + sizeInTiles.width * tileGrid.width
            , 1 + sizeInTiles.height * tileGrid.height };
}

} // namespace

HeightMapBase::HeightMapBase(const vr::ReferenceFrame &referenceFrame
                             , vts::Lod lod, vts::TileRange tileRange)
    : referenceFrame_(&referenceFrame)
    , tileSize_(vts::NavTile::size())
    , tileGrid_(tileSize_.width - 1, tileSize_.height - 1)
    , lod_(lod)
    , tileRange_(tileRange)
    , sizeInTiles_(calculateSizeInTiles(tileRange_))
    , sizeInPixels_(calculateSizeInPixels(tileGrid_, sizeInTiles_))
    , pane_(sizeInPixels_.height, sizeInPixels_.width
            , vts::opencv::NavTile::CvDataType
            , cv::Scalar(def::InvalidHeight))
    , worldExtents_(worldExtents(lod_, tileRange_, *referenceFrame_, srs_))
{}

HeightMap::HeightMap(Accumulator &&a
                     , const vr::ReferenceFrame &referenceFrame
                     , double dtmExtractionRadius)
    : HeightMapBase(referenceFrame, a.lod_, a.tileRange_)
{
    LOG(info2) << "Copying heightmaps from tiles into one pane.";
    for (const auto &item : a.tiles_) {
        // copy tile in proper place
        Accumulator::Index offset(item.first - tileRange_.ll);
        offset(0) *= tileGrid_.width;
        offset(1) *= tileGrid_.height;
        cv::Mat tile(pane_, cv::Range(offset(1), offset(1) + tileSize_.height)
                     , cv::Range(offset(0), offset(0) + tileSize_.width));
        item.second.copyTo(tile);
    }

    // drop original pane
    a.tiles_.clear();

    debugDump(*this, "hm-plain.png");
    dtmize(pane_, std::ceil(dtmExtractionRadius));
    debugDump(*this, "hm-dtmized.png");
}

namespace {

HeightMap::Accumulator::Index index(const vts::TileId &tileId)
{
    return HeightMap::Accumulator::Index(tileId.x, tileId.y);
}

vts::Lod lod(const vts::MeshOpInput::list &source)
{
    return (source.empty() ? 0 : source.front().tileId().lod);
}

vts::TileRange tileRange(const vts::MeshOpInput::list &source)
{
    vts::TileRange tr(math::InvalidExtents{});
    for (const auto &input : source) {
        if (input.hasNavtile()) {
            update(tr, index(input.tileId()));
        }
    }
    return tr;
}

} // namespace

HeightMap::HeightMap(const vts::TileId &tileId
                     , const vts::MeshOpInput::list &source
                     , const vr::ReferenceFrame &referenceFrame)
    : HeightMapBase(referenceFrame, lod(source), tileRange(source))
{
    for (const auto &input : source) {
        if (!input.hasNavtile()) { continue; }

        // copy tile in proper place
        Accumulator::Index offset(index(input.tileId()) - tileRange_.ll);
        offset(0) *= tileGrid_.width;
        offset(1) *= tileGrid_.height;
        cv::Mat tile(pane_, cv::Range(offset(1), offset(1) + tileSize_.height)
                     , cv::Range(offset(0), offset(0) + tileSize_.width));

        const auto &nt(input.navtile());
        nt.data().copyTo(tile, renderCoverage(nt));
    }

    // debug
    debugDump(*this, "src-hm-%s.png", tileId);
}

namespace {

class HeightMapRaster
    : public imgproc::BoundsValidator<HeightMapRaster>
{
public:
    typedef cv::Vec<NTDataType, 1> value_type;
    typedef NTDataType channel_type;

    explicit HeightMapRaster(const cv::Mat &mat, NTDataType invalidValue)
        : mat_(mat), invalidValue_(invalidValue)
    {}

    int channels() const { return 1; };

    const value_type& operator()(int x, int y) const {
        return mat_.at<value_type>(y, x);
    }

    channel_type saturate(double value) const {
        return value;
    }

    channel_type undefined() const { return invalidValue_; }

    int width() const { return mat_.cols; }
    int height() const { return mat_.rows; }
    math::Size2i size() const { return { mat_.cols, mat_.rows }; }

    bool valid(int x, int y) const {
        return (imgproc::BoundsValidator<HeightMapRaster>::valid(x, y)
                && (mat_.at<NTDataType>(y, x) != invalidValue_));
    }

private:
    const cv::Mat &mat_;
    NTDataType invalidValue_;
};

} // namespace

void HeightMap::resize(vts::Lod lod)
{
    if (lod > lod_) {
        LOGTHROW(err2, std::runtime_error)
            << "Heightmap can be only shrinked.";
    }
    // no-op if same lod
    if (lod == lod_) { return; }

    LOG(info2) << "Resizing heightmap from LOD " << lod_ << " to LOD "
               << lod << ".";

    // calculate new tile range
    // go up in the tile tree
    vts::TileId ll(lod_, tileRange_.ll(0), tileRange_.ll(1));
    vts::TileId ur(lod_, tileRange_.ur(0), tileRange_.ur(1));
    auto localId(local(lod, ll));
    int scale(1 << localId.lod);
    ll = parent(ll, localId.lod);
    ur = parent(ur, localId.lod);

    vts::TileRange tileRange(ll.x, ll.y, ur.x, ur.y);
    math::Size2 sizeInTiles(math::size(tileRange));
    ++sizeInTiles.width; ++sizeInTiles.height;
    math::Size2 sizeInPixels(calculateSizeInPixels(tileGrid_, sizeInTiles));
    math::Point2i offset(-localId.x * tileGrid_.width
                         , -localId.y * tileGrid_.height);

    // create new pane
    cv::Mat tmp(sizeInPixels.height, sizeInPixels.width
                , vts::opencv::NavTile::CvDataType
                , cv::Scalar(def::InvalidHeight));

    // filter heightmap from pane_ into tmp using filter
    HeightMapRaster srcRaster(pane_, def::InvalidHeight);
    math::CatmullRom2 filter(4.0 * localId.lod, 4.0 * localId.lod);
    for (int j(0); j < tmp.rows; ++j) {
        for (int i(0); i < tmp.cols; ++i) {
            // map x,y into original pane (applying scale and tile offset)
            math::Point2 srcPos(scale * i + offset(0)
                                , scale * j + offset(1));

            if (srcRaster.valid(srcPos(0), srcPos(1))) {
                auto nval(imgproc::reconstruct(srcRaster, filter, srcPos)[0]);
                tmp.at<NTDataType>(j, i) = nval;
            }
        }
    }

    // set new content
    lod_ = lod;
    tileRange_ = tileRange;
    sizeInTiles_ = sizeInTiles;
    sizeInPixels_ = sizeInPixels;
    std::swap(tmp, pane_);
    worldExtents_ = worldExtents(lod_, tileRange_, *referenceFrame_, srs_);

    debugDump(*this, "hm-%d.png", lod_);
}

vts::NavTile::pointer HeightMap::navtile(const vts::TileId &tileId) const
{
    if (tileId.lod != lod_) {
        LOGTHROW(err2, std::runtime_error)
            << "Cannot generate navtile for " << tileId << " from data at LOD "
            << lod_ << ".";
    }

    // find place for tile data and copy if in valid range
    if (!inside(tileRange_, point(tileId))) {
        LOG(info1) << "No navtile data for tile " << tileId << ".";
        return {};
    }

    // create navtile
    auto nt(std::make_shared<vts::opencv::NavTile>());

    // we know that tileId is inside tileRange, get pixel offset
    math::Point2i offset((tileId.x - tileRange_.ll(0)) * tileGrid_.width
                         , (tileId.y - tileRange_.ll(1)) * tileGrid_.height);

    // get tile data
    cv::Mat tile(pane_, cv::Range(offset(1), offset(1) + tileSize_.height)
                 , cv::Range(offset(0), offset(0) + tileSize_.width));

    // creat mask

    // copy data from pane
    nt->data(tile);

    // optimistic approach: start with full mask, unset invalid nodes
    auto &cm(nt->coverageMask());
    for (auto j(0); j < tile.rows; ++j) {
        for (auto i(0); i < tile.cols; ++i) {
            if (tile.at<NTDataType>(j, i) == def::InvalidHeight) {
                cm.set(i, j, false);
            }
        }
    }

    // done
    return nt;
}

void HeightMap::dump(const boost::filesystem::path &filename) const
{
    NTDataType min(def::Infinity);
    NTDataType max(-def::Infinity);

    for (auto j(0); j < pane_.rows; ++j) {
        for (auto i(0); i < pane_.cols; ++i) {
            auto value(pane_.at<NTDataType>(j, i));
            if (value == def::InvalidHeight) { continue; }
            min = std::min(min, value);
            max = std::max(max, value);
        }
    }

    cv::Mat image(pane_.rows, pane_.cols, CV_8UC3, cv::Scalar(255, 0, 0));

    auto size(max - min);
    if (size >= 1e-6) {
        for (auto j(0); j < pane_.rows; ++j) {
            for (auto i(0); i < pane_.cols; ++i) {
                auto value(pane_.at<NTDataType>(j, i));
                if (value == def::InvalidHeight) { continue; }
                auto v(std::uint8_t(std::round((255 * (value - min)) / size)));
                image.at<cv::Vec3b>(j, i) = cv::Vec3b(v, v, v);
            }
        }
    }

    create_directories(filename.parent_path());
    imwrite(filename.string(), image);
}

HeightMap::BestPosition HeightMap::bestPosition() const
{
    // TODO: what to do if centroid is not at valid pixel?

    BestPosition out;
    auto &c(out.location);

    math::Extents2i validExtents(math::InvalidExtents{});

    long count(0);
    for (int j(0); j < pane_.rows; ++j) {
        for (int i(0); i < pane_.cols; ++i) {
            if (pane_.at<NTDataType>(j, i) == def::InvalidHeight) { continue; }

            c(0) += i;
            c(1) += j;
            ++count;
            update(validExtents, math::Point2i(i, j));
        }
    }

    if (count) {
        c(0) /= count;
        c(1) /= count;
    } else {
        auto cc(math::center(validExtents));
        c(0) = cc(0);
        c(1) = cc(1);
    }

    // sample height
    c(2) = pane_.at<NTDataType>(c(1), c(0));

    auto es(math::size(worldExtents_));
    auto eul(ul(worldExtents_));

    auto worldX([&](double x) {
            return eul(0) + ((x * es.width) / sizeInPixels_.width);
        });
    auto worldY([&](double y) {
            return eul(1) - ((y * es.height) / sizeInPixels_.height);
        });

    auto world([&](const math::Point3 &p) {
            return math::Point3(worldX(p(0)), worldY(p(1)), p(2));
        });

    {
        math::Point2 d1(c - validExtents.ll);
        math::Point2 d2(validExtents.ur - c);
        double distance(std::max({ d1(0), d1(1), d2(0), d2(1) }));

        // vertical extent points
        math::Point3 e1(c(0), c(1) - distance, c(2));
        math::Point3 e2(c(0), c(1) + distance, c(2));

        e1 = world(e1);
        e2 = world(e2);

        distance = boost::numeric::ublas::norm_2(e2 - e1);

        out.verticalExtent = distance;
    }

    c = world(c);

    // done
    return out;
}

namespace {

math::Extents2 addHalfPixel(const math::Extents2 &e
                            , const math::Size2 gs)
{
    auto es(size(e));
    const math::Size2f pixelSize(es.width / gs.width, es.height / gs.height);
    const math::Point2 halfPixel(pixelSize.width / 2.0
                                 , pixelSize.height / 2.0);
    return math::Extents2(e.ll - halfPixel, e.ur + halfPixel);
}

} // namespace

void HeightMap::warp(const vr::ReferenceFrame &referenceFrame
                     , vts::Lod lod, const vts::TileRange &tileRange)
{
    if (empty()) { return; }

    // TODO: work with native type inside GeoDataset when GeoDataset interface
    // is ready

    math::Size2 sizeInTiles(calculateSizeInTiles(tileRange));
    math::Size2 sizeInPixels(calculateSizeInPixels(tileGrid_, sizeInTiles));

    std::string srs;
    auto extents(worldExtents(lod, tileRange, referenceFrame, srs));

    auto srcSrs(vr::Registry::srs(srs_));
    auto dstSrs(vr::Registry::srs(srs));

    // create source dataset (extents are inflate by half pixel in each
    // direction to facilitate grid registry)
    auto srcDs(geo::GeoDataset::create
               ("", srcSrs.srsDef
                , addHalfPixel(worldExtents_, sizeInPixels_)
                , sizeInPixels_
                , geo::GeoDataset::Format::dsm
                (geo::GeoDataset::Format::Storage::memory)
                , def::InvalidHeight));

    // prepare mask
    {
        auto &cm(srcDs.mask());
        // optimistic approach: start with full mask, unset invalid nodes
        cm.reset(true);
        for (auto j(0); j < pane_.rows; ++j) {
            for (auto i(0); i < pane_.cols; ++i) {
                if (pane_.at<NTDataType>(j, i) == def::InvalidHeight) {
                    cm.set(i, j, false);
                }
            }
        }
    }

    // copy data inside dataset
    pane_.convertTo(srcDs.data(), srcDs.data().type());
    srcDs.flush();

    // create destination dataset (extents are inflate by half pixel in each
    // direction to facilitate grid registry)
    auto dstDs(geo::GeoDataset::deriveInMemory
               (srcDs, dstSrs.srsDef, sizeInPixels
                , addHalfPixel(extents, sizeInPixels)));

    // warp
    srcDs.warpInto(dstDs);

    /** fix Z component
     * for each valid point XY in navtile:
     *     X'Y' = conv2d(XY, srs -> referenceFrame_.model.navigationSrs)
     *     Z' = conv3d(X'Y'Z, referenceFrame_.model.navigationSrs
     *                , referenceFrame.model.navigationSrs).Z
     */
    {
        // temporary pane
        cv::Mat tmp(sizeInPixels.height, sizeInPixels.width
                    , vts::opencv::NavTile::CvDataType
                    , cv::Scalar(def::InvalidHeight));
        // 2d convertor between srs and referenceFrame_.model.navigationSrs
        const vts::CsConvertor sds2srcNav
            (srs, referenceFrame_->model.navigationSrs);

        const vts::CsConvertor srcNav2dstNav
            (referenceFrame_->model.navigationSrs
             , referenceFrame.model.navigationSrs);

        const auto &hf(dstDs.cdata());
        dstDs.cmask().forEach([&](int x, int y, bool)
        {
            // compose 2D point in srs
            const math::Point2 sdsXY
                (dstDs.raster2geo(math::Point2(x, y), 0.0));
            // height from current heightmap
            const auto srcZ(hf.at<double>(y, x));

            // convert to source nav system
            const auto mavXY(sds2srcNav(sdsXY));

            // nox, compose 3D point in source nav system and convert to
            // destination nav system and store Z component into destination
            // heightmap
            const auto dstZ
                (srcNav2dstNav(math::Point3(mavXY(0), mavXY(1), srcZ))(2));
            tmp.at<float>(y, x) = dstZ;
        }, geo::GeoDataset::Mask::Filter::white);

        // done, swap panes
        std::swap(tmp, pane_);
    }

    // set new content
    lod_ = lod;
    tileRange_ = tileRange;
    sizeInTiles_ = sizeInTiles_;
    sizeInPixels_ = sizeInPixels_;
    srs_ = srs;
    worldExtents_ = extents;
    referenceFrame_ = &referenceFrame;
}

void HeightMap::warp(const vts::NodeInfo &nodeInfo)
{
    warp(*nodeInfo.referenceFrame, nodeInfo.node.id.lod
         , vts::TileRange(point(nodeInfo)));
    debugDump(*this, "hm-warped-%s-%s.png", srs_, nodeInfo.nodeId());
}
