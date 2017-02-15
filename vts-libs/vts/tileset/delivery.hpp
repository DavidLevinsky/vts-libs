#ifndef vtslibs_vts_tileset_delivery_hpp_included_
#define vtslibs_vts_tileset_delivery_hpp_included_

#include <memory>

#include <boost/noncopyable.hpp>

#include "../../storage/streams.hpp"
#include "../../storage/resources.hpp"
#include "../mapconfig.hpp"
#include "./properties.hpp"

namespace vtslibs { namespace vts {

class Driver;
namespace tileset { class Index; }

using storage::IStream;
using storage::File;
using storage::TileFile;
using storage::FileStat;
using storage::Resources;
using storage::NullWhenNotFound_t;
using storage::NullWhenNotFound;

class Delivery : boost::noncopyable {
public:
    typedef std::shared_ptr<Delivery> pointer;

    // named ctor
    static pointer open(const boost::filesystem::path &root);

    IStream::pointer input(File type) const;

    IStream::pointer input(File type, const NullWhenNotFound_t&) const;

    IStream::pointer input(const TileId &tileId, TileFile type
                           , FileFlavor flavor) const;

    IStream::pointer input(const TileId &tileId, TileFile type
                           , FileFlavor flavor
                           , const NullWhenNotFound_t&) const;

    FileStat stat(File type) const;

    FileStat stat(const TileId &tileId, TileFile type) const;

    IStream::pointer input(const std::string &name) const;

    IStream::pointer input(const std::string &name
                           , const NullWhenNotFound_t&) const;

    FileStat stat(const std::string &name) const;

    Resources resources() const;

    bool externallyChanged() const;

    std::time_t lastModified() const;

    MapConfig mapConfig(bool includeExtra = true) const;

    MeshTilesConfig meshTilesConfig(bool includeExtra = true) const;

private:
    struct AccessToken {};

    std::shared_ptr<Driver> driver_;
    const FullTileSetProperties properties_;
    std::shared_ptr<tileset::Index> index_;

public:
    /** Opens storage.
     */
    Delivery(AccessToken, const boost::filesystem::path &root);
};

} } // namespace vtslibs::vts

#endif // vtslibs_vts_tileset_delivery_hpp_included_
