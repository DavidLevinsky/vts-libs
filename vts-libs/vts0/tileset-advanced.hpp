/**
 * \file vts/tileset.hpp
 * \author Vaclav Blazek <vaclav.blazek@citationtech.net>
 *
 * Advanced tile set API.
 *
 */

#ifndef vtslibs_vts0_tileset_advanced_hpp_included_
#define vtslibs_vts0_tileset_advanced_hpp_included_

#include "../storage/streams.hpp"
#include "./tileset.hpp"

namespace vtslibs { namespace vts0 {

using storage::IStream;
using storage::OStream;
using storage::FileStat;
using storage::File;
using storage::TileFile;

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

    /** Returns size of given file.
     */
    FileStat stat(File type) const;

    /** Returns size of given tile file.
     */
    FileStat stat(const TileId tileId, TileFile type) const;

    MetaNode setMetaNode(const TileId &tileId, const MetaNode& metanode);

    /** Change metalevels. Metatiles and tile/meta indices are regenerated.
     */
    void changeMetaLevels(const LodLevels &metaLevels);

    /** Changes SRS - succeeds only if there is no current SRS or if current SRS
        is same as new SRS
     */
    void changeSrs(const std::string& srs);

    /** Renames tile set, i.e. its id is changed.
     */
    void rename(const std::string &newId);

    /** Regenerate index from metadata.
     */
    void regenerateTileIndex();

    /** Remove the tile and its descendants if its out of the 
     *  defined Lod Range
     */
    void removeOutOfLodRange( const TileId &tileId
                            , const LodRange & lodRange );

    /** Removes the tiles out of the defined extents
     */
    void removeOutOfLodRange( const LodRange & lodRange );

    /** Remove the tile and its descendants if its out of the 
     *  defined extents
     */
    void removeOutOfExtents( const TileId &tileId
                           , const math::Extents2 & extents );

    /** Removes the tiles out of the defined extents
     */
    void removeOutOfExtents( const math::Extents2 & extents );




    /** Forces metadata to the tile and its descendants
     */
    void forceMetadata( TileId tileId
                      , const TileMetadata &metadata
                      , const TileMetadata::MaskType mask 
                                        = ~(TileMetadata::MaskType(0)));

    /** Forces metadata to the whole tileset
     */
    void forceMetadata( const TileMetadata &metadata
                      , const TileMetadata::MaskType mask 
                                        = ~(TileMetadata::MaskType(0)));

    /** Traverse tiles. Calls op(TileId) for each existing tile.
     */
    template <typename Op> void traverseTiles(const Op &op) const;

    /** Traverse metatiles. Calls op(TileId) for each existing metatile.
     */
    template <typename Op> void traverseMetas(const Op &op) const;

    /** Returns tile index.
     */
    const TileIndex& tileIndex() const;

private:
    /** Returns metatile index.
     */
    const TileIndex& metaIndex() const;

    TileSet::pointer tileSet_;
};

template <typename Op>
void TileSet::AdvancedApi::traverseTiles(const Op &op) const
{
    ::vtslibs::vts0::traverseTiles(tileIndex(), op);
}

template <typename Op>
void TileSet::AdvancedApi::traverseMetas(const Op &op) const
{
    ::vtslibs::vts0::traverseTiles(metaIndex(), op);
}

} } // namespace vtslibs::vts0

#endif // vtslibs_vts0_tileset_advanced_hpp_included_
