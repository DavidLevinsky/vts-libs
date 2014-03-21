#ifndef vadstena_libs_tilestorage_tileindex_hpp_included_
#define vadstena_libs_tilestorage_tileindex_hpp_included_

#include <map>

#include <boost/filesystem/path.hpp>

#include "imgproc/rastermask.hpp"

#include "./maskfwd.hpp"
#include "./basetypes.hpp"
#include "./tileop.hpp"
#include "./traverser.hpp"

#include "../entities.hpp"

namespace vadstena { namespace tilestorage {

class TileIndex {
public:
    TileIndex(long baseTileSize = 0)
        : baseTileSize_(baseTileSize), minLod_() {}

    TileIndex(const Alignment &alignment, long baseTileSize
              , Extents extents
              , LodRange lodRange
              , const TileIndex *other = nullptr);

    typedef std::vector<RasterMask> Masks;

    void load(std::istream &is);
    void load(const boost::filesystem::path &path);

    void save(std::ostream &os) const;
    void save(const boost::filesystem::path &path) const;

    bool exists(const TileId &tileId) const;

    bool exists(const Index &index) const;

    void fill(Lod lod, const TileIndex &other);

    void fill(const TileIndex &other);

    void intersect(Lod lod, const TileIndex &other);

    void intersect(const TileIndex &other);

    void subtract(Lod lod, const TileIndex &other);

    void subtract(const TileIndex &other);

    void set(const TileId &tileId, bool value = true);

    void set(const Index &index, bool value = true);

    Extents extents() const;

    TileId tileId(const Index &index) const;

    Index index(const TileId &tileId) const;

    Size2l rasterSize(Lod lod) const;

    bool empty() const;

    long baseTileSize() const { return baseTileSize_; }

    Lod minLod() const { return minLod_; }

    Lod maxLod() const;

    LodRange lodRange() const;

    TileIndex& growUp();

    TileIndex& growDown();

    TileIndex& invert();

    const Masks masks() const { return masks_; };

    friend class Traverser;

    Traverser traverser() const;

    /** Clears lod content.
     */
    void clear(Lod lod);

private:
    const RasterMask* mask(Lod lod) const;

    RasterMask* mask(Lod lod);

    long baseTileSize_;
    Point2l origin_;
    Lod minLod_;
    Masks masks_;
};

/** Dump tile indes as set of images.
 */
void dumpAsImages(const boost::filesystem::path &path, const TileIndex &ti);

class Bootstrap {
public:
    Bootstrap() : lodRange_(0, -1), extents_() {}
    Bootstrap(const LodRange &lodRange) : lodRange_(lodRange), extents_() {}
    Bootstrap(const TileIndex &ti)
        : lodRange_(ti.lodRange()), extents_(ti.extents())
    {}

    const LodRange& lodRange() const { return lodRange_; }
    Bootstrap& lodRange(const LodRange &lodRange) {
        lodRange_ = lodRange;
        return *this;
    }

    const Extents& extents() const { return extents_; }
    Bootstrap& extents(const Extents &extents) {
        extents_ = extents;
        return *this;
    }

private:
    LodRange lodRange_;
    Extents extents_;
};

TileIndex unite(const Alignment &alignment
                , const std::vector<const TileIndex*> &tis
                , const Bootstrap &bootstrap = Bootstrap());

TileIndex unite(const Alignment &alignment
                , const TileIndex &l, const TileIndex &r
                , const Bootstrap &bootstrap = Bootstrap());

TileIndex intersect(const Alignment &alignment
                    , const TileIndex &l, const TileIndex &r
                    , const Bootstrap &bootstrap = Bootstrap());

TileIndex difference(const Alignment &alignment
                     , const TileIndex &l, const TileIndex &r
                     , const Bootstrap &bootstrap = Bootstrap());

// inline stuff

inline bool TileIndex::empty() const
{
    return masks_.empty();
}

inline Lod TileIndex::maxLod() const
{
    return minLod_ + masks_.size() - 1;
}

inline LodRange TileIndex::lodRange() const
{
    return { minLod_, maxLod() };
}

inline const RasterMask* TileIndex::mask(Lod lod) const
{
    auto idx(lod - minLod_);
    if ((idx < 0) || (idx >= int(masks_.size()))) {
        return nullptr;
    }

    // get mask
    return &masks_[idx];
}

inline RasterMask* TileIndex::mask(Lod lod)
{
    auto idx(lod - minLod_);
    if ((idx < 0) || (idx > int(masks_.size()))) {
        return nullptr;
    }

    // get mask
    return &masks_[idx];
}

inline Extents TileIndex::extents() const
{
    if (masks_.empty()) { return { origin_, origin_ }; }

    const auto size(masks_.front().size());
    const auto ts(tileSize(baseTileSize_, minLod_));

    return { origin_(0), origin_(1)
            , origin_(0) + ts * size.width
            , origin_(1) + ts * size.height };
}

inline Size2l TileIndex::rasterSize(Lod lod) const
{
    const auto *m(mask(lod));
    if (!m) { return {}; }
    auto d(m->dims());
    return Size2l(d.width, d.height);
}

inline bool TileIndex::exists(const Index &index) const
{
    const auto *m(mask(index.lod));
    if (!m) { return false; }
    return m->get(index.easting, index.northing);
}

inline bool TileIndex::exists(const TileId &tileId) const
{
    return exists(index(tileId));
}

inline TileId TileIndex::tileId(const Index &index) const
{
    const auto ts(tileSize(baseTileSize_, index.lod));
    return { index.lod, origin_(0) + index.easting * ts
            , origin_(1) + index.northing * ts };
}

inline Index TileIndex::index(const TileId &tileId) const
{
    const auto ts(tileSize(baseTileSize_, tileId.lod));
    return { tileId.lod, (tileId.easting - origin_(0)) / ts
            , (tileId.northing - origin_(1)) / ts };
}

inline void TileIndex::set(const TileId &tileId, bool value)
{
    return set(index(tileId), value);
}

inline void TileIndex::set(const Index &index, bool value)
{
    if (auto *m = mask(index.lod)) {
        m->set(index.easting, index.northing, value);
    }
}

inline Traverser TileIndex::traverser() const
{
    return Traverser(this);
}

} } // namespace vadstena::tilestorage

#endif // vadstena_libs_tilestorage_tileindex_hpp_included_
