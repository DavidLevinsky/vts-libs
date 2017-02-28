/**
 * \file vts/storageview.hpp
 * \author Vaclav Blazek <vaclav.blazek@citationtech.net>
 *
 * Tile set storage view access.
 */

#ifndef vtslibs_vts_storageview_hpp_included_
#define vtslibs_vts_storageview_hpp_included_

#include <memory>
#include <string>

#include <boost/optional.hpp>
#include <boost/noncopyable.hpp>
#include <boost/filesystem/path.hpp>

#include "utility/enum-io.hpp"

#include "./tileset.hpp"
#include "./basetypes.hpp"

#include "./storage.hpp"

namespace vtslibs { namespace vts {

/** Properties, imports extra storage properties.
 */
struct StorageViewProperties  {
    /** Path to storage. Can be relative.
     */
    boost::filesystem::path storagePath;

    /** Set of tilesets.
     */
    TilesetIdSet tilesets;

    /** Set of tilesets returned as free surfaces.
     */
    TilesetIdSet freeLayerTilesets;

    /** Extra properties.
     */
    ExtraStorageProperties extra;
};

/** StorageView interface.
 */
class StorageView {
public:
    /** Opens storage view.
     */
    StorageView(const boost::filesystem::path &path);

    ~StorageView();

    bool externallyChanged() const;

    std::time_t lastModified() const;

    vtslibs::storage::Resources resources() const;

    /** Generates map configuration for this storage view.
     */
    MapConfig mapConfig() const;

    const Storage& storage() const;

    const TilesetIdSet& tilesets() const;

    boost::filesystem::path storagePath() const;

    /** Flattens content of this storageview into new tileset at tilesetPath.
     *
     * \param tilesetPath path to tileset
     * \param createOptions create options
     */
    TileSet clone(const boost::filesystem::path &tilesetPath
                  , const CloneOptions &createOptions);

    /** Generates map configuration for storage view at given path.
     */
    static MapConfig mapConfig(const boost::filesystem::path &path);

    /** Check for storageview at given path.
     */
    static bool check(const boost::filesystem::path &path);

    static void relocate(const boost::filesystem::path &root
                         , const RelocateOptions &options
                         , const std::string &prefix = "");

    static void reencode(const boost::filesystem::path &root
                         , const ReencodeOptions &options
                         , const std::string &prefix = "");

    /** Internals. Public to ease library developers' life, not to allow users
     *  to put their dirty hands in the storageview's guts!
     */
    struct Detail;

private:
    std::shared_ptr<Detail> detail_;
    Detail& detail() { return *detail_; }
    const Detail& detail() const { return *detail_; }

public:
    struct Properties;
};


} } // namespace vtslibs::vts

#endif // vtslibs_vts_storageview_hpp_included_
