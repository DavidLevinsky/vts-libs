#ifndef vadstena_libs_tilestorage_fs_hpp_included_
#define vadstena_libs_tilestorage_fs_hpp_included_

#include <map>

#include "utility/gccversion.hpp"

#include "../tilestorage.hpp"

namespace vadstena { namespace tilestorage {

namespace detail {
    typedef std::map<TileId, MetaNode> MetaNodeMap;
}

class FileSystemStorage : public Storage {
public:
    virtual ~FileSystemStorage();

private:
    /** Grant access to factory.
     */
    friend class Storage::Factory;

    FileSystemStorage(const std::string &root, OpenMode mode);

    FileSystemStorage(const std::string &root
                      , const CreateProperties &properties
                      , CreateMode mode);

    /** Get tile content.
     * \param tileId idetifier of tile to return.
     * \return read tile
     * \throws Error if tile with given tileId is not found
     */
    virtual Tile getTile_impl(const TileId &tileId) override;

    /** Set new tile content.
     * \param tileId idetifier of tile write.
     * \param mesh new tile's mesh
     * \param atlas new tile's atlas
     */
    virtual void setTile_impl(const TileId &tileId, const Mesh &mesh
                              , const Atlas &atlas) override;

    /** Get tile's metadata.
     * \param tileId idetifier of metatile to return.
     * \return read metadata
     * \throws Error if metadate with given tileId is not found
     */
    virtual MetaNode getMetaData_impl(const TileId &tileId) override;

    /** Set tile's metadata.
     * \param tileId idetifier of tile to write metadata to.
     * \param meta new tile's metadata
     */
    virtual void setMetaData_impl(const TileId &tileId, const MetaNode &meta)
        override;

    /** Query for tile's existence.
     * \param tileId identifier of queried tile
     * \return true if given tile exists, false otherwise
     *
     ** NB: Should be optimized.
     */
    virtual bool tileExists_impl(const TileId &tileId) override;

    /** Get storage propeties.
     * \return storage properties
     */
    virtual Properties getProperties_impl() override;

    /** Set new storage propeties.
     * \param properties new storage properties
     * \param mask marks fields to be updated
     * \return all new storage properties after change
     */
    virtual Properties setProperties_impl(const SettableProperties &properties
                                          , int mask) override;

    virtual void flush_impl() override;

    void loadConfig();

    void saveConfig();

    /** Internals.
     */
    struct Detail;
    std::unique_ptr<Detail> detail_;
    Detail& detail() { return *detail_; }
    const Detail& detail() const { return *detail_; }
};

} } // namespace vadstena::tilestorage

#endif // vadstena_libs_tilestorage_fs_hpp_included_
