#ifndef vadstena_libs_tilestorage_driver_hpp_included_
#define vadstena_libs_tilestorage_driver_hpp_included_

#include <iostream>

#include "./types.hpp"

namespace vadstena { namespace tilestorage {

class Driver : boost::noncopyable {
public:
    struct OStream;
    struct IStream;

    typedef std::shared_ptr<Driver> pointer;

    virtual ~Driver() {};

    enum class TileFile { meta, mesh, atlas };

    enum class File { config, tileIndex, metaIndex };

    std::shared_ptr<OStream> output(File type);

    std::shared_ptr<IStream> input(File type) const;

    std::shared_ptr<OStream> output(const TileId tileId, TileFile type);

    std::shared_ptr<IStream> input(const TileId tileId, TileFile type) const;

    bool readOnly() const { return readOnly_; }

    void wannaWrite(const std::string &what) const;

    void begin();

    void commit();

    void rollback();

    class Factory;

    template <typename DriverClass> static void registerDriver();

    static Driver::pointer create(Locator locator
                                  , CreateMode mode);

    static Driver::pointer open(Locator locator, OpenMode mode);

protected:
    Driver(bool readOnly) : readOnly_(readOnly) {}

private:
    virtual std::shared_ptr<OStream> output_impl(const File type) = 0;

    virtual std::shared_ptr<IStream> input_impl(File type) const = 0;

    virtual std::shared_ptr<OStream>
    output_impl(const TileId tileId, TileFile type) = 0;

    virtual std::shared_ptr<IStream>
    input_impl(const TileId tileId, TileFile type) const = 0;

    virtual void begin_impl() = 0;

    virtual void commit_impl() = 0;

    virtual void rollback_impl() = 0;

    static void registerDriver(const std::shared_ptr<Factory> &factory);

    bool readOnly_;
};

class Driver::OStream {
public:
    OStream() {}
    virtual ~OStream() {}
    virtual std::ostream& get() = 0;
    virtual void close() = 0;

    operator std::ostream&() { return get(); }
    typedef std::shared_ptr<OStream> pointer;
};

class Driver::IStream {
public:
    IStream() {}
    virtual ~IStream() {}
    virtual std::istream& get() = 0;
    virtual void close() = 0;

    operator std::istream&() { return get(); }
    typedef std::shared_ptr<IStream> pointer;
};

class Driver::Factory {
public:
    typedef std::shared_ptr<Factory> pointer;
    Factory(const std::string &type) : type(type) {}

    virtual ~Factory() {}

    virtual Driver::pointer create(const std::string location
                                   , CreateMode mode) const = 0;

    virtual Driver::pointer open(const std::string location
                                 , OpenMode mode) const = 0;

    const std::string type;
};

// inline stuff

template <typename DriverClass>
void Driver::registerDriver()
{
    registerDriver(std::make_shared<typename DriverClass::Factory>());
}

inline Driver::OStream::pointer Driver::output(File type)
{
    return output_impl( type);
}

inline Driver::IStream::pointer Driver::input(File type) const
{
    return input_impl(type);
}

inline Driver::OStream::pointer
Driver::output(const TileId tileId, TileFile type)
{
    return output_impl(tileId, type);
}

inline Driver::IStream::pointer
Driver::input(const TileId tileId, TileFile type) const
{
    return input_impl(tileId, type);
}

inline void Driver::begin()
{
    return begin_impl();
}

inline void Driver::commit()
{
    return commit_impl();
}

inline void Driver::rollback()
{
    return rollback_impl();
}

} } // namespace vadstena::tilestorage

#endif // vadstena_libs_tilestorage_driver_hpp_included_

