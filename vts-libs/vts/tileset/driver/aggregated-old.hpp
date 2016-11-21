#ifndef vadstena_libs_vts_tileset_driver_aggregated_old_hpp_included_
#define vadstena_libs_vts_tileset_driver_aggregated_old_hpp_included_

#include <set>
#include <map>
#include <memory>

#include "../driver.hpp"
#include "../../storage.hpp"

namespace vadstena { namespace vts { namespace driver {

/** Helper class.
 */
struct OldAggregatedDriverBase {
    OldAggregatedDriverBase() {}
    OldAggregatedDriverBase(const CloneOptions &cloneOptions);
};

class OldAggregatedDriver : private OldAggregatedDriverBase, public Driver {
public:
    typedef std::shared_ptr<OldAggregatedDriver> pointer;

    /** Opens storage.
     */
    OldAggregatedDriver(const boost::filesystem::path &root
                        , const OpenOptions &openOptions
                        , const OldAggregatedOptions &options);

    /** Cloner
     */
    OldAggregatedDriver(const boost::filesystem::path &root
                     , const OldAggregatedOptions &options
                     , const CloneOptions &cloneOptions
                     , const OldAggregatedDriver &src);

    virtual ~OldAggregatedDriver();

    /** Base class for tileset and glue info.
     */
    struct EnhancedInfo {
        Driver::pointer driver;
        tileset::Index::pointer tsi;
        std::string name;
        bool hasAlienTiles;
        bool isAlien;

        EnhancedInfo(const registry::ReferenceFrame &referenceFrame)
            : tsi(std::make_shared<tileset::Index>
                  (referenceFrame.metaBinaryOrder))
            , hasAlienTiles(false), isAlien(false)
        {}
    };

    struct TileSetInfo : EnhancedInfo {
        struct GlueInfo : TileSetGlues::EnhancedGlue, EnhancedInfo {
            GlueInfo(const registry::ReferenceFrame &referenceFrame
                     , const TileSetGlues::EnhancedGlue &glue)
                : EnhancedGlue(glue)
                , EnhancedInfo(referenceFrame)
            {}

            typedef std::vector<GlueInfo> list;
        };

        TilesetId tilesetId;

        GlueInfo::list glues;

        TileSetInfo(const registry::ReferenceFrame &referenceFrame
                    , const TileSetGlues &tsg)
            : EnhancedInfo(referenceFrame)
            , tilesetId(tsg.tilesetId)
        {
            for (const auto &g : tsg.glues) {
                glues.emplace_back(referenceFrame, g);
            }
        }

        typedef std::vector<TileSetInfo> list;
    };

private:
    virtual OStream::pointer output_impl(const File type);

    virtual IStream::pointer input_impl(File type) const {
        return input_impl(type, true);
    }

    virtual IStream::pointer input_impl(File type, const NullWhenNotFound_t&)
        const
    {
        return input_impl(type, false);
    }

    virtual OStream::pointer
    output_impl(const TileId &tileId, TileFile type);

    virtual IStream::pointer
    input_impl(const TileId &tileId, TileFile type) const {
        return input_impl(tileId, type, true);
    }

    virtual IStream::pointer
    input_impl(const TileId &tileId, TileFile type, const NullWhenNotFound_t&)
        const
    {
        return input_impl(tileId, type, false);
    }

    IStream::pointer input_impl(File type, bool noSuchFile) const;

    IStream::pointer input_impl(const TileId &tileId, TileFile type
                                , bool noSuchFile) const;

    virtual void drop_impl();

    virtual void flush_impl();

    virtual FileStat stat_impl(File type) const;

    virtual FileStat stat_impl(const TileId &tileId, TileFile type) const;

    virtual Resources resources_impl() const;

    Driver::pointer clone_impl(const boost::filesystem::path &root
                               , const CloneOptions &cloneOptions) const;

    virtual std::string info_impl() const;

    IStream::pointer input_mem(File type) const;

    virtual tileset::Index* getTileIndex_impl() { return &tsi_; }

    virtual const tileset::Index* getTileIndex_impl() const { return &tsi_; }

    inline const OldAggregatedOptions& options() const {
        return Driver::options<const OldAggregatedOptions&>();
    }

    TileSetInfo::list buildTilesetInfo() const;

    TileSet::Properties build(const OldAggregatedOptions &options
                              , const CloneOptions &cloneOptions);

    Storage storage_;

    registry::ReferenceFrame referenceFrame_;

    tileset::Index tsi_;

    /** Stuff ripe for delivery.
     */
    TileSetInfo::list tilesetInfo_;

    boost::optional<TileSet::Properties> memProperties_;
};

} } } // namespace vadstena::vts::driver

#endif // vadstena_libs_vts_tileset_driver_aggregated_hpp_included_
