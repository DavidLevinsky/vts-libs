/**
 * \file vts/storage.hpp
 * \author Vaclav Blazek <vaclav.blazek@citationtech.net>
 *
 * Tile set storage access.
 */

#ifndef vadstena_libs_vts_storage_hpp_included_
#define vadstena_libs_vts_storage_hpp_included_

#include <memory>
#include <string>

#include <boost/optional.hpp>
#include <boost/noncopyable.hpp>
#include <boost/filesystem/path.hpp>

#include "utility/enum-io.hpp"

#include "./tileset.hpp"
#include "./basetypes.hpp"

namespace vadstena { namespace vts {

struct StorageProperties {
    std::string referenceFrame;
};

struct ExtraStorageProperties {
};

struct StoredTileset {
    enum class GlueMode { none, full };

    TilesetId tilesetId;
    GlueMode glueMode;

    typedef std::vector<StoredTileset> list;

    StoredTileset(const boost::optional<TilesetId> &tid = boost::none
                  , GlueMode glueMode = GlueMode::full)
        : tilesetId(tid ? *tid : TilesetId())
        , glueMode(glueMode)
    {}
};

class TileFilter {
public:
    boost::optional<LodRange> lodRange() const { return lodRange_; }

    TileFilter& lodRange(const boost::optional<LodRange> &lodRange) {
        lodRange_ = lodRange;  return *this;
    }

private:
    boost::optional<LodRange> lodRange_;
};

/** Storage interface.
 */
class Storage {
public:
    /** Opens existing storage.
     */
    Storage(const boost::filesystem::path &path, OpenMode mode);

    /** Creates new storage.
     */
    Storage(const boost::filesystem::path &path
            , const StorageProperties &properties
            , CreateMode mode);

    ~Storage();

    struct Location {
        std::string where;

        enum class Direction { below, above };
        Direction direction;

        Location(const std::string &where, Direction direction)
            : where(where), direction(direction)
        {}

        Location() : direction(Direction::below) {}
    };

    /** Adds tileset from given path to the tileset at where location.
     *  Operation fails if give tileset is already present in the stack.
     *
     *  \param tilesetPath path to source tileset
     *  \param where location in the stack where to add
     *  \param info how to store this tileset.
     *  \param filter optional filter for input dataset
     *
     *  Tileset's own id is used if info.tilesetId is empty.
     */
    void add(const boost::filesystem::path &tilesetPath, const Location &where
             , const StoredTileset &info
             , const TileFilter &filter = TileFilter());

    /** Removes given tileset from the storage.
     *
     *  \param tilesetIds Ids of tilesets to remove
     */
    void remove(const TilesetIdList &tilesetIds);

    /** Flattens content of this storage into new tileset at tilesetPath.
     *
     * \param tilesetPath path to tileset
     * \param mode create mode of new tileset
     * \param tilesetId ID of new tileset (defaults to filename of path)
     */
    TileSet flatten(const boost::filesystem::path &tilesetPath
                    , CreateMode mode
                    , const boost::optional<std::string> tilesetId
                    = boost::none);

    /** Returns list of tileses in the stacked order (bottom to top);
     */
    TilesetIdList tilesets() const;

    /** Returns list of existing glues.
     */
    Glue::map glues() const;

    bool externallyChanged() const;

    std::time_t lastModified() const;

    vadstena::storage::Resources resources() const;

    /** Generates map configuration for this storage.
     */
    MapConfig mapConfig() const;

    /** Generates map configuration for storage at given path.
     */
    static MapConfig mapConfig(const boost::filesystem::path &path);

    /** Check for storage at given path.
     */
    static bool check(const boost::filesystem::path &path);

    /** Internals. Public to ease library developers' life, not to allow users
     *  to put their dirty hands in the storage's guts!
     */
    struct Detail;

private:
    struct DetailDeleter { void operator()(Detail*); };
    std::unique_ptr<Detail, DetailDeleter> detail_;
    Detail& detail() { return *detail_; }
    const Detail& detail() const { return *detail_; }

public:
    struct Properties;
};

UTILITY_GENERATE_ENUM_IO(Storage::Location::Direction,
    ((below))
    ((above))
)

UTILITY_GENERATE_ENUM_IO(StoredTileset::GlueMode,
    ((full))
    ((none))
)

template<typename CharT, typename Traits>
inline std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits> &os, const Storage::Location &l)
{
    if (l.where.empty()) {
        if (l.direction == Storage::Location::Direction::above) {
            return os << "@BOTTOM";
        } else {
            return os << "@TOP";
        }
    }

    return os << ((l.direction == Storage::Location::Direction::below)
                  ? '-' : '+') << l.where;
}

template<typename CharT, typename Traits>
inline std::basic_istream<CharT, Traits>&
operator>>(std::basic_istream<CharT, Traits> &is, Storage::Location &l)
{
    std::string id;
    is >> id;
    if (id == "@BOTTOM") {
        l.where.erase();
        l.direction = Storage::Location::Direction::above;
        return is;
    } else if (id == "@TOP") {
        l.where.erase();
        l.direction = Storage::Location::Direction::below;
        return is;
    }

    // +/- and at least one character
    if (id.size() < 2) {
        is.setstate(std::ios::failbit);
        return is;
    }

    switch (id[0]) {
    case '-': l.direction = Storage::Location::Direction::below; break;
    case '+': l.direction = Storage::Location::Direction::above; break;
    default:
        is.setstate(std::ios::failbit);
        return is;
    }
    l.where = id.substr(1);

    return is;
}

} } // namespace vadstena::vts

#endif // vadstena_libs_vts_storage_hpp_included_
