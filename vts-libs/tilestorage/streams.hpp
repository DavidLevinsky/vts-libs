#ifndef vadstena_libs_tilestorage_streams_hpp_included_
#define vadstena_libs_tilestorage_streams_hpp_included_

#include <ctime>
#include <iostream>
#include <memory>

#include <boost/filesystem/path.hpp>

#include "./filetypes.hpp"

namespace vadstena { namespace tilestorage {

struct FileStat {
    std::size_t size;
    std::time_t lastModified;
    const char *contentType;

    FileStat(std::size_t size, std::time_t lastModified
             , const char *contentType = "application/octet-stream")
        : size(size), lastModified(lastModified), contentType(contentType)
    {}

    static FileStat stat(const boost::filesystem::path &path);
};

const char* contentType(File type);
const char* contentType(TileFile type);

class OStream {
public:
    OStream() {}
    virtual ~OStream() {}
    virtual std::ostream& get() = 0;
    virtual void close() = 0;
    virtual std::string name() const = 0;

    virtual FileStat stat() const = 0;

    operator std::ostream&() { return get(); }
    typedef std::shared_ptr<OStream> pointer;
};

class IStream {
public:
    IStream() {}
    virtual ~IStream() {}
    virtual std::istream& get() = 0;
    virtual void close() = 0;
    virtual std::string name() const = 0;

    /** Read data from stream at given location.
     *  defaults to seek & read
     */
    virtual std::size_t read(char *buf, std::size_t size
                             , std::istream::pos_type off);

    virtual FileStat stat() const = 0;

    operator std::istream&() { return get(); }
    typedef std::shared_ptr<IStream> pointer;
};

void copyFile(const IStream::pointer &in, const OStream::pointer &out);

} } // namespace vadstena::tilestorage

#endif // vadstena_libs_tilestorage_streams_hpp_included_

