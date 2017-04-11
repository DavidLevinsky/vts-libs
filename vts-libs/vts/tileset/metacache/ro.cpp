#include <atomic>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>

#include "utility/time.hpp"

#include "../../../storage/error.hpp"

#include "../metacache.hpp"

namespace vtslibs { namespace vts {

namespace limit {
std::size_t ReadOnlyMetatileLimit(1000);
std::atomic<std::size_t> roMetatiles(0);
} // namespace limit

namespace detail {

namespace {

typedef decltype(utility::usecFromEpoch()) Time;
const Time MaxTime(std::numeric_limits<Time>::max());

struct Record {
    Record(const MetaTile::pointer &metatile)
        : metaId(metatile->origin()), lastHit(utility::usecFromEpoch())
        , metatile(metatile)
    {}

    void hit() { lastHit = utility::usecFromEpoch(); }

    TileId metaId;
    Time lastHit;
    MetaTile::pointer metatile;
};

struct MetaIdx {};
struct LastHitIdx {};

typedef boost::multi_index_container<
    Record
    , boost::multi_index::indexed_by<
          boost::multi_index::ordered_unique
          <boost::multi_index::tag<MetaIdx>
           , BOOST_MULTI_INDEX_MEMBER
           (Record, decltype(Record::metaId), metaId)>
          , boost::multi_index::ordered_non_unique
          <boost::multi_index::tag<LastHitIdx>
           , BOOST_MULTI_INDEX_MEMBER
           (Record, decltype(Record::lastHit), lastHit)>
          >
    > Map;

} // namespace

class RoMetaCache : public MetaCache {
public:
    RoMetaCache(const Driver::pointer &driver)
        : MetaCache(driver)
    {
        LOG(info1) << "RoMetaCache(" << driver->info() << ")";
    }

    MetaTile::pointer add(const MetaTile::pointer &metatile) {
        Record record(metatile);
        auto res(map_.insert(record));
        if (!res.second) {
            map_.replace(res.first, record);
        } else {
            ++limit::roMetatiles;
        }

        // housekeeping, we want to keep found record
        houseKeeping(&record.metaId);
        return metatile;
    }

    MetaTile::pointer find(const TileId &metaId) {
        auto fmap(map_.find(metaId));
        if (fmap != map_.end()) {
            hit(map_, fmap);
            return fmap->metatile;
        }
        return {};
    }

    void clear() {
        limit::roMetatiles -= map_.size();
        map_.clear();
    }

    void save() {
        LOGTHROW(err1, storage::ReadOnlyError)
            << "Cannot save metatile from read-only cache.";
    }

private:
    void houseKeeping(const TileId *keep = nullptr);

    template <typename Idx, typename Iterator>
    inline void hit(Idx &idx, Iterator iterator) {
        idx.modify(iterator, [](Record &r) { r.hit(); });
    }

    Map map_;
};

void RoMetaCache::houseKeeping(const TileId *keep)
{
    if (limit::roMetatiles <= limit::ReadOnlyMetatileLimit) {
        return;
    }

    auto &idx(map_.get<LastHitIdx>());

    int toDrop(5);
    for (auto iidx(idx.begin()); toDrop && (iidx != idx.end()); ) {
        // skip keep tile
        if (keep && (iidx->metaId == *keep)) {
            LOG(debug) << "Metatile " << iidx->metaId
                       << " must be kept in the cache.";
            ++iidx;
            continue;
        }

        // no changes -> we are free to remove the file
        LOG(debug) << "Removing metatile "
                   << iidx->metaId << '/' << iidx->lastHit
                   << " from the cache.";

        iidx = idx.erase(iidx);
        --toDrop;
        --limit::roMetatiles;
        break;
    }
}

/** Scarce memory implementation.
 *  Read-only version, holds only one metatile per lod.
 */
class RoMetaCacheSM : public MetaCache {
public:
    RoMetaCacheSM(const Driver::pointer &driver)
        : MetaCache(driver)
    {
        LOG(info1) << "RoMetaCacheSM(" << driver->info() << ")";
    }

private:
    virtual MetaTile::pointer add(const MetaTile::pointer &metatile);
    virtual MetaTile::pointer find(const TileId &metaId);
    virtual void clear();
    virtual void save();

    typedef std::vector<MetaTile::pointer> MetaTileCache;
    MetaTileCache cache_;
};

MetaTile::pointer RoMetaCacheSM::add(const MetaTile::pointer &metatile)
{
    const auto &metaId(metatile->origin());
    if (metaId.lod >= cache_.size()) {
        // make room
        cache_.resize(metaId.lod + 1);
    }

    return cache_[metaId.lod] = metatile;
}

MetaTile::pointer RoMetaCacheSM::find(const TileId &metaId)
{
    if (metaId.lod >= cache_.size()) { return {};}
    const auto &mt(cache_[metaId.lod]);
    if (mt && (mt->origin() == metaId)) { return mt; }
    return {};
}

void RoMetaCacheSM::clear()
{
    cache_.clear();
}

void RoMetaCacheSM::save()
{
    LOGTHROW(err1, storage::ReadOnlyError)
        << "Cannot save metatile from read-only cache.";
}

} // namespace detail

MetaCache::~MetaCache() {}

std::unique_ptr<MetaCache> MetaCache::create(const Driver::pointer &driver)
{
    if (driver->readOnly()) {
        if (driver->openOptions().scarceMemory()) {
            return roScarceMemory(driver);
        }
        // we are using rw cache implementation due to performance reasons
        return rw(driver);
    }
    return rw(driver);
}

std::unique_ptr<MetaCache> MetaCache::ro(const Driver::pointer &driver)
{
    return std::unique_ptr<MetaCache>
        (new detail::RoMetaCache(driver));
}

std::unique_ptr<MetaCache>
MetaCache::roScarceMemory(const Driver::pointer &driver)
{
    return std::unique_ptr<MetaCache>
        (new detail::RoMetaCacheSM(driver));
}

} } // namespace vtslibs::vts
