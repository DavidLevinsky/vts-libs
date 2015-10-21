#include <boost/filesystem.hpp>
#include <boost/format.hpp>

#include <opencv2/highgui/highgui.hpp>

#include "dbglog/dbglog.hpp"
#include "utility/binaryio.hpp"

#include "../storage/error.hpp"

#include "./tileindex.hpp"
#include "./tileop.hpp"
#include "./io.hpp"
#include "./tileindex-io.hpp"

namespace vadstena { namespace vts {

namespace fs = boost::filesystem;

namespace {
    const char TILE_INDEX_IO_MAGIC[2] = { 'T', 'I' };
}

TileIndex::TileIndex(const TileIndex &other)
    : minLod_(other.minLod_)
    , trees_(other.trees_)
{
}

TileIndex::TileIndex(const TileIndex &other, ShallowCopy)
    : minLod_(other.minLod_)
{
    trees_.reserve(other.trees_.size());
    for (const auto &tree : other.trees_) {
        trees_.emplace_back(tree.order());
    }
}

TileIndex::TileIndex(LodRange lodRange
                     , const TileIndex *other, bool noFill)
{
    // include old definition if non-empty
    if (other && !other->empty()) {
        // something present in on-disk data
        lodRange = unite(lodRange, other->lodRange());
    }

    // set minimum LOD
    minLod_ = lodRange.min;

    // fill in trees
    for (auto lod : lodRange) {
        trees_.emplace_back(lod);

        // fill in old data (if exists)
        if (other && !noFill) {
            fill(lod, *other);
        };
    }
}

void TileIndex::fill(Lod lod, const TileIndex &other)
{
    // find old and new trees
    const auto *oldTree(other.tree(lod));
    if (!oldTree) { return; }

    auto *newTree(tree(lod));
    if (!newTree) { return; }

    newTree->merge(*oldTree, [](QTree::value_type value) { return value; });
}

void TileIndex::fill(const TileIndex &other)
{
    for (auto lod : lodRange()) {
        fill(lod, other);
    }
}

void TileIndex::load(std::istream &f, const fs::path &path)
{
    using utility::binaryio::read;

    char magic[sizeof(TILE_INDEX_IO_MAGIC)];
    read(f, magic);

    if (std::memcmp(magic, TILE_INDEX_IO_MAGIC, sizeof(TILE_INDEX_IO_MAGIC))) {
        LOGTHROW(err2, storage::Error)
            << "TileIndex " << path << " has wrong magic.";
    }

    uint8_t minLod, size;
    read(f, minLod);
    read(f, size);
    minLod_ = minLod;

    trees_.resize(size);

    for (auto &tree : trees_) {
        tree.load(f, path);
    }
}

void TileIndex::load(const fs::path &path)
{
    std::ifstream f;
    f.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    f.open(path.string(), std::ifstream::in | std::ifstream::binary);

    load(f, path);

    f.close();
}

void TileIndex::save(std::ostream &f) const
{
    using utility::binaryio::write;

    write(f, TILE_INDEX_IO_MAGIC); // 7 bytes

    write(f, uint8_t(minLod_));
    write(f, uint8_t(trees_.size()));

    // save lod-tree mapping
    for (const auto &tree : trees_) {
        tree.save(f);
    }
}

void TileIndex::save(const fs::path &path) const
{
    utility::ofstreambuf f;
    f.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    f.open(path.string(), std::ifstream::out | std::ifstream::trunc);

    save(f);

    f.close();
}

void TileIndex::clear(Lod lod)
{
    if (auto *m = tree(lod)) {
        m->recreate(m->order());
    }
}

QTree* TileIndex::tree(Lod lod, bool create)
{
    auto idx(lod - minLod_);
    if (!create) {
        if ((idx < 0) || (idx >= int(trees_.size()))) {
            return nullptr;
        }

        // get tree
        return &trees_[idx];
    }

    // first lod? just add
    if (trees_.empty()) {
        LOG(debug) << "Creating first lod: " << lod;
        minLod_ = lod;
        trees_.emplace_back(lod);

    } else if (lod < minLod_) {
        // LOD too low
        // generate all tree up to given LOD
        Trees trees;
        for (Lod l(lod); l < minLod_; ++l) {
            LOG(debug) << "Prepending lod: " << l;
            trees.emplace_back(l);
        }
        // prepend all generated trees befor existing
        trees_.insert(trees_.begin(), trees.begin(), trees.end());
        minLod_ = lod;

    } else if (lod >= (minLod_ + int(trees_.size()))) {
        // LOD too high
        // generate all tree up to given LOD
        for (Lod l(minLod_ + trees_.size()); l <= lod; ++l) {
            LOG(debug) << "Appending lod: " << l;
            trees_.emplace_back(l);
        }
    }

    // get tree
    return &trees_[lod - minLod_];
}

std::size_t TileIndex::count() const
{
    std::size_t total(0);
    for (const auto &tree : trees_) {
        total += tree.count();
    }
    return total;
}

void TileIndex::setMask(const TileId &tileId, QTree::value_type mask
                        , bool on)
{
    update(tileId, [&](QTree::value_type value) -> QTree::value_type
    {
        return (on
                ? (value | mask)
                : (value & ~mask));
    });
}

TileIndex::Stat TileIndex::statMask(QTree::value_type mask) const
{
    Stat stat;

    auto lod(minLod_);
    for (const auto &tree : trees_) {
        stat.tileRanges.emplace_back(math::InvalidExtents{});
        auto &tileRange(stat.tileRanges.back());

        tree.forEachNode([&](unsigned int x, unsigned int y, unsigned int size
                             , QTree::value_type value)
        {
            if (!(value & mask)) { return; }

            // remember lod
            storage::update(stat.lodRange, lod);

            // update tile range
            math::update(tileRange, x, y);
            math::update(tileRange, x + size - 1, y + size - 1);
        });
        ++lod;
    }

    if (stat.lodRange.empty()) {
        // nothing, drop whole tileRanges
        stat.tileRanges.clear();
    } else {
        // remove difference between minLod_ and stat.lodRange.min (if any)
        stat.tileRanges.erase
            (stat.tileRanges.begin()
             , stat.tileRanges.begin() + (stat.lodRange.min - minLod_));
    }

    return stat;
}

TileIndex TileIndex::grow(const LodRange &lodRange
                          , Flag::value_type type) const
{
    auto filter([type](QTree::value_type value) { return (value & type); });
    TileIndex ti(lodRange, *this, filter);

    if (trees_.size() < 2) {
        // nothing to grow
        return ti;
    }

    // traverse trees top to bottom and refine -> propagates tiles from top
    // to bottom
    {
        auto lod(lodRange.min);
        auto ctrees(ti.trees_.begin());

        for (auto itrees(ctrees + 1), etrees(ti.trees_.end());
             itrees != etrees; ++itrees, ++ctrees, ++lod)
        {
            LOG(debug) << "gd: " << lod << " -> " << (lod + 1);

            auto &tree(*itrees);

            // merge in parent -> all children are set
            tree.merge(*ctrees, filter);
        }
    }

    // traverse trees bottom to top and coarsen -> propagates tiles from bottom
    // to top
    {
        auto lod(lodRange.max);
        auto ctrees(ti.trees_.rbegin());

        for (auto itrees(ctrees + 1), etrees(ti.trees_.rend());
             itrees != etrees; ++itrees, ++ctrees, --lod)
        {
            LOG(debug) << "gu: " << lod << " -> " << (lod - 1);

            // make copy of child
            QTree child(*ctrees);
            auto &tree(*itrees);

            // coarsen child (do not change child!)
            child.coarsen(filter);
            // merge in coarsened child -> all parents are set
            tree.merge(child, filter);
        }
    }

    return ti;
}

TileIndex TileIndex::intersect(const TileIndex &other
                               , Flag::value_type type)
    const
{
    auto filter([type](QTree::value_type value) { return (value & type); });
    TileIndex ti(lodRange(), *this, filter);

    if (ti.empty()) {
        // nothing to intersect
        return ti;
    }

    auto ntree(ti.trees_.begin());
    for (auto lod : lodRange()) {
        if (const auto *otree = other.tree(lod)) {
            ntree->intersect(*otree, filter);
        }
        ++ntree;
    }
    return ti;
}

bool TileIndex::notoverlaps(const TileIndex &other, Flag::value_type type)
    const
{
    auto filter([type](QTree::value_type value) { return (value & type); });

    // different lod range -> mismatch
    if (lodRange() != other.lodRange()) { return true; }

    auto ntree(trees_.begin());
    for (const auto &otree : other.trees_) {
        if (!ntree->overlaps(otree, filter)) {
            return true;
        }
        ++ntree;
    }
    return false;
}

} } // namespace vadstena::vts
