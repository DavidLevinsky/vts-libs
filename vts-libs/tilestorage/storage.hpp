/**
 * \file tilestorage/storage.hpp
 * \author Vaclav Blazek <vaclav.blazek@citationtech.net>
 *
 * Store.
 */

#ifndef vadstena_libs_tilestorage_storage_hpp_included_
#define vadstena_libs_tilestorage_storage_hpp_included_

#include <memory>
#include <vector>

#include <boost/noncopyable.hpp>
#include <boost/filesystem/path.hpp>

#include "./types.hpp"

namespace vadstena { namespace tilestorage {

struct StorageProperties {
    /** List of input tile sets.
     */
    std::vector<std::string> inputSets;

    /** Output tile sets.
     */
    std::string outputSet;
};

class Storage : boost::noncopyable
{
public:
    typedef std::shared_ptr<Storage> pointer;

    /** Needed to instantiate.
     */
    class Factory;
    friend class Factory;

private:
    Storage(const boost::filesystem::path &root, bool readOnly);

    struct Detail;
    std::unique_ptr<Detail> detail_;
    Detail& detail() { return *detail_; }
    const Detail& detail() const { return *detail_; }
};

} } // namespace vadstena::tilestorage

#endif // vadstena_libs_tilestorage_storage_hpp_included_
