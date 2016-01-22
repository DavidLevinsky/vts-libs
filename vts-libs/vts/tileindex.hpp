#ifndef vadstena_libs_vts_tileindex_hpp_included_
#define vadstena_libs_vts_tileindex_hpp_included_

#include <map>

#include <boost/filesystem/path.hpp>

#include "./qtree.hpp"
#include "./basetypes.hpp"
#include "./tileop.hpp"

namespace vadstena { namespace vts {

class TileIndex {
public:
    struct Flag {
        typedef std::uint8_t value_type;

        enum : value_type {
            mesh = 0x01
            , watertight = 0x02
            , atlas = 0x04
            , navtile = 0x08
            , meta = 0x10
            , reference = 0x20

             // tile is real if it contains mesh
             , real = mesh

             // content: tile has some content
             , content = (mesh | atlas | navtile)

             // cannot be 0xff since it is reserved value!
            , any = 0x7f
        };

        static bool isReal(value_type flags) { return (flags & real); };
    };

    TileIndex() : minLod_() {}

    TileIndex(LodRange lodRange, const TileIndex *other = nullptr
              , bool noFill = false);

    template <typename Filter>
    TileIndex(LodRange lodRange, const TileIndex &other
              , const Filter &filter, bool fullRange = true);

    TileIndex(const TileIndex &other);

    struct ShallowCopy {};
    TileIndex(const TileIndex &other, ShallowCopy);

    typedef std::vector<QTree> Trees;

    void load(std::istream &is, const boost::filesystem::path &path
              = "unknown");
    void load(const boost::filesystem::path &path);

    void save(std::ostream &os) const;
    void save(const boost::filesystem::path &path) const;

    bool exists(const TileId &tileId) const { return get(tileId); }

    bool real(const TileId &tileId) const {
        return (get(tileId) & Flag::real);
    }

    void fill(Lod lod, const TileIndex &other);

    void fill(const TileIndex &other);

    template <typename Op>
    void fill(Lod lod, const TileIndex &other, const Op &op);

    template <typename Op>
    void fill(const TileIndex &other, const Op &op);

    void set(const TileId &tileId, QTree::value_type value);

    QTree::value_type get(const TileId &tileId) const;

    QTree::value_type checkMask(const TileId &tileId, QTree::value_type mask)
        const
    {
        return get(tileId) & mask;
    }

    void unset(const TileId &tileId) { set(tileId, 0); }

    /** Updates value. Equivalent to set(tileId, op(get(tileId)))
     */
    template <typename Op>
    void update(const TileId &tileId, Op op) {
        set(tileId, op(get(tileId)));
    }

    TileId tileId(Lod lod, long x, long y) const;

    math::Size2 rasterSize(Lod lod) const {
        return math::Size2(1 << lod, 1 << lod);
    }

    bool empty() const;

    Lod minLod() const { return minLod_; }

    Lod maxLod() const;

    LodRange lodRange() const;

    const Trees& trees() const { return trees_; };

    /** Clears lod content.
     */
    void clear(Lod lod);

    /** Returns count of tiles in the index.
     */
    std::size_t count() const;

    const QTree* tree(Lod lod) const;

    /** Sets value as a bit mask.
     *  Affects only bits set in mask.
     */
    void setMask(const TileId &tileId, QTree::value_type mask
                 , QTree::value_type value = Flag::any);

    struct Stat {
        storage::LodRange lodRange;
        std::vector<TileRange> tileRanges;
        std::size_t count;

        Stat()
            : lodRange(LodRange::emptyRange()), count()
        {}
    };

    /** Get statistics for all tiles with given mask.
     */
    Stat statMask(QTree::value_type mask) const;

    /** Translates node values.
     */
    template <typename Op>
    void translate(const Op &op) {
        for (auto &tree : trees_) {
            tree.translateEachNode(op);
        }
    }

    /** Creates new tile index that has given lod range and every tile from
     *  input where Filter return true has all parents up to lodRange.min and
     *  all children down to lodRange.max
     */
    TileIndex grow(const LodRange &lodRange
                   , Flag::value_type type = Flag::any) const;

    /** Intersect this index with the other.
     */
    TileIndex intersect(const TileIndex &other
                        , Flag::value_type type = Flag::any) const;

    /** Checks for any non-overlapping lod.
     */
    bool notoverlaps(const TileIndex &other
                     , Flag::value_type type = Flag::any) const;

    /** Grows this tileindex down (inplace).
     */
    TileIndex& growDown(Flag::value_type type = Flag::any);

