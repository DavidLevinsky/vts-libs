/**
 * \file vts/storage/storage.cpp
 * \author Vaclav Blazek <vaclav.blazek@citationtech.net>
 *
 * Tile set storage access.
 */

#include <memory>
#include <string>
#include <exception>
#include <algorithm>
#include <iterator>

#include <boost/optional.hpp>
#include <boost/noncopyable.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/range/adaptor/reversed.hpp>

#include "utility/streams.hpp"
#include "utility/guarded-call.hpp"

#include "../../storage/error.hpp"
#include "../storage.hpp"
#include "../../vts.hpp"
#include "./detail.hpp"
#include "./config.hpp"

namespace fs = boost::filesystem;

namespace vadstena { namespace vts {

namespace {
    const fs::path ConfigFilename("storage.conf");
    const fs::path TileSetDir("tilesets");
    const fs::path GlueDir("glues");
    const fs::path TempDir("tmp");
}

Storage::Storage(const boost::filesystem::path &path, OpenMode mode)
    : detail_(new Detail(path, mode))
{
}

Storage::Storage(const boost::filesystem::path &path
                 , const StorageProperties &properties
                 , CreateMode mode)
    : detail_(new Detail(path, properties, mode))
{}

Storage::~Storage()
{
    // no-op
}

void Storage::add(const boost::filesystem::path &tilesetPath
                  , const Location &where
                  , const boost::optional<std::string> tilesetId)
{
    auto ts(openTileSet(tilesetPath));
    detail().add(ts, where
                 , tilesetId ? *tilesetId : ts.getProperties().id);
}

void Storage::remove(const TilesetIdList &tilesetIds)
{
    detail().remove(tilesetIds);
}

Storage::Detail::~Detail()
{
}

Storage::Detail::Detail(const boost::filesystem::path &root
                        , const StorageProperties &properties
                        , CreateMode mode)
    : root(root)
{
    // fill in slice
    static_cast<StorageProperties&>(this->properties) = properties;

    if (!create_directories(root)) {
        // directory already exists -> fail if mode says so
        if (mode == CreateMode::failIfExists) {
            LOGTHROW(err2, vadstena::storage::StorageAlreadyExists)
                << "Storage " << root << " already exists.";
        }

        // OK, we can overwrite; cache contents of old config (if any)
        try {
            auto old(storage::loadConfig(root / ConfigFilename));
            this->properties.revision = old.revision + 1;
        } catch (...) {}
    }

    saveConfig();
}

Storage::Detail::Detail(const boost::filesystem::path &root
                        , OpenMode mode)
    : root(root)
{
    (void) mode;

    loadConfig();
}

void Storage::Detail::loadConfig()
{
    try {
        // load config
        const auto p(storage::loadConfig(root / ConfigFilename));

        // set
        properties = p;
    } catch (const std::exception &e) {
        LOGTHROW(err2, vadstena::storage::Error)
            << "Unable to read config: <" << e.what() << ">.";
    }
}

void Storage::Detail::saveConfig()
{
    // save json
    try {
        storage::saveConfig(root / ConfigFilename, properties);
    } catch (const std::exception &e) {
        LOGTHROW(err2, vadstena::storage::Error)
            << "Unable to write config: <" << e.what() << ">.";
    }
}

namespace {

void rmrf(const fs::path &path)
{
    boost::system::error_code ec;
    remove_all(path, ec);
}

fs::path tilesetPath(const fs::path &root, const std::string &tilesetId
                     , bool tmp = false)
{
    if (tmp) {
        return root / TempDir / tilesetId;
    }

    return root / TileSetDir / tilesetId;
}

fs::path gluePath(const fs::path &root, const Glue &glue, bool tmp = false)
{
    if (tmp) {
        return root / TempDir / glue.path;
    }

    return root / GlueDir / glue.path;
}

class Tx {
public:
    Tx(const fs::path &root) : root_(root) {}
    ~Tx();

    void add(const fs::path &work, const fs::path &dst);

    const fs::path root() const { return root_; }

    fs::path tilesetPath(const std::string &tilesetId, bool tmp = false)
        const
    {
        return vts::tilesetPath(root_, tilesetId, tmp);
    }

