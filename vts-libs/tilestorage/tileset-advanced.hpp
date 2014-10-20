/**
 * \file tilestorage/tileset.hpp
 * \author Vaclav Blazek <vaclav.blazek@citationtech.net>
 *
 * Advanced tile set API.
 *
 */

#ifndef vadstena_libs_tilestorage_tileset_advanced_hpp_included_
#define vadstena_libs_tilestorage_tileset_advanced_hpp_included_

#include "./tileset.hpp"
#include "./streams.hpp"

namespace vadstena { namespace tilestorage {

/** TileSet advanced interface.
 */
class TileSet::AdvancedApi {
public:
    // creates new api
    AdvancedApi(const TileSet::pointer &tileSet) : tileSet_(tileSet) {}

    /** Returns output stream for given file.
     */
    OStream::pointer output(File type);

    /** Returns input stream for given file.
     */
    IStream::pointer input(File type) const;

    /** Returns output stream for given tile file.
     */
    OStream::pointer output(const TileId tileId, TileFile type);

    /** Returns input stream for given tile file.
     */
    IStream::pointer input(const TileId tileId, TileFile type) const;

    /** Change metalevels. Metatiles and tile/meta indices are regenerated.
     */
    void changeMetaLevels(const LodLevels &metaLevels);

    /** Renames tile set, i.e. its id is changed.
     */
    void rename(const std::string &newId);

    /** Regenerate index from metadata.
     */
    void regenerateTileIndex();

    void forceMetadata( TileId tileId
                      , const TileMetadata &metadata
                      , const TileMetadata::MaskType mask 
                                        = ~(TileMetadata::MaskType(0)));

    void forceMetadata( const TileMetadata &metadata
                      , const TileMetadata::MaskType mask 
                                        = ~(TileMetadata::MaskType(0)));

    /** Traverse tiles. Calls op(TileId) for each existing tile.
     */
    template <typename Op> void traverseTiles(const Op &op) const;

    /** Traverse metatiles. Calls op(TileId) for each existing metatile.
     */
    template <typename Op> void traverseMetas(const Op &op) const;

private:
    /** Returns tile index.
     */
    const TileIndex& tileIndex() const;

    /** Returns metatile index.
     */
    const TileIndex& metaIndex() const;

    TileSet::pointer tileSet_;
};

template <typename Op>
void TileSet::AdvancedApi::traverseTiles(const Op &op) const
{
    ::vadstena::tilestorage::traverseTiles(tileIndex(), op);
}

template <typename Op>
void TileSet::AdvancedApi::traverseMetas(const Op &op) const
{
    ::vadstena::tilestorage::traverseTiles(metaIndex(), op);
}

} } // namespace vadstena::tilestorage

#endif // vadstena_libs_tilestorage_tileset_advanced_hpp_included_
