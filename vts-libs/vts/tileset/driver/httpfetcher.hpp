#ifndef vadstena_libs_vts_tileset_driver_httpfetcher_hpp_included_
#define vadstena_libs_vts_tileset_driver_httpfetcher_hpp_included_

#include <memory>

#include "../driver.hpp"

namespace vadstena { namespace vts { namespace driver {

class HttpFetcher {
public:
    class Options {
    public:
        Options() : tries_(-1) {}

        int tries() const { return tries_; }
        Options& tries(int tries) { tries_ = tries; return *this; }

        const OpenOptions::CNames cnames() const { return cnames_; }
        Options& cnames(const OpenOptions::CNames &cnames) {
            cnames_ = cnames; return *this;
        }

    private:
        /** Number of attempts to do before failing.
         *  Negative number means forever (default).
         */
        int tries_;
        OpenOptions::CNames cnames_;
    };

    HttpFetcher(const std::string &rootUrl, const Options &options);

    IStream::pointer input(File type, bool noSuchFile = true) const;

    IStream::pointer input(const TileId &tileId, TileFile type
                           , unsigned int revision
                           , bool noSuchFile = true) const;

private:
    const std::string rootUrl_;
    Options options_;
    std::shared_ptr<void> handle_;
};

} } } // namespace vadstena::vts::driver

#endif // vadstena_libs_vts_tileset_driver_httpfetcher_hpp_included_