    fs::path gluePath(const Glue &glue, bool tmp = false)
        const
    {
        return vts::gluePath(root_, glue, tmp);
    }

    TileSet open(const std::string &tilesetId) const;

private:
    void rollback();
    void commit();

    const fs::path root_;
    typedef std::map<fs::path, fs::path> Mapping;
    Mapping mapping_;
};

Tx::~Tx() {
    if (std::uncaught_exception()) {
        // we cannot throw!
        rollback();
    } else {
        commit();
    }
}

void Tx::add(const fs::path &work, const fs::path &dst)
{
    mapping_.insert(Mapping::value_type(work, dst));
}

void Tx::rollback()
{
    for (const auto &item : mapping_) {
        try { rmrf(item.first); } catch (...) {}
    }
}

void Tx::commit()
{
    for (const auto &item : mapping_) {
        // TODO: make more robust
        // remove old stuff
        rmrf(item.second);
        // move new stuff there
        rename(item.first, item.second);
    }
}

TileSet Tx::open(const std::string &tilesetId) const
{
    return openTileSet(tilesetPath(tilesetId));
}

} // namespace

TilesetIdList::iterator
Storage::Properties::findTileset(const std::string& tileset)
{
    return std::find(tilesets.begin(), tilesets.end(), tileset);
}

TilesetIdList::const_iterator
Storage::Properties::findTileset(const std::string& tileset) const
{
    return std::find(tilesets.begin(), tilesets.end(), tileset);
}

Glue::map::iterator Storage::Properties::findGlue(const Glue::Id &glue)
{
    return glues.find(glue);
}

Glue::map::const_iterator
Storage::Properties::findGlue(const Glue::Id& glue) const
{
    return glues.find(glue);
}

TilesetIdList Storage::tilesets() const
{
    return detail().properties.tilesets;
}

Glue::map Storage::glues() const
{
    return detail().properties.glues;
}


namespace {

typedef std::vector<TileSet> TileSets;
typedef std::vector<TileIndex> TileIndices;

std::tuple<TileSets, std::size_t>
openTilesets(Tx &tx, const TilesetIdList &ids, TileSet &tileset)
{
    std::tuple<TileSets, std::size_t> res;
    TileSets &tilesets(std::get<0>(res));
    std::size_t index(0);
    for (const auto &id : ids) {
        if (id == tileset.id()) {
            tilesets.push_back(tileset);
            std::get<1>(res) = index;
            LOG(info4) << "Reused <" << tilesets.back().id() << ">.";
        } else {
            tilesets.push_back(tx.open(id));
            LOG(info4) << "Opened <" << tilesets.back().id() << ">.";
        }

        ++index;
    }
    return res;
}

LodRange range(const TileSets &tilesets)
{
    auto lr(LodRange::emptyRange());
    for (const auto &ts : tilesets) {
        lr = unite(lr, ts.lodRange());
    }
    return lr;
}

Storage::Properties
createGlues(Tx &tx, Storage::Properties properties
            , const std::tuple<TileSets, std::size_t> &tsets)
{
    if (properties.tilesets.size() <= 1) {
        LOG(info3) << "No need to create any glue.";
        return properties;
    }

    const auto &tilesets(std::get<0>(tsets));
    const auto addedIndex(std::get<1>(tsets));

    // accumulate lod range for all tilesets
    auto lr(range(tilesets));

    LOG(info4) << lr;

    // here, we are interested inly in tiles with mesh
    auto meshFilter([](QTree::value_type value)
    {
        return (value & TileIndex::TileFlag::mesh);
    });

    const auto &added(tilesets[addedIndex]);

    TileIndices grown;
    for (const auto &ts : tilesets) {
        grown.push_back(ts.tileIndex().grow(lr, meshFilter));
        LOG(info4) << "<" << ts.id() << "> grown up and down.";
    }

    const auto &addedGrown(grown[addedIndex]);
    for (const auto &ti : grown) {
        // ignore added tileset
        if (&ti == &addedGrown) { continue; }
        if (ti.notoverlaps(addedGrown, meshFilter)) {
            continue;
        }
        LOG(info4) << "<" << added.id() << "> shares the same tile space!";
    }

    (void) tx;

    return properties;
}

} // namespace

Storage::Properties Storage::Detail::addTileset(const Properties &properties
                                                , const std::string tilesetId
                                                , const Location &where) const
{
    if (properties.hasTileset(tilesetId)) {
        LOGTHROW(err1, vadstena::storage::TileSetAlreadyExists)
            << "Tileset <" << tilesetId << "> already present in storage "
            << root << ".";
    }

    auto p(properties);
    auto &tilesets(p.tilesets);

    if (where.where.empty()) {
        // void reference
        if (where.direction == Location::Direction::below) {
            // below void -> to the top of the stack
            tilesets.push_back(tilesetId);
        } else {
            // above void -> to the bottom of the stack
            tilesets.insert(tilesets.begin(), tilesetId);
        }
        return p;
    }

    // some reference
    auto ftilesets(p.findTileset(where.where));
    if (ftilesets == tilesets.end()) {
        LOGTHROW(err1, vadstena::storage::NoSuchTileSet)
            << "Tileset <" << tilesetId << "> (used as a reference) "
            "not found in storage " << root << ".";
    }

    if (where.direction == Location::Direction::below) {
        // below given reference -> just insert here
        tilesets.insert(ftilesets, tilesetId);
    } else {
        // above reference -> advance and insert
        tilesets.insert(std::next(ftilesets), tilesetId);
    }

    return p;
}

std::tuple<Storage::Properties, Glue::map>
Storage::Detail::removeTilesets(const Properties &properties
                                , const TilesetIdList &tilesetIds)
    const
{
    std::tuple<Properties, Glue::map> res(properties, {});
    auto &p(std::get<0>(res));
    auto &tilesets(p.tilesets);

    for (const auto &tilesetId : tilesetIds) {
        auto ftilesets(p.findTileset(tilesetId));
        if (ftilesets == tilesets.end()) {
            LOG(warn1) << "Tileset <" << tilesetId << "> "
                "not found in storage " << root << ".";
            continue;
        }

        // remove from tilesets
        tilesets.erase(ftilesets);
    }

    // TODO: drop all glues that reference this tileset

    for (const auto &tilesetId : tilesetIds) {
        (void) tilesetId;
        auto &resGlues(std::get<1>(res));
        (void) resGlues;
    }

    return res;
}

void Storage::Detail::add(const TileSet &tileset
                          , const Location &where
                          , const std::string tilesetId)
{
    dbglog::thread_id(str(boost::format("%s->%s/%s")
                          % tileset.id()
                          % root.filename().string()
                          % tilesetId));

    // prepare new tileset list
    auto nProperties(addTileset(properties, tilesetId, where));

    LOG(info3) << "Adding tileset <" << tileset.id() << "> (from "
               << tileset.root() << ").";

    {
        Tx tx(root);
        const auto workPath(tilesetPath(root, tilesetId, true));

        tx.add(workPath, tilesetPath(root, tilesetId));

        // create tileset at work path (overwrite any existing stuff here)
        auto dst(cloneTileSet(workPath, tileset, CreateMode::overwrite
                              , tilesetId));

        // create glues
        auto tilesets(openTilesets(tx, nProperties.tilesets, dst));

        nProperties = createGlues(tx, nProperties, tilesets);
    }

    // FIXME: remove
    if (::getenv("ABORT")) {
        abort();
    }

    // commit properties
    properties = nProperties;
    saveConfig();
}

void Storage::Detail::remove(const TilesetIdList &tilesetIds)
{
    // dbglog::thread_id(str(boost::format("%s/%s->void")
    //                       % root.filename().string()
    //                       % tilesetId));

    Properties nProperties;
    Glue::map glues;
    std::tie(nProperties, glues) = removeTilesets(properties, tilesetIds);

    LOG(info3)
        << "Removing tilesets <" << utility::join(tilesetIds, ", ") << ">.";

    properties = nProperties;
    saveConfig();

    // physical removal
    for (const auto &tilesetId : tilesetIds) {
        auto path(tilesetPath(root, tilesetId));
        LOG(info3) << "Removing " << path << ".";
        rmrf(path);
    }

    for (const auto &item : glues) {
        const auto &glue(item.second);
        auto path(gluePath(root, glue));
        LOG(info3) << "Removing glue " << glue.path << ".";
        rmrf(path);
    }
}

} } // namespace vadstena::vts
