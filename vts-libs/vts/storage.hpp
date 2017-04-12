/**
 * \file vts/storage.hpp
 * \author Vaclav Blazek <vaclav.blazek@citationtech.net>
 *
 * Tile set storage access.
 */

#ifndef vtslibs_vts_storage_hpp_included_
#define vtslibs_vts_storage_hpp_included_

#include <memory>
#include <string>

#include <boost/optional.hpp>
#include <boost/noncopyable.hpp>
#include <boost/filesystem/path.hpp>

#include "utility/enum-io.hpp"

#include "./tileset.hpp"
#include "./basetypes.hpp"
#include "./glue.hpp"
#include "./virtualsurface.hpp"
#include "./options.hpp"

namespace vtslibs { namespace vts {

struct StorageProperties {
    std::string referenceFrame;
};

struct ExtraStorageProperties {
    /** Override position.
     */
    boost::optional<registry::Position> position;

    /** ROI server(s) definition.
     */
    registry::Roi::list rois;

    /** Initial view.
     */
    registry::View view;

    /** Named views.
     */
    registry::View::map namedViews;

    /** Credits definition to include in the output.
     */
    registry::Credit::dict credits;

    /** Bound layers definition to include in the output.
     */
    registry::BoundLayer::dict boundLayers;

    /** Free layers definition to include in the output.
     */
    registry::FreeLayer::dict freeLayers;

    /** Browser core options. Opaque structure.
     */
    boost::any browserOptions;

    // TODO: freeLayers

    ExtraStorageProperties() {}
};

typedef std::set<std::string> Tags;

/** Info about stored tileset
 */
struct StoredTileset {
    /** Unique tileset identifier.
     */
    TilesetId tilesetId;

    /** Base identifier (without version).
     */
    TilesetId baseId;

    /** Version of tileset
     */
    int version;

    /** Tileset tags
     */
    Tags tags;

    typedef std::vector<StoredTileset> list;

    typedef std::vector<const StoredTileset*> constptrlist;

    StoredTileset() : version() {}

    StoredTileset(const TilesetId &tilesetId)
        : tilesetId(tilesetId), version()
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

class PendingGluesError : public std::runtime_error {
public:
    PendingGluesError(Glue::IdSet glues)
        : std::runtime_error("Penging glues.")
        , glues_(std::move(glues))
    {}

    // needed by old gcc
    virtual ~PendingGluesError() throw() {}

    const Glue::IdSet& glues() const { return glues_; }

private:
    Glue::IdSet glues_;
};

/** Helper for storage/glue locking.
 *
 *  If glueId is empty, whole storage is locked.
 */
class StorageLocker {
public:
    typedef std::shared_ptr<StorageLocker> pointer;

    StorageLocker() {};
    virtual ~StorageLocker() {}

    /** Locks storage (if sublock is empty) or specific entity inside storage
     *  (if sublock is non-empty)
     */
    void lock(const std::string &sublock = std::string());

    /** Unlocks storage (if glueId is empty) or specific glue (ig glueId is
     *  non-empty)
     */
    void unlock(const std::string &sublock = std::string());

private:
    virtual void lock_impl(const std::string &sublock) = 0;
    virtual void unlock_impl(const std::string &sublock) = 0;
};

class ScopedStorageLock {
public:
    ScopedStorageLock(const StorageLocker::pointer &locker
                      , const std::string &sublock = std::string()
                      , ScopedStorageLock *lockToUnlock = nullptr)
        : locker_(locker), sublock_(sublock), locked_(false)
        , lockToUnlock_(lockToUnlock)
    {
        // lock this lock
        lock();
        // unlock other if set
        if (lockToUnlock_) { lockToUnlock_->unlock(); }
    }

    ~ScopedStorageLock() {
        // lock other if set
        if (lockToUnlock_) { lockToUnlock_->lock(); }

        // unlock this
        try {
            unlock();
        } catch (const std::exception &e) {
            LOG(fatal) << "Unable to unlock storage lock, bailing out. "
                "Error was: <" << e.what() << ">.";
            std::abort();
        } catch (...) {
            LOG(fatal) << "Unable to unlock storage lock, bailing out. "
                "Error is unknown.";
            std::abort();
        }
    }

