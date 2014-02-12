#include <queue>

#include <boost/format.hpp>

#include "dbglog/dbglog.hpp"
#include "utility/binaryio.hpp"

#include "../tilestorage.hpp"
#include "./tileindex.hpp"
#include "./io.hpp"
#include "./tileop.hpp"
#include "./driver/flat.hpp"
#include "./tileset-detail.hpp"

namespace vadstena { namespace tilestorage {

namespace {

const char METATILE_IO_MAGIC[8] = {  'M', 'E', 'T', 'A', 'T', 'I', 'L', 'E' };

const unsigned METATILE_IO_VERSION = 1;

void tileSetDeleter(TileSet *tileSet)
{
    tileSet->flush();
    delete tileSet;
}

} // namespace

struct TileSet::Factory
{
    static TileSet::pointer create(const Locator &locator
                                   , const CreateProperties &properties
                                   , CreateMode mode)
    {
        auto driver(Driver::create(locator, properties, mode));
        return { new TileSet(driver), &tileSetDeleter };
    }

    static TileSet::pointer open(const Locator &locator, OpenMode mode)
    {
        auto driver(Driver::open(locator, mode));
        return { new TileSet(driver), &tileSetDeleter };
    }
};

/** This simple class allows access to the "detail" implementation of a tile
 * set.
 */
struct TileSet::Accessor
{
    static Detail& detail(TileSet &ts) { return ts.detail(); }
    static const Detail& detail(const TileSet &ts) { return ts.detail(); }
};

TileSet::pointer createTileSet(const Locator &locator
                               , const CreateProperties &properties
                               , CreateMode mode)
{
    return TileSet::Factory::create(locator, properties, mode);
}

TileSet::pointer openTileSet(const Locator &locator, OpenMode mode)
{
    return TileSet::Factory::open(locator, mode);
}

void TileSet::Detail::loadConfig()
{
    properties = driver->loadProperties();
    savedProperties = properties;
}

void TileSet::Detail::saveConfig()
{
    LOG(info1)
        << "Saving properties:\n"
        << utility::dump(properties, "    ");
    driver->saveProperties(properties);

    // done; remember saved properties and go on
    savedProperties = properties;
    propertiesChanged = false;
}

void TileSet::Detail::saveMetadata()
{
    driver->wannaWrite("save metadata");

    // create tile index (initialize with existing one)
    TileIndex ti(properties.alignment, properties.baseTileSize
                 , extents, lodRange, &tileIndex);

    // fill in new metadata
    ti.fill(metadata);

    // save
    try {
        auto f(driver->tileIndexOutput());
        ti.save(*f);
        f->close();
    } catch (const std::exception &e) {
        LOGTHROW(err2, Error)
            << "Unable to write tile index: " << e.what() << ".";
    }

    // cool, we have new tile index
    tileIndex = ti;

    // TODO: we have to load all metatiles present on the disk to proceed
    if (metadata.empty()) {
        // no tile, we should invalidate foat
        properties.foat = {};
        LOG(info2) << "Tile set <" << properties.id << ">: New foat is "
                   << properties.foat << ".";
    }

    // well, dump metatiles now
    saveMetatiles();

    // saved => no change
    metadataChanged = false;
}

void TileSet::Detail::loadTileIndex()
{
    try {
        tileIndex = { properties.baseTileSize };
        auto f(driver->tileIndexInput());
        tileIndex.load(*f);
        f->close();
    } catch (const std::exception &e) {
        LOGTHROW(err2, Error)
            << "Unable to read tile index: " << e.what() << ".";
    }

    // new extents
    lodRange = tileIndex.lodRange();
    extents = tileIndex.extents();
    LOG(info2) << "Loaded tile index: " << tileIndex;
}

MetaNode* TileSet::Detail::findMetaNode(const TileId &tileId)
    const
{
    auto fmetadata(metadata.find(tileId));
    if (fmetadata == metadata.end()) {
        // not found in memory -> load from disk
        return loadMetatile(tileId);
    }

    return &fmetadata->second;
}

void TileSet::Detail::setMetaNode(const TileId &tileId
                                  , const MetaNode& metanode)
{
    // this ensures that we have old metanode in memory
    findMetaNode(tileId);

    auto res(metadata.insert(Metadata::value_type(tileId, metanode)));
    if (!res.second) {
        res.first->second = metanode;
    }

    // grow extents
    if (!area(extents)) {
        // invalid extents, add first tile!
        extents = tileExtents(properties, tileId);
        // initial lod range
        lodRange.min = lodRange.max = tileId.lod;
    } else {
        // add tile

        // update extents
        extents = unite(extents, tileExtents(properties, tileId));

        // update lod range
        if (tileId.lod < lodRange.min) {
            lodRange.min = tileId.lod;
        } else if (tileId.lod > lodRange.max) {
            lodRange.max = tileId.lod;
        }
    }

    // layout updated -> update zboxes up the tree
    updateZbox(tileId, res.first->second);

    metadataChanged = true;
}

MetaNode&
TileSet::Detail::createVirtualMetaNode(const TileId &tileId)
{
    // this ensures that we have old metanode in memory
    auto *md(findMetaNode(tileId));
    if (!md) {
        // no node (real or virtual), create virtual node
        md = &(metadata.insert(Metadata::value_type(tileId, MetaNode()))
               .first->second);
    }

    metadataChanged = true;

    // ok
    return *md;
}

MetaNode* TileSet::Detail::loadMetatile(const TileId &tileId)
    const
{
    // TODO: implement me
    (void) tileId;
    return nullptr;

    // NB: this call loads whole metatile file where tileId lives; only nodes
    // that are not present in this->metadata are inserted there
}

void TileSet::Detail::setMetadata(const TileId &tileId
                                            , const TileMetadata& metadata)
{
    // this ensures that we have old metanode in memory
    auto metanode(findMetaNode(tileId));
    if (!metanode) {
        LOGTHROW(err2, NoSuchTile)
            << "There is no tile at " << tileId << ".";
    }

    // assign new metadata
    static_cast<TileMetadata&>(*metanode) = metadata;
}

void TileSet::Detail::check(const TileId &tileId) const
{
    auto ts(tileSize(properties, tileId.lod));

    auto aligned(fromAlignment(properties, tileId));
    if ((aligned.easting % ts) || (aligned.northing % ts)) {
        LOGTHROW(err2, NoSuchTile)
            << "Misaligned tile at " << tileId << " cannot exist.";
    }
}

void TileSet::Detail::updateZbox(const TileId &tileId
                                           , MetaNode &metanode)
{
    // process all 4 children
    for (const auto &childId : children(properties.baseTileSize, tileId)) {
        if (auto *node = findMetaNode(childId)) {
            metanode.zmin = std::min(metanode.zmin, node->zmin);
            metanode.zmax = std::max(metanode.zmax, node->zmax);
        }
    }

    // this tile is (current) foat -> no parent can ever exist (until real tile
    // is added)
    if (isFoat(tileId)) {
        // we reached foat tile => way up
        if (tileId != properties.foat) {
            properties.foat = tileId;
            properties.foatSize = tileSize(properties, tileId.lod);
            propertiesChanged = true;
            LOG(info2) << "Tile set <" << properties.id
                       << ">: New foat is " << properties.foat << ".";
        }
        return;
    }

    auto parentId(parent(properties.alignment
                         , properties.baseTileSize
                         , tileId));
    if (auto *parentNode = findMetaNode(parentId)) {
        updateZbox(parentId, *parentNode);
    } else {
        // there is no parent present in the tree; process freshly generated
        // parent
        updateZbox(parentId, createVirtualMetaNode(parentId));
    }
}

bool TileSet::Detail::isFoat(const TileId &tileId) const
{
    if (extents.ll(0) < tileId.easting) { return false; }
    if (extents.ll(1) < tileId.northing) { return false; }

    auto ts(tileSize(properties.baseTileSize, tileId.lod));

    if (extents.ur(0) > (tileId.easting + ts)) { return false; }
    if (extents.ur(1) > (tileId.northing + ts)) { return false; }

    return true;
}

void TileSet::Detail::flush()
{
    if (driver->readOnly()) { return; }
    LOG(info2) << "Tile set <" << properties.id << ">: flushing";

    // force metadata save
    if (metadataChanged) {
        saveMetadata();
    }

    // force config save
    if (propertiesChanged) {
        saveConfig();
    }
}

void TileSet::Detail::saveMetatileTree(MetatileDef::queue &subtrees
                                                 , std::ostream &f
                                                 , const MetatileDef &tile)
    const
{
    using utility::binaryio::write;

    auto bottom(tile.bottom());

    LOG(info2) << "Tile set <" << properties.id << ">: dumping "
               << tile.id << ", " << tile.end
               << ", bottom: " << bottom
               << ".";

    const auto *node(findMetaNode(tile.id));
    if (!node) {
        LOGTHROW(err2, Error)
            << "Can't find metanode for tile " << tile.id;
    }

    node->dump(f);

    std::uint8_t childFlags(0);
    std::uint8_t mask(1);
    auto childrenIds(children(properties.baseTileSize, tile.id));

    for (auto &childId : childrenIds) {
        if (findMetaNode(childId)) {
            childFlags |= mask;
        }
        mask <<= 1;
    }

    if (!bottom) {
        // children are local
        childFlags |= (childFlags << 4);
    }
    write(f, childFlags);

    // either dump 4 subnodes now or remember them in the subtrees
    mask = 1;
    for (const auto &childId : childrenIds) {
        if ((childFlags & mask)) {
            if (bottom) {
                // we are at the bottom of the metatile; remember subtree
                subtrees.emplace
                    (childId, deltaDown(properties.metaLevels, childId.lod));
            } else {
                // save subtree in this tile
                saveMetatileTree(subtrees, f
                                 , { childId, deltaDown(properties.metaLevels
                                                        , childId.lod) });
            }
        }
        mask <<= 1;
    }
}

/** Save metatile at given tile: subtree from tile'slod until end lod is
 * reached
 */
void TileSet::Detail::saveMetatile(MetatileDef::queue &subtrees
                                   , const MetatileDef &tile)
    const
{
    using utility::binaryio::write;

    auto f(driver->metatileOutput(tile.id));

    write(*f, METATILE_IO_MAGIC);
    write(*f, uint32_t(METATILE_IO_VERSION));

    saveMetatileTree(subtrees, *f, tile);
    f->close();
}

void TileSet::Detail::saveMetatiles() const
{
    // initialize subtrees with foat
    MetatileDef::queue subtrees;
    subtrees.emplace
        (properties.foat
         , deltaDown(properties.metaLevels, properties.foat.lod));

    while (!subtrees.empty()) {
        saveMetatile(subtrees, subtrees.front());
        subtrees.pop();
    }
}

void TileSet::Detail::begin()
{
    driver->wannaWrite("begin transaction");
    if (tx) {
        LOGTHROW(err2, PendingTransaction)
            << "Transaction already in progress.";

    }

    driver->begin();

    tx = true;
}

void TileSet::Detail::commit()
{
    driver->wannaWrite("commit transaction");

    if (!tx) {
        LOGTHROW(err2, PendingTransaction)
            << "There is no active transaction to commit.";

    }
    // forced flush
    flush();

    driver->commit();

    tx = false;
}

void TileSet::Detail::rollback()
{
    if (!tx) {
        LOGTHROW(err2, PendingTransaction)
            << "There is no active transaction to roll back.";

    }

    // TODO: what to do if anything throws?

    driver->rollback();

    // re-read backing store state
    loadConfig();

    // destroy tile index
    tileIndex = {};
    metadata = {};
    metadataChanged = false;

    if (properties.foatSize) {
        // load tile index only if foat is valid
        loadTileIndex();
    }

    // no pending tx
    tx = false;
}

// tileSet itself

TileSet::TileSet(const Driver::pointer &driver)
    : detail_(new Detail(driver))
{
    detail().loadConfig();
    // load tile index only if foat is valid
    if (detail().properties.foatSize) {
        detail().loadTileIndex();
    }
}

TileSet::~TileSet()
{
}

void TileSet::flush()
{
    detail().flush();
}

Tile TileSet::getTile(const TileId &tileId) const
{
    detail().check(tileId);

    auto md(detail().findMetaNode(tileId));
    if (!md) {
        LOGTHROW(err2, NoSuchTile)
            << "There is no tile at " << tileId << ".";
    }

    return { detail().driver->loadMesh(tileId)
            , detail().driver->loadAtlas(tileId), *md };
}

void TileSet::setTile(const TileId &tileId, const Mesh &mesh
                      , const Atlas &atlas, const TileMetadata *metadata)
{
    detail().driver->wannaWrite("set tile");

    detail().check(tileId);

    detail().driver->saveMesh(tileId, mesh);
    detail().driver->saveAtlas(tileId, atlas
                               , detail().properties.textureQuality);

    // create new metadata
    MetaNode metanode;

    // copy extra metadata
    if (metadata) {
        static_cast<TileMetadata&>(metanode) = *metadata;
    }

    // calculate dependent metadata
    metanode.calcParams(mesh, { atlas.cols, atlas.rows });

    // remember new metanode
    detail().setMetaNode(tileId, metanode);
}

void TileSet::setMetadata(const TileId &tileId, const TileMetadata &metadata)
{
    detail().driver->wannaWrite("set tile metadata");

    detail().check(tileId);

    detail().setMetadata(tileId, metadata);
}

bool TileSet::tileExists(const TileId &tileId) const
{
    // have look to in-memory data
    const auto &metadata(detail().metadata);
    auto fmetadata(metadata.find(tileId));
    if (fmetadata != metadata.end()) {
        return fmetadata->second.exists();
    }

    // try tileIndex
    return detail().tileIndex.exists(tileId);
}

Properties TileSet::getProperties() const
{
    return detail().properties;
}

Properties TileSet::setProperties(const SettableProperties &properties
                                  , int mask)
{
    detail().driver->wannaWrite("set properties");

    // merge in new properties
    if (detail().properties.merge(properties, mask)) {
        detail().propertiesChanged = true;
    }
    return detail().properties;
}

void TileSet::begin()
{
    detail().begin();
}

void TileSet::commit()
{
    detail().commit();
}

void TileSet::rollback()
{
    detail().rollback();
}

namespace {

LodRange range(const TileSet::list &sets)
{
    if (sets.empty()) {
        // empty set
        return { 0, -1 };
    }

    LodRange r;
    for (const auto &set : sets) {
        r = unite(r, TileSet::Accessor::detail(*set).lodRange);
    }
    return r;
}

typedef std::vector<const TileIndex*> TileIndices;

inline void tileIndicesImpl(TileIndices &indices, const TileSet::list &set)
{
    // get all tile indices
    for (const auto &ts : set) {
        auto &detail(TileSet::Accessor::detail(*ts));
        // we need fresh index
        detail.flush();
        indices.push_back(&detail.tileIndex);
    }
}

template <typename ...Args>
inline void tileIndicesImpl(TileIndices &indices, const TileSet::list &set
                            , Args &&...rest)
{
    tileIndicesImpl(indices, set);
    tileIndicesImpl(indices, std::forward<Args>(rest)...);
}

template <typename ...Args>
inline TileIndices tileIndices(Args &&...args)
{
    TileIndices indices;
    tileIndicesImpl(indices, std::forward<Args>(args)...);
    return indices;
}

inline void dump(const boost::filesystem::path &dir, const TileSet::list &set)
{
    int i(0);
    for (const auto &s : set) {
        auto &detail(TileSet::Accessor::detail(*s));
        // we need fresh index
        detail.flush();

        auto path(dir / str(boost::format("%03d") % i));
        dumpAsImages(path, detail.tileIndex);
        ++i;
    }
}

} // namespace

void TileSet::mergeIn(const list &kept, const list &update)
{
    dump("debug/update", update);

    const auto &alignment(detail().properties.alignment);

    // calculate storage update (we need to know lod range of kept sets to
    // ensure all indices cover same lod range)
    auto tsUpdate(unite(alignment, tileIndices(update), range(kept)));
    if (empty(tsUpdate.extents())) {
        LOG(warn3) << "(merge-in) Nothing to merge in. Bailing out.";
        return;
    }

    dumpAsImages("debug/tsUpdate", tsUpdate);
    tsUpdate.growDown();
    dumpAsImages("debug/tsUpdate-gd", tsUpdate);

    // calculate storage post state
    auto tsPost(unite(alignment, tileIndices(update, kept), tsUpdate));
    dumpAsImages("debug/tsPost", tsPost);
    tsPost.growUp();
    dumpAsImages("debug/tsPost-gu", tsPost);

    // calculate storage pre state
    auto tsPre(unite(alignment, tileIndices(kept), tsUpdate));
    dumpAsImages("debug/tsPre", tsPre);
    tsPre.growUp().invert();
    dumpAsImages("debug/tsPre-gu-inv", tsPre);

    LOG(info2) << "(merge-in) down(tsUpdate): " << tsUpdate;
    LOG(info2) << "(merge-in) up(tsPost): " << tsPost;
    LOG(info2) << "(merge-in) inv(up(tsPre)): " << tsPre;

    auto generate(intersect(alignment, tsPost
                            , unite(alignment, tsUpdate, tsPre)));
    dumpAsImages("debug/generate", generate);

    LOG(info2) << "(merge-in) generate: " << generate;

    if (generate.empty()) {
        LOG(warn3) << "(merge-in) Nothing to generate. Bailing out.";
        return;
    }

#if 0
    auto topLevel(generate.masks().first());
    auto size(topLevel.dims());
    for ();
#endif
}

void TileSet::mergeOut(const list &kept, const list &update)
{
    // TODO: implement when mergeIn works
    (void) update;
    (void) kept;
}

} } // namespace vadstena::tilestorage
