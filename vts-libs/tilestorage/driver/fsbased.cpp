#include <fstream>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/utility/in_place_factory.hpp>

#include "utility/streams.hpp"
#include "utility/path.hpp"

#include "./fsbased.hpp"
#include "./flat.hpp"
#include "../io.hpp"
#include "../error.hpp"

#include "tilestorage/browser/index.html.hpp"
#include "tilestorage/browser/index-offline.html.hpp"
#include "tilestorage/browser/skydome.jpg.hpp"

namespace vadstena { namespace tilestorage {

namespace fs = boost::filesystem;

namespace {

    const std::string ConfigName("mapConfig.json");
    const std::string TileIndexName("index.bin");
    const std::string TransactionRoot("tx");

    const std::string filePath(File type)
    {
        switch (type) {
        case File::config: return ConfigName;
        case File::tileIndex: return TileIndexName;
        }
        throw "unknown file type";
    }

    const char *extension(TileFile type)
    {
        switch (type) {
        case TileFile::meta: return "meta";
        case TileFile::mesh: return "bin";
        case TileFile::atlas: return "jpg";
        }
        throw "unknown tile file type";
    }

    fs::path filePath(const TileId &tileId, TileFile type)
    {
        return str(boost::format("%s-%07d-%07d.%s")
                   % tileId.lod % tileId.easting % tileId.northing
                   % extension(type));
    }

    class FileOStream : public OStream {
    public:
        FileOStream(const fs::path &path, FsBasedDriver::OnClose onClose
                    = FsBasedDriver::OnClose())
            : path_(path), f_(), onClose_(onClose)
        {
            try {
                f_.exceptions(std::ios::badbit | std::ios::failbit);
                f_.open(path.string()
                        , std::ios_base::out | std::ios_base::trunc);
            } catch (const std::exception &e) {
                LOGTHROW(err1, std::runtime_error)
                    << "Unable to open file " << path << " for writing.";
            }
        }

        virtual ~FileOStream() {
            if (!std::uncaught_exception() && f_.is_open()) {
                LOG(warn3) << "File was not closed!";
            }
        }

        virtual std::ostream& get() UTILITY_OVERRIDE {
            return f_;
        }

        virtual void close() UTILITY_OVERRIDE {
            // TODO: call onClose in case of failure (when exception is thrown)
            // via utility::ScopeGuard
            f_.close();
            onClose_(true);
        }

        virtual std::string name() UTILITY_OVERRIDE {
            return path_.string();
        };

    private:
        fs::path path_;
        utility::ofstreambuf f_;
        FsBasedDriver::OnClose onClose_;
    };

    class FileIStream : public IStream {
    public:
        FileIStream(const fs::path &path)
            : path_(path), f_()
        {
            try {
                f_.exceptions(std::ios::badbit | std::ios::failbit);
                f_.open(path.string());
            } catch (const std::exception &e) {
                LOGTHROW(err1, std::runtime_error)
                    << "Unable to open file " << path << " for reading.";
            }
        }

        virtual ~FileIStream() {
            if (!std::uncaught_exception() && f_.is_open()) {
                LOG(warn3) << "File was not closed!";
            }
        }

        virtual std::istream& get() UTILITY_OVERRIDE {
            return f_;
        }

        virtual void close() UTILITY_OVERRIDE {
            f_.close();
        }

        virtual std::string name() UTILITY_OVERRIDE {
            return path_.string();
        };

