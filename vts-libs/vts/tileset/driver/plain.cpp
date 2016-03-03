#include <stdexcept>
#include <limits>
#include <type_traits>
#include <fstream>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/utility/in_place_factory.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "dbglog/dbglog.hpp"

#include "utility/streams.hpp"
#include "utility/path.hpp"

#include "../../../storage/error.hpp"
#include "../../../storage/fstreams.hpp"
#include "../../io.hpp"
#include "../config.hpp"
#include "./plain.hpp"

namespace vadstena { namespace vts { namespace driver {

namespace fs = boost::filesystem;

namespace {
    const std::uint8_t DefaultBinaryOrder(5);

    const std::string ConfigName("tileset.conf");
    const std::string ExtraConfigName("extra.conf");
    const std::string TileIndexName("tileset.index");

    const std::string filePath(File type)
    {
        switch (type) {
        case File::config: return ConfigName;
        case File::extraConfig: return ExtraConfigName;
        case File::tileIndex: return TileIndexName;
        default: break;
        }
        throw "unknown file type";
    }

    boost::uuids::uuid generateUuid() {
        // generate random uuid
        return boost::uuids::random_generator()();
    }
} // namespace

long PlainDriverOptions::calculateMask(std::uint8_t order)
{
    long value(0);
    for (long bit(1l); order; --order, bit <<= 1) {
        value |= bit;
    }
    return value;
}

boost::uuids::uuid PlainDriverOptions::generateUuid() {
    // generate random uuid
    return boost::uuids::random_generator()();
}

PlainDriver::PlainDriver(const boost::filesystem::path &root
                         , const PlainDriverOptions &options
                         , CreateMode mode)
    : Driver(root, PlainDriverOptions(options.binaryOrder()), mode)
    , cache_(this->root(), this->options<PlainDriverOptions>()
             , false)
{}

PlainDriver::PlainDriver(const boost::filesystem::path &root
                         , const PlainDriverOptions &options)
    : Driver(root, options)
    , cache_(this->root(), this->options<PlainDriverOptions>(), true)
{
}

PlainDriver::~PlainDriver() {}

OStream::pointer PlainDriver::output_impl(File type)
{
    const auto path(root() / filePath(type));
    LOG(info1) << "Saving to " << path << ".";
    return fileOStream(type, path);
}

IStream::pointer PlainDriver::input_impl(File type) const
{
    auto path(root() / filePath(type));
    LOG(info1) << "Loading from " << path << ".";
    return fileIStream(type, path);
}

OStream::pointer PlainDriver::output_impl(const TileId &tileId, TileFile type)
{
    return cache_.output(tileId, type);
}

IStream::pointer PlainDriver::input_impl(const TileId &tileId, TileFile type)
    const
{
    return cache_.input(tileId, type);
}

FileStat PlainDriver::stat_impl(File type) const
{
    const auto name(filePath(type));
    const auto path(root() / name);
    LOG(info1) << "Statting " << path << ".";
    return FileStat::stat(path);
}

FileStat PlainDriver::stat_impl(const TileId &tileId, TileFile type) const
{
    return cache_.stat(tileId, type);
}

storage::Resources PlainDriver::resources_impl() const
{
    return cache_.resources();
}


void PlainDriver::flush_impl()
{
    cache_.flush();
}

void PlainDriver::drop_impl()
{
    // remove whole root directory
    remove_all(root());
}

} } } // namespace vadstena::vts::driver
