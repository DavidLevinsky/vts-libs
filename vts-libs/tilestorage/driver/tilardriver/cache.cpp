#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/crc.hpp>

#include "dbglog/dbglog.hpp"

#include "./cache.hpp"
#include "../../io.hpp"

namespace vadstena { namespace tilestorage { namespace tilardriver {

namespace {

Tilar::ContentTypes tileContentTypes({ "", "image/jpeg" });
Tilar::ContentTypes metatileContentTypes;

std::uint32_t calculateHash(const std::string &data)
{
    boost::crc_32_type crc;
    crc.process_bytes(data.data(), data.size());
    return crc.checksum();
}

fs::path dir(const fs::path &filename)
{
    const auto hash(calculateHash(filename.string()));
    return str(boost::format("%02x") % ((hash >> 24) & 0xff));
}

int fileType(TileFile type) {
    switch (type) {
    case TileFile::meta: return 0;
    case TileFile::mesh: return 0;
    case TileFile::atlas: return 1;
    }
    throw;
}

} // namespace

Cache::Archives::Archives(const std::string &extension, int filesPerTile
                          , const Options &options
                          , const Tilar::ContentTypes &contentTypes)
    : extension(extension)
    , options(options.binaryOrder, filesPerTile, options.uuid)
    , contentTypes(contentTypes)
{}

fs::path Cache::Archives::filePath(const fs::path &root
                                   , const Index &index) const
{
    const auto filename(str(boost::format("%s-%07d-%07d.%s")
                            % index.lod % index.easting % index.northing
                            % extension));
    const auto parent(root / dir(filename));
    create_directories(parent);
    return parent / filename;
}

Cache::Cache(const fs::path &root, const Options &options
             , bool readOnly)
    : root_(root), options_(options), readOnly_(readOnly)
    , tiles_("tiles", 2, options, tileContentTypes)
    , metatiles_("metatiles", 1, options, metatileContentTypes)
{}

namespace {

Tilar tilar(const fs::path &path, const Tilar::Options &options
            , bool readOnly)
{
    if (readOnly) {
        // read-only
        return Tilar::open(path, options
                           , Tilar::OpenMode::readOnly);
    }
    return Tilar::create(path, options
                         , Tilar::CreateMode::appendOrTruncate);
}

} // namespace

Tilar& Cache::open(Archives &archives, const Index &archive)
{
    auto fmap(archives.map.find(archive));
    if (fmap != archives.map.end()) {
        return fmap->second;
    }

    const auto path(archives.filePath(root_, archive));

    auto file(tilar(path, archives.options, readOnly_));
    file.setContentTypes(archives.contentTypes);

    return archives.map.insert
        (Archives::Map::value_type(archive, std::move(file))).first->second;
}

IStream::pointer Cache::input(const TileId tileId, TileFile type)
{
    auto index(options_.index(tileId, fileType(type)));
    return open(getArchives(type), index.archive).input(index.file);
}

OStream::pointer Cache::output(const TileId tileId, TileFile type)
{
    auto index(options_.index(tileId, fileType(type)));
    return open(getArchives(type), index.archive).output(index.file);
}

void Cache::remove(const TileId tileId, TileFile type)
{
    auto index(options_.index(tileId, fileType(type)));
    try {
        return open(getArchives(type), index.archive).remove(index.file);
    } catch (const std::exception &e) {
        // ignore, this fails when the file cannot be opened
    }
}

std::size_t Cache::size(const TileId tileId, TileFile type)
{
    auto index(options_.index(tileId, fileType(type)));
    return open(getArchives(type), index.archive).size(index.file);
}

FileStat Cache::stat(const TileId tileId, TileFile type)
{
    auto index(options_.index(tileId, fileType(type)));
    return open(getArchives(type), index.archive).stat(index.file);
}

void Cache::commit()
{
    if (readOnly_) { return; }
    tiles_.commitChanges();
    metatiles_.commitChanges();
}

void Cache::rollback()
{
    if (readOnly_) { return; }
    tiles_.discardChanges();
    metatiles_.discardChanges();
}

} } } // namespace vadstena::tilestorage::tilardriver