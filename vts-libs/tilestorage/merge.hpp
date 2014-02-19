#ifndef vadstena_libs_tilestorage_merge_hpp_included_
#define vadstena_libs_tilestorage_merge_hpp_included_

#include "./types.hpp"

namespace vadstena { namespace tilestorage {

constexpr int MERGE_WHOLE_FALLBACK_TILE = 100;
constexpr int MERGE_NO_FALLBACK_TILE = -100;

/** Merge tiles based on their quality. Areas not covered by any tile in `tiles`
 *  is covered by `fallbackQuad` quadrant of `fallback` tile.
 *
 * Prerequisities:
 *     * all input tiles have dimension AxA
 *     * fallback tile has dimension 2Ax2A
 *     * output tile has dimension AxA
 *
 * \param tiles tiles to merge
 * \param fallback fallback tile for areas not covered by any tile
 * \param fallbackQuad which quadrant of fallback tile (or whole tile) to use:
 *            * 0: lower-left
 *            * 1: lower-right
 *            * 2: upper-left
 *            * 3: upper-right
 *            * other positive: whole tile
 *            * other negative: ignore fallback tile
 * \return merged tile
 */
Tile merge(const Tile::list &tiles, const Tile &fallback
           , int fallbackQuad);

} } // namespace vadstena::tilestorage

#endif // vadstena_libs_tilestorage_merge_hpp_included_
