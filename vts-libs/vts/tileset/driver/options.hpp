#ifndef vadstena_libs_vts_tileset_driver_options_hpp_included_
#define vadstena_libs_vts_tileset_driver_options_hpp_included_

#include <set>
#include <map>

#include <boost/any.hpp>
#include <boost/optional.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/uuid/uuid.hpp>

#include "../../../storage/tilar.hpp"
#include "../../options.hpp"

namespace vadstena { namespace vts { namespace driver {

/** Map configuration override.
 */
struct MapConfigOverride {
    /** Root override.
     */
    std::string root;

    MapConfigOverride(const boost::any &options);
};

using storage::Tilar;

class PlainOptions {
public:
    PlainOptions() : binaryOrder_(0), tileMask_(0) {}

    PlainOptions(std::uint8_t binaryOrder)
        : binaryOrder_(binaryOrder)
        , uuid_(generateUuid())
        , tileMask_(calculateMask(binaryOrder))
    {}

    PlainOptions(std::uint8_t binaryOrder, boost::uuids::uuid uuid)
        : binaryOrder_(binaryOrder)
        , uuid_(uuid)
        , tileMask_(calculateMask(binaryOrder))
    {}

    std::uint8_t binaryOrder() const { return binaryOrder_; }
    void binaryOrder(std::uint8_t value) {
        binaryOrder_ = value;
        tileMask_ = calculateMask(binaryOrder_);
    }

    const boost::uuids::uuid& uuid() const { return uuid_; }
    void uuid(const boost::uuids::uuid &value) { uuid_ = value; }

    long tileMask() const { return tileMask_; }

    /** Tilar options derived from the above for tiles.
     */
    Tilar::Options tilar(unsigned int filesPerTile) const;

    struct Index {
        TileId archive;
        Tilar::FileIndex file;
    };

    /** Converts tileId into index of tilar file in the super grid and a file
     * index inside this archive.
     */
    Index index(const TileId &tileId, int type) const;

    /** Tries to relocate resources. Returns valid result in case of relocation.
     */
    boost::any relocate(const RelocateOptions &options) const;

private:
    /** Binary order of magnitude of data stored in the individial tile
     *  archives (each archive has square grid of
     *  (2^binaryOrder_)*(2^binaryOrder_) tiles.
     *
     * This information maps directly to LOD-shift (tile space of tiles at
     * any LOD are stored in space of "super" tiles at (LOD - binaryOrder_)).
     */
    std::uint8_t binaryOrder_;

    /** UUID of storage. Generated automatically on creation. Passed to
     *  tilar file create/check.
     */
    boost::uuids::uuid uuid_;

    /** Tile mask applied to tile index to get index inside archive.
     */
    long tileMask_;

    static long calculateMask(std::uint8_t order);
    static boost::uuids::uuid generateUuid();
};

struct AggregatedOptions {
    boost::filesystem::path storagePath;
    TilesetIdSet tilesets;

    boost::any relocate(const RelocateOptions &options) const;
};

struct RemoteOptions {
    std::string url;

    boost::any relocate(const RelocateOptions &options) const;
};

struct LocalOptions {
    boost::filesystem::path path;

    boost::any relocate(const RelocateOptions &options) const;
};

// inlines

inline Tilar::Options PlainOptions::tilar(unsigned int filesPerTile)
    const
{
    return { binaryOrder_, filesPerTile, uuid_ };
}

inline PlainOptions::Index
PlainOptions::index(const TileId &i, int type) const
{
    return {
        TileId(i.lod, i.x >> binaryOrder_, i.y >> binaryOrder_)
       , Tilar::FileIndex(i.x & tileMask_, i.y & tileMask_, type)
    };
}

} } } // namespace vadstena::vts::driver

#endif // vadstena_libs_vts_tileset_driver_options_hpp_included_
