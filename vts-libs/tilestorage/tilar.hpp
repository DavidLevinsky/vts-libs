/**
 * \file tilestorage/tilar.hpp
 * \author Vaclav Blazek <vaclav.blazek@citationtech.net>
 *
 * Tile Archive handler.
 */

#ifndef vadstena_libs_tilestorage_tilar_hpp_included_
#define vadstena_libs_tilestorage_tilar_hpp_included_

#include <memory>
#include <cstdint>
#include <ostream>

#include <boost/filesystem/path.hpp>
#include <boost/iostreams/categories.hpp>

#include "../ids.hpp"
#include "../range.hpp"

#include "./types.hpp"
#include "./streams.hpp"

namespace vadstena { namespace tilestorage {

/** Tilar interface.
 */
class Tilar {
public:
    enum class CreateMode {
        //!< truncates already existing file
        truncate
        //!< about to extend already existing file, options must match
        , append
        //!< fail if the file exists
        , failIfExists
    };

    enum class OpenMode { readOnly, readWrite };

    struct Options {
        unsigned int binaryOrder;
        unsigned int filesPerTile;

        bool operator==(const Options &o) const {
            return ((binaryOrder == o.binaryOrder)
                    && (filesPerTile == o.filesPerTile));
        }

        bool operator!=(const Options &o) const {
            return !operator==(o);
        }
    };

    static Tilar open(const boost::filesystem::path &path
                      , OpenMode openMode = OpenMode::readWrite);

    static Tilar create(const boost::filesystem::path &path
                        , const Options &options
                        , CreateMode createMode = CreateMode::append);

    // these 3 functions cannot be defined here due to undefined Detail struct
    Tilar(Tilar&&);
    Tilar& operator=(Tilar&&);
    ~Tilar();

    Tilar(const Tilar&) = delete;
    Tilar& operator=(const Tilar&) = delete;

    struct FileIndex {
        unsigned int col;
        unsigned int row;
        unsigned int type;

        FileIndex(unsigned int col = 0, unsigned int row = 0
                  , unsigned int type = 0)
            : col(col), row(row), type(type)
        {}
    };

    /** Flushes file to the disk (writes new index if needed).
     */
    void flush();

    // operations

    /** Get output stream to write content of new file at given index.
     */
    OStream::pointer output(const FileIndex &index);

    /** Get input stream to read content of file at given index.
     *  Throws if file doesn't exist.
     */
    IStream::pointer input(const FileIndex &index);

    /** Removes file at given index.
     */
    void remove(const FileIndex &index);

private:
    struct Detail;

    Tilar(Detail *detail);

    std::unique_ptr<Detail> detail_;
    Detail& detail() { return *detail_; }
    const Detail& detail() const { return *detail_; }

    class Device; friend class Device;
    class Source; friend class Source;
    class Sink; friend class Sink;
};

} } // namespace vadstena::tilestorage

#endif // vadstena_libs_tilestorage_tilar_hpp_included_