    /** Grows this tileindex up (inplace).
     */
    TileIndex& growUp(Flag::value_type type = Flag::any);

    /** Inverts value of all tiles. If tile's mask matches then it is set to
     *  zero; if mask doesn't match it is set to type.
     */
    TileIndex& invert(Flag::value_type type = Flag::any);

    /** Converts this tileindex to simplified index (black -> 0, white -> 1)
     */
    TileIndex& simplify(Flag::value_type type = Flag::any);

    /** Makes tileindex complete -> every existing tile has its parent.
     */
    TileIndex& complete(Flag::value_type type = Flag::any);

    /** Makes tileindex complete down the tree -> every existing tile has its
     *  child.
     */
    TileIndex& completeDown(Flag::value_type type = Flag::any);

    /** Ensures that quad-tree comdition is met: every tile has its sibling.
     */
    TileIndex& round(Flag::value_type type = Flag::any);

private:
    QTree* tree(Lod lod, bool create = false);

    Lod minLod_;
    Trees trees_;
};

typedef std::vector<const TileIndex*> TileIndices;

TileIndex unite(const TileIndices &tis
                , TileIndex::Flag::value_type type = TileIndex::Flag::any
                , const LodRange &lodRange = LodRange::emptyRange());

TileIndex unite(const TileIndex &l, const TileIndex &r
                , TileIndex::Flag::value_type type = TileIndex::Flag::any
                , const LodRange &lodRange = LodRange::emptyRange());

/** Dump tile indes as set of images.
 */
void dumpAsImages(const boost::filesystem::path &path, const TileIndex &ti
                  , TileIndex::Flag::value_type type = TileIndex::Flag::any
                  , const long maxArea = 1 << 26);

// inline stuff

inline bool TileIndex::empty() const
{
    return trees_.empty();
}

inline Lod TileIndex::maxLod() const
{
    return minLod_ + trees_.size() - 1;
}

inline LodRange TileIndex::lodRange() const
{
    if (empty()) { return LodRange::emptyRange(); }
    return { minLod_, maxLod() };
}

inline const QTree* TileIndex::tree(Lod lod) const
{
    auto idx(lod - minLod_);
    if ((idx < 0) || (idx >= int(trees_.size()))) {
        return nullptr;
    }

    // get tree
    return &trees_[idx];
}

inline QTree::value_type TileIndex::get(const TileId &tileId) const
{
    const auto *m(tree(tileId.lod));
    if (!m) { return 0; }
    return m->get(tileId.x, tileId.y);
}

inline void TileIndex::set(const TileId &tileId, QTree::value_type value)
{
    if (auto *m = tree(tileId.lod, true)) {
        m->set(tileId.x, tileId.y, value);
    }
}

template <typename Op>
inline void traverse(const QTree &tree, Lod lod, const Op &op)
{
    tree.forEach([&](long x, long y, QTree::value_type mask)
    {
        op(TileId(lod, x, y), mask);
    }, QTree::Filter::white);
}

template <typename Op>
inline void traverse(const TileIndex &ti, const Op &op)
{
    auto lod(ti.minLod());
    for (const auto &tree : ti.trees()) {
        traverse(tree, lod++, op);
    }
}

template <typename Op>
inline void traverse(const TileIndex &ti, Lod lod, const Op &op)
{
    const auto *tree(ti.tree(lod));
    if (!tree) { return; }
    traverse(*tree, lod, op);
}

template <typename Op>
void TileIndex::fill(Lod lod, const TileIndex &other
                     , const Op &op)
{
    // find old and new trees
    const auto *oldTree(other.tree(lod));
    if (!oldTree) { return; }

    auto *newTree(tree(lod));
    if (!newTree) { return; }

    newTree->merge(*oldTree, op);
}

template <typename Op>
void TileIndex::fill(const TileIndex &other, const Op &op)
{
    for (auto lod : lodRange()) {
        fill(lod, other, op);
    }
}

template <typename Filter>
inline TileIndex::TileIndex(LodRange lodRange, const TileIndex &other
                            , const Filter &filter, bool fullRange)
{
    if (fullRange) {
        // something present in on-disk data
        lodRange = unite(lodRange, other.lodRange());
    }

    // set minimum LOD
    minLod_ = lodRange.min;

    // fill in trees
    for (auto lod : lodRange) {
        trees_.emplace_back(lod);

        fill(lod, other, filter);
    }
}

} } // namespace vadstena::vts

#endif // vadstena_libs_vts_tileindex_hpp_included_
