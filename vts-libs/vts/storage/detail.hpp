/**
 * \file vts/storage/detail.hpp
 * \author Vaclav Blazek <vaclav.blazek@citationtech.net>
 *
 * Storage access (internals).
 */

#ifndef vadstena_libs_vts_storage_detail_hpp_included_
#define vadstena_libs_vts_storage_detail_hpp_included_

#include <boost/filesystem/path.hpp>

#include "../storage.hpp"

namespace vadstena { namespace vts {

struct Storage::Properties : StorageProperties {
    /** Data version/revision. Should be incremented anytime the data change.
     *  Used in template URL's to push through caches.
     */
    unsigned int revision;

    /** List of tilesets in this storage.
     */
    TilesetIdList tilesets;

    /** List of glues
     */
    Glue::list glues;

    Properties() : revision(0) {}

    TilesetIdList::iterator findTileset(const std::string& tileset);
    TilesetIdList::const_iterator
    findTileset(const std::string& tileset) const;

    bool hasTileset(const std::string& tileset) const {
        return findTileset(tileset) != tilesets.end();
    }

    Glue::list::iterator findGlue(const Glue::Id& glue);
    Glue::list::const_iterator findGlue(const Glue::Id& glue) const;

    bool hasGlue(const Glue::Id& glue) const {
        return findGlue(glue) != glues.end();
    }
};

struct Storage::Detail
{
    bool readOnly;

    Detail(const boost::filesystem::path &root
           , const StorageProperties &properties
           , CreateMode mode);

    Detail(const boost::filesystem::path &root
           , OpenMode mode);

    ~Detail();

    void loadConfig();

    void saveConfig();

    void add(const TileSet &tileset, const Location &where
             , const std::string tilesetId);

    void remove(const TilesetIdList &tilesetIds);

    Properties addTileset(const Properties &properties
                          , const std::string tilesetId
                          , const Location &where) const;

    /** Removes given tileset from properties and returns new properties and
     *  list of removed glues.
     */
    std::tuple<Properties, Glue::list>
    removeTilesets(const Properties &properties
                   , const TilesetIdList &tilesetIds)
        const;

    boost::filesystem::path root;

    Properties properties;
};

inline void Storage::DetailDeleter::operator()(Detail *d) { delete d; }

} } // namespace vadstena::vts

#endif // vadstena_libs_vts_storage_detail_hpp_included_