    void lock() {
        if (!locker_ || locked_) { return; }
        locker_->lock(sublock_);
        locked_ = true;
    }
    void unlock() {
        if (!locker_ || !locked_) { return; }
        locker_->unlock(sublock_);
        locked_ = false;
    }

private:
    StorageLocker::pointer locker_;
    std::string sublock_;
    bool locked_;
    ScopedStorageLock *lockToUnlock_;
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

    /** Add options:
     *  bumpVersion: bump version in case of ID collision
     *  textureQuality: JPEG quality of glue textures 0 means no atlas repacking
     *  filter optional: filter for input dataset
     *  dryRun: do not modify anything, simulate add
     *  tags: set of tags assigned to added tileset
     *  openOptions: options for tileset open
     *  mode: glue generation mode
     *  overwrite: allow glue overwrite
     *  locker: external locking API (leave unset if external locking is not
     *          available)
     */
    struct AddOptions : public GlueCreationOptions {
        bool bumpVersion;
        TileFilter filter;
        bool dryRun;
        boost::optional<boost::filesystem::path> tmp;
        Tags tags;
        OpenOptions openOptions;
        enum Mode { legacy, full, lazy };
        Mode mode;
        bool overwrite;
        StorageLocker::pointer locker;

        AddOptions()
            : bumpVersion(false), filter(), dryRun(false)
            , mode(Mode::legacy), overwrite(false)
        {}
    };

    /** Adds tileset from given path to the tileset at where location.
     *  Operation fails if give tileset is already present in the stack.
     *
     *  \param tilesetPath path to source tileset
     *  \param where location in the stack where to add
     *  \param tilesetId (base) id of added tileset;
     *                   pass empty string to get ID from source
     *
     *  Tileset's own id is used if info.tilesetId is empty.
     */
    void add(const boost::filesystem::path &tilesetPath, const Location &where
             , const TilesetId &tilesetId, const AddOptions &addOptions);

    /** Removes given tileset from the storage.
     *
     * Removes all glues and virtual surfaces that reference given tileset.
     *
     *  \param tilesetIds Ids of tilesets to remove
     */
    void remove(const TilesetIdList &tilesetIds
                , const StorageLocker::pointer &locker
                = StorageLocker::pointer());

    void generateGlues(const TilesetId &tilesetId
                       , const AddOptions &addOptions);

    void generateGlue(const Glue::Id &glueId
                      , const AddOptions &addOptions);

    /** Creates a virtual surface from storage.
     *
     *  Operation fails if given virtual surface is already present in the stack
     *  (unless mode is Create::Mode overwrite) or any tileset referenced by
     *  virtual surface doesn't exist.
     *
     *  \param virtualSurfaceId virtual surface ID
     *  \param mode create mode
     */
    void createVirtualSurface(const TilesetIdSet &tilesets
                              , CreateMode mode
                              , const StorageLocker::pointer &locker
                              = StorageLocker::pointer());

    /** Removes a virtual surface from storage.
     *
     *  \param virtualSurfaceId virtual surface ID
     */
    void removeVirtualSurface(const TilesetIdSet &tilesets
                              , const StorageLocker::pointer &locker
                              = StorageLocker::pointer());

    /** Flattens content of this storage into new tileset at tilesetPath.
     *
     * \param tilesetPath path to tileset
     * \param createOptions create options
     * \param subset optional subset of tilesets
     */
    TileSet clone(const boost::filesystem::path &tilesetPath
                  , const CloneOptions &createOptions
                  , const TilesetIdSet *subset = nullptr) const;

    /** Returns list of tileset ID's of tilesets in the stacked order (bottom to
     *  top).
     */
    TilesetIdList tilesets() const;

    /** Returns list of tileset ID's of tilesets in the stacked order (bottom to
     *  top); only subset matching given set is returned.
     */
    TilesetIdList tilesets(const TilesetIdSet &subset) const;

    /** Returns list of stored tilesets in the stacked order (bottom to top);
     */
    StoredTileset::list storedTilesets() const;

    /** Returns list of existing glues.
     */
    Glue::map glues() const;

