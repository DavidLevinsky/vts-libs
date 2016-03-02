#ifndef vadstena_libs_vts_tileset_driver_hpp_included_
#define vadstena_libs_vts_tileset_driver_hpp_included_

#include <set>
#include <map>
#include <memory>

#include "utility/runnable.hpp"

#include "../../storage/streams.hpp"
#include "../../storage/resources.hpp"

#include "./driver/options.hpp"

namespace vadstena { namespace vts {

using storage::OStream;
using storage::IStream;
using storage::File;
using storage::TileFile;
using storage::FileStat;
using storage::Resources;

class Driver {
public:
    typedef std::shared_ptr<Driver> pointer;

    virtual ~Driver();

    static pointer create(const boost::filesystem::path &root
                          , const boost::any &options, CreateMode mode);

    static pointer open(const boost::filesystem::path &root);

    OStream::pointer output(File type);

    IStream::pointer input(File type) const;

    OStream::pointer output(const TileId tileId, TileFile type);

    IStream::pointer input(const TileId tileId, TileFile type) const;

    FileStat stat(File type) const;

    FileStat stat(const TileId tileId, TileFile type) const;

    Resources resources() const;

    void flush();

    bool externallyChanged() const;

    const boost::any& options() const { return options_; }

    template <typename T>
    const T& options() const { return boost::any_cast<const T&>(options_); }

    template <typename T>
    static const T& options(const boost::any &options) {
        return boost::any_cast<const T&>(options);
    }

    void wannaWrite(const std::string &what) const;

    /** Drop storage.
     */
    void drop();

    /** Sets runnable that is observed during access operations.
     *  If stopped runnable is encountered operation throws Interrupted.
     *  Pass nullptr to stop watching runnable.
     */
    void watch(utility::Runnable *runnable);

    /** Gets old config file content and removes any notion about it.
     */
    boost::optional<std::string> oldConfig() {
        auto c(oldConfig_); oldConfig_= boost::none; return c;
    }

    const boost::filesystem::path& root() const { return root_; }

    /** Returns time of last modification. Recorded at read-only open.
     */
    std::time_t lastModified() const { return lastModified_; }

    bool readOnly() const;

protected:
    /** Creates new storage. Existing storage is overwritten only if mode ==
     *  CreateMode::overwrite.
     */
    Driver(const boost::filesystem::path &root, const boost::any &options
           , CreateMode mode);

    /** Opens storage.
     */
    Driver(const boost::filesystem::path &root
           , const boost::any &options);

private:
    virtual OStream::pointer output_impl(const File type) = 0;

    virtual IStream::pointer input_impl(File type) const = 0;

    virtual OStream::pointer
    output_impl(const TileId tileId, TileFile type) = 0;

    virtual IStream::pointer
    input_impl(const TileId tileId, TileFile type) const = 0;

    virtual void drop_impl() = 0;

    virtual void flush_impl() = 0;

    virtual FileStat stat_impl(File type) const = 0;

    virtual FileStat stat_impl(const TileId tileId, TileFile type)
        const = 0;

    virtual Resources resources_impl() const = 0;

    void checkRunning() const;

    void notRunning() const;

    /** Backing root.
     */
    const boost::filesystem::path root_;

    bool readOnly_;

    /** Path to config
     */
    boost::filesystem::path configPath_;

    /** Path to extra-config
     */
    boost::filesystem::path extraConfigPath_;

    boost::any options_;

    /** Information about root when tileset was open in read-only mode.
     */
    FileStat rootStat_;

    /** Information about config when tileset was open in read-only mode.
     */
    FileStat configStat_;

    /** Information about extra-config when tileset was open in read-only mode.
     */
    FileStat extraConfigStat_;

    /** Runnable associated with the driver.
     */
    utility::Runnable *runnable_;

    /** Content of old config file.
     */
    boost::optional<std::string> oldConfig_;

    /** Time of last modification (recorded at read-only open)
     */
    std::time_t lastModified_;
};

inline void Driver::watch(utility::Runnable *runnable)
{
    runnable_ = runnable;
}

inline void Driver::checkRunning() const
{
    if (!runnable_ || *runnable_) { return; }
    notRunning();
}

inline OStream::pointer Driver::output(File type)
{
    checkRunning();
    return output_impl(type);
}

inline IStream::pointer Driver::input(File type) const
{
    checkRunning();
    return input_impl(type);
}

inline OStream::pointer Driver::output(const TileId tileId, TileFile type)
{
    checkRunning();
    return output_impl(tileId, type);
}

inline IStream::pointer Driver::input(const TileId tileId, TileFile type)
    const
{
    checkRunning();
    return input_impl(tileId, type);
}

inline FileStat Driver::stat(File type) const
{
    checkRunning();
    return stat_impl(type);
}

inline FileStat Driver::stat(const TileId tileId, TileFile type) const
{
    checkRunning();
    return stat_impl(tileId, type);
}

inline storage::Resources Driver::resources() const
{
    return resources_impl();
}

inline void Driver::drop()
{
    checkRunning();
    return drop_impl();
}

inline void Driver::flush()
{
    return flush_impl();
}

} } // namespace vadstena::vts

#endif // vadstena_libs_vts_tileset_driver_hpp_included_