    private:
        fs::path path_;
        utility::ifstreambuf f_;
    };

} // namespace

FsBasedDriver::FsBasedDriver(const boost::filesystem::path &root
                       , CreateMode mode)
    : Driver(false)
    , root_(root), tmp_(root / TransactionRoot)
    , dirCache_(root_)
{
    if (!create_directories(root_)) {
        // directory already exists -> fail if mode says so
        if (mode == CreateMode::failIfExists) {
            LOGTHROW(err2, TileSetAlreadyExists)
                << "Tile set at " << root_ << " already exists.";
        }
    }

    // write convenience browser
    utility::write(root_ / "index.html", browser::index_html);
    utility::write(root_ / "index-offline.html", browser::index_offline_html);
    utility::write(root_ / "skydome.jpg", browser::skydome_jpg);
}

FsBasedDriver::FsBasedDriver(const boost::filesystem::path &root
                       , OpenMode mode)
    : Driver(mode == OpenMode::readOnly)
    , root_(root), tmp_(root / TransactionRoot)
    , dirCache_(root_)
{
}

FsBasedDriver::~FsBasedDriver()
{
    if (tx_) {
        LOG(warn3) << "Active transaction on driver close; rolling back.";
        try {
            rollback_impl();
        } catch (const std::exception &e) {
            LOG(warn3)
                << "Error while trying to destroy active transaction on "
                "driver close: <" << e.what() << ">.";
        }
    }
}

OStream::pointer FsBasedDriver::output_impl(File type)
{
    const auto name(filePath(type));
    const auto dir(fileDir(type, name));
    const auto path(writePath(dir, name));
    LOG(info1) << "Saving to " << path.first << ".";
    return std::make_shared<FileOStream>(path.first, path.second);
}

IStream::pointer FsBasedDriver::input_impl(File type) const
{
    const auto name(filePath(type));
    const auto dir(fileDir(type, name));
    const auto path(readPath(dir, name));
    LOG(info1) << "Loading from " << path << ".";
    return std::make_shared<FileIStream>(path);
}

OStream::pointer FsBasedDriver::output_impl(const TileId tileId, TileFile type)
{
    const auto name(filePath(tileId, type));
    const auto dir(fileDir(tileId, type, name));
    const auto path(writePath(dir, name));
    LOG(info1) << "Saving to " << path.first << ".";
    return std::make_shared<FileOStream>(path.first, path.second);
}

IStream::pointer FsBasedDriver::input_impl(const TileId tileId, TileFile type)
    const
{
    const auto name(filePath(tileId, type));
    const auto dir(fileDir(tileId, type, name));
    const auto path(readPath(dir, name));
    LOG(info1) << "Loading from " << path << ".";
    return std::make_shared<FileIStream>(path);
}

void FsBasedDriver::remove_impl(const TileId tileId, TileFile type)
{
    const auto name(filePath(tileId, type));
    const auto dir(fileDir(tileId, type, name));
    const auto path(removePath(dir, name));
    LOG(info1) << "Removing " << path << ".";

    fs::remove_all(path);
}

void FsBasedDriver::begin_impl()
{
    if (tx_) {
        LOGTHROW(err2, PendingTransaction)
            << "Transaction already in progress";
    }

    // remove whole tmp directory
    remove_all(tmp_);
    // create fresh tmp directory
    create_directories(tmp_);

    // begin tx
    tx_ = boost::in_place(tmp_);
}

void FsBasedDriver::commit_impl()
{
    if (!tx_) {
        LOGTHROW(err2, PendingTransaction)
            << "No transaction in progress";
    }

    // move all files updated in transaction to from tmp to regular directory
    for (const auto &file : tx_->files) {
        const auto &name = file.first;
        const auto &record = file.second;
        const auto path(record.dir / name);

        if (record.removed) {
            // remove file
            remove_all(root_ / path);
        } else {
            // create directory for file
            dirCache_.create(record.dir);
            // move file
            rename(tmp_ / path, root_ / path);
        }
    }

    // remove whole tmp directory
    remove_all(tmp_);

    // no tx at all
    tx_ = boost::none;
}

void FsBasedDriver::rollback_impl()
{
    if (!tx_) {
        LOGTHROW(err2, PendingTransaction)
            << "No transaction in progress";
    }

    // remove whole tmp directory
    remove_all(tmp_);

    // no tx at all
    tx_ = boost::none;
}

void FsBasedDriver::drop_impl()
{
    if (tx_) {
        LOGTHROW(err2, PendingTransaction)
            << "Cannot drop tile set inside an active transaction.";
    }

    // remove whole tmp directory
    remove_all(root_);
}

fs::path FsBasedDriver::readPath(const fs::path &dir, const fs::path &name)
    const
{
    if (tx_) {
        // we have active transaction: is file part of the tx?
        auto ffiles(tx_->files.find(name));
        if (ffiles != tx_->files.end()) {
            // yes -> tmp file
            return tmp_ / dir / name;
        }
    }

    // regular path
    return root_ / dir / name;
}

std::pair<fs::path, FsBasedDriver::OnClose>
FsBasedDriver::writePath(const fs::path &dir, const fs::path &name)
{
    DirCache *dirCache(&dirCache_);
    if (tx_) {
        // temporary file
        dirCache = &tx_->dirCache;
    }

    // get dir and return full path
    auto outFile(dirCache->create(dir) / name);
    if (!tx_) {
        // no destination
        return { outFile, [](bool){} };
    }

    auto tmpFile(utility::addExtension(outFile, ".tmp"));
    return {
        tmpFile
        , [this, outFile, tmpFile, name, dir] (bool success)
        {
            if (!success) {
                // failed -> remove
                LOG(warn2)
                    << "Removing failed file " << tmpFile << ".";
                fs::remove(tmpFile);
                return;
            }

            // OK -> move file to destination
            LOG(info1)
                << "Moving file " << tmpFile << " to " << outFile << ".";
            rename(tmpFile, outFile);

            // remember file in transaction
            auto res(tx_->files.insert(Tx::Files::value_type(name, dir)));
            if (res.first->second.removed) {
                // mark as existing
                res.first->second.removed = false;
            }
        }
    };
}

fs::path
FsBasedDriver::removePath(const fs::path &dir, const fs::path &name)
{
    DirCache *dirCache(&dirCache_);
    if (tx_) {
        // temporary file
        dirCache = &tx_->dirCache;
    }

    // get dir and return full path
    auto outFile(dirCache->path(dir) / name);

    // remember removal in transaction
    if (tx_) {
        auto res
            (tx_->files.insert(Tx::Files::value_type(name, { dir, true })));
        if (!res.first->second.removed) {
            // mark as removed
            res.first->second.removed = true;
        }
    }

    return outFile;
}

fs::path FsBasedDriver::DirCache::create(const fs::path &dir)
{
    if (dir.empty()) { return root_; }

    auto rdir(root_ / dir);
    if (dirs_.find(dir) == dirs_.end()) {
        // possibly non-existent dir, create
        create_directories(rdir);
        dirs_.insert(dir);
    }
    return rdir;
}

fs::path FsBasedDriver::DirCache::path(const fs::path &dir)
{
    return root_ / dir;
}

const std::string FlatDriver::help
("Filesystem-based storage driver with flat structure: all "
 "files are stored in one directory.");

} } // namespace vadstena::tilestorage