    /** Return list of tileset's glues (glues that have given tileset at the top
     *  of the stack).
     */
    Glue::list glues(const TilesetId &tilesetId) const;

    /** Return list of tileset's glues (glues that have given tileset at the top
     *  of the stack).
     */
    Glue::list glues(const TilesetId &tilesetId
                     , const std::function<bool(const Glue::Id&)> &filter)
        const;

    /** Return list of all pending glues.
     */
    Glue::IdSet pendingGlues() const;

    /** Return list tileset's pending glues.
     */
    Glue::IdSet pendingGlues(const TilesetId &tilesetId) const;

    /** Return list pending glues in mapconfig.
     *
     * \subset subset of tilesets to be included, use null for no limit.
     */
    Glue::IdSet pendingGlues(const TilesetIdSet *subset) const;

    /** Returns list of existing virtualSurfaces.
     */
    VirtualSurface::map virtualSurfaces() const;

    /** Return list of tileset's virtualSurfaces (virtualSurfaces that have
     *  given tileset at the top of the stack).
     */
    VirtualSurface::list virtualSurfaces(const TilesetId &tilesetId) const;

    /** Return list of tileset's virtualSurfaces (virtualSurfaces that have
     *  given tileset at the top of the stack).
     */
    VirtualSurface::list
    virtualSurfaces(const TilesetId &tilesetId
                    , const std::function<bool(const VirtualSurface::Id&)>
                    &filter) const;

    bool externallyChanged() const;

    std::time_t lastModified() const;

    vtslibs::storage::Resources resources() const;

    boost::filesystem::path path() const;

    const StorageProperties& getProperties() const;

    const registry::ReferenceFrame& referenceFrame() const;

    TileSet open(const TilesetId &tilesetId) const;

    TileSet open(const Glue &glue) const;

    boost::filesystem::path path(const TilesetId &tilesetId) const;

    boost::filesystem::path path(const Glue &glue) const;

    boost::filesystem::path path(const Glue::Id &glueId) const;

    boost::filesystem::path path(const VirtualSurface &virtualSurface) const;

    void updateTags(const TilesetId &tilesetId
                    , const Tags &add, const Tags &remove);

    /** Generates map configuration for this storage.
     */
    MapConfig mapConfig() const;

    /** Generates map configuration for storage at given path.
     */
    static MapConfig mapConfig(const boost::filesystem::path &path);

    /** Special mapconfig generator to get subset (i.e. storage view) map
     *  configuration.
     *
     * \param path path to storage
     * \param extra extra configuration
     * \param subset tilesets to put in output
     * \param freeLayers tilesets to be put in output as mesh-tiles free layers
     * \param prefix path prefix in all URL templates
     */
    static MapConfig mapConfig(const boost::filesystem::path &path
                               , const ExtraStorageProperties &extra
                               , const TilesetIdSet &subset
                               , const TilesetIdSet &freeLayers
                               , const boost::filesystem::path &prefix);

    /** Check for storage at given path.
     */
    static bool check(const boost::filesystem::path &path);

    static void relocate(const boost::filesystem::path &root
                         , const RelocateOptions &options
                         , const std::string &prefix = "");

    static void reencode(const boost::filesystem::path &root
                         , const ReencodeOptions &options
                         , const std::string &prefix = "");

    /** Internals. Public to ease library developers' life, not to allow users
     *  to put their dirty hands in the storage's guts!
     */
    struct Detail;

private:
    std::shared_ptr<Detail> detail_;
    Detail& detail() { return *detail_; }
    const Detail& detail() const { return *detail_; }

public:
    struct Properties;
};

UTILITY_GENERATE_ENUM_IO(Storage::Location::Direction,
    ((below))
    ((above))
)

UTILITY_GENERATE_ENUM_IO(Storage::AddOptions::Mode,
    ((legacy))
    ((full))
    ((lazy))
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

inline void StorageLocker::lock(const std::string &sublock) {
    lock_impl(sublock);
}

inline void StorageLocker::unlock(const std::string &sublock) {
    unlock_impl(sublock);
}

} } // namespace vtslibs::vts

#endif // vtslibs_vts_storage_hpp_included_
