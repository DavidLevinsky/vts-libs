#ifndef vadstena_libs_vts_tileset_driver_aggregated_hpp_included_
#define vadstena_libs_vts_tileset_driver_aggregated_hpp_included_

#include <set>
#include <map>
#include <memory>

#include "../driver.hpp"

namespace vadstena { namespace vts { namespace driver {

/** Helper class.
 */
struct AggregatedDriverBase {
    AggregatedDriverBase() {}
    AggregatedDriverBase(const CloneOptions &cloneOptions);
};

class AggregatedDriver : private AggregatedDriverBase, public Driver {
public:
    typedef std::shared_ptr<AggregatedDriver> pointer;

    /** Creates new storage. Existing storage is overwritten only if mode ==
     *  CreateMode::overwrite.
     */
    AggregatedDriver(const boost::filesystem::path &root
                     , const AggregatedOptions &options
                     , const CloneOptions &cloneOptions);

    /** Opens storage.
     */
    AggregatedDriver(const boost::filesystem::path &root
                     , const AggregatedOptions &options);

    /** Cloner
     */
    AggregatedDriver(const boost::filesystem::path &root
                     , const AggregatedOptions &options
                     , const CloneOptions &cloneOptions
                     , const AggregatedDriver &src);

    virtual ~AggregatedDriver();

    /** Base class for tileset and glue info.
     */
    struct EnhancedInfo {
        Driver::pointer driver;
        tileset::Index tsi;
        std::string name;
    };

    struct TileSetInfo : EnhancedInfo {
        struct GlueInfo : TileSetGlues::EnhancedGlue, EnhancedInfo {
            GlueInfo(const TileSetGlues::EnhancedGlue &glue)
                : EnhancedGlue(glue) {}

            typedef std::vector<GlueInfo> list;
        };

        TilesetId tilesetId;

        GlueInfo::list glues;

        TileSetInfo(const TileSetGlues &tsg)
            : tilesetId(tsg.tilesetId)
            , glues{tsg.glues.begin(), tsg.glues.end()}
        {}

        typedef std::vector<TileSetInfo> list;
    };

private:
    virtual OStream::pointer output_impl(const File type);

    virtual IStream::pointer input_impl(File type) const;

    virtual OStream::pointer
    output_impl(const TileId &tileId, TileFile type);

    virtual IStream::pointer
    input_impl(const TileId &tileId, TileFile type) const;

    virtual void drop_impl();

    virtual void flush_impl();

    virtual FileStat stat_impl(File type) const;

    virtual FileStat stat_impl(const TileId &tileId, TileFile type) const;

    virtual Resources resources_impl() const;

    Driver::pointer clone_impl(const boost::filesystem::path &root
                               , const CloneOptions &cloneOptions) const;

    virtual std::string info_impl() const;

    inline const AggregatedOptions& options() const {
        return Driver::options<const AggregatedOptions&>();
    }

    TileSetInfo::list buildTilesetInfo() const;

    Storage storage_;

    registry::ReferenceFrame referenceFrame_;

    tileset::Index tsi_;

    /** Stuff ripe for delivery.
     */
    TileSetInfo::list tilesetInfo_;
};

} } } // namespace vadstena::vts::driver

#endif // vadstena_libs_vts_tileset_driver_aggregated_hpp_included_
