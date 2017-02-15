#ifndef vtslibs_vts_tileset_driver_options_hpp_included_
#define vtslibs_vts_tileset_driver_options_hpp_included_

#include <set>
#include <map>

#include <boost/any.hpp>
#include <boost/optional.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/uuid/uuid.hpp>

#include "../../../storage/tilar.hpp"
#include "../../options.hpp"

namespace vtslibs { namespace vts { namespace driver {

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
    PlainOptions()
        : binaryOrder_(0), tileMask_(0)
        , metaUnusedBits_(0)
    {}

    PlainOptions(std::uint8_t binaryOrder
                 , std::uint8_t metaUnusedBits = 0)
        : binaryOrder_(binaryOrder)
        , uuid_(generateUuid())
        , tileMask_(calculateMask(binaryOrder))
        , metaUnusedBits_(metaUnusedBits)
    {}

    /** Copy ctor with force uuid generation option
     */
    PlainOptions(const PlainOptions &other, bool generateUuid = false)
        : binaryOrder_(other.binaryOrder_)
        , uuid_(generateUuid ? PlainOptions::generateUuid() : other.uuid_)
        , tileMask_(calculateMask(binaryOrder_))
        , metaUnusedBits_(other.metaUnusedBits_)
    {}

    std::uint8_t binaryOrder() const { return binaryOrder_; }
    void binaryOrder(std::uint8_t value) {
        binaryOrder_ = value;
        tileMask_ = calculateMask(binaryOrder_);
    }

    std::uint8_t metaUnusedBits() const { return metaUnusedBits_; }
    void metaUnusedBits(std::uint8_t value) {
        metaUnusedBits_ = value;
        tileMask_ = calculateMask(metaUnusedBits_);
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
    Index index(TileId tileId, storage::TileFile fileType, int type) const;

    /** Tries to relocate resources. Returns valid result in case of relocation.
     */
    boost::any relocate(const RelocateOptions &options
                        , const std::string &prefix) const;

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

    /** How many metatile ID least significant bits are unused.
     */
    std::uint8_t metaUnusedBits_;

    static long calculateMask(std::uint8_t order);
    static boost::uuids::uuid generateUuid();
};

/** Old, unoptimized aggregated driver.
 */
struct OldAggregatedOptions {
    boost::filesystem::path storagePath;
    TilesetIdSet tilesets;

    boost::any relocate(const RelocateOptions &options
                        , const std::string &prefix) const;
};

/** Optimized aggregated driver.
 */
struct AggregatedOptions {
    boost::filesystem::path storagePath;
    TilesetIdSet tilesets;
    std::string tsMap;
    bool surfaceReferences;

    boost::any relocate(const RelocateOptions &options
                        , const std::string &prefix) const;

    AggregatedOptions() : surfaceReferences(false) {}
};

struct RemoteOptions {
    std::string url;

    boost::any relocate(const RelocateOptions &options
                        , const std::string &prefix) const;
};

struct LocalOptions {
    boost::filesystem::path path;

    boost::any relocate(const RelocateOptions &options
                        , const std::string &prefix) const;
};

// inlines

inline Tilar::Options PlainOptions::tilar(unsigned int filesPerTile)
    const
{
    return { binaryOrder_, filesPerTile, uuid_ };
}

inline PlainOptions::Index
PlainOptions::index(TileId i, storage::TileFile fileType, int type) const
{
    if (fileType == storage::TileFile::meta) {
        // shrink metatile space
        i.x >>= metaUnusedBits_;
        i.y >>= metaUnusedBits_;
    }

    return {
        TileId(i.lod, i.x >> binaryOrder_, i.y >> binaryOrder_)
       , Tilar::FileIndex(i.x & tileMask_, i.y & tileMask_, type)
    };
}

} } } // namespace vtslibs::vts::driver

#endif // vtslibs_vts_tileset_driver_options_hpp_included_
