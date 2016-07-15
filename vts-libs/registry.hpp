/**
 * \file registry.hpp
 * \author Vaclav Blazek <vaclav.blazek@citationtech.net>
 */

#ifndef vadstena_libs_registry_hpp_included_
#define vadstena_libs_registry_hpp_included_

#include "./registry/referenceframe.hpp"
#include "./registry/freelayer.hpp"
#include "./registry/datafile.hpp"

namespace vadstena { namespace registry {

struct Registry {
    static const Srs* srs(const std::string &id, std::nothrow_t);
    static const Srs& srs(const std::string &id);
    static const Srs::dict srsList();

    static const ReferenceFrame*
    referenceFrame(const std::string &id, std::nothrow_t);
    static const ReferenceFrame&
    referenceFrame(const std::string &id);
    static const ReferenceFrame::dict referenceFrames();

    static const BoundLayer*
    boundLayer(const std::string &id, std::nothrow_t);
    static const BoundLayer& boundLayer(const std::string &id);
    static const BoundLayer*
    boundLayer(BoundLayer::NumericId id, std::nothrow_t);
    static const BoundLayer& boundLayer(BoundLayer::NumericId id);
    static const BoundLayer::dict boundLayers();
    static const BoundLayer::ndict boundLayers(int);

    static const Credit*
    credit(const std::string &id, std::nothrow_t);
    static const Credit& credit(const std::string &id);
    static const Credit* credit(Credit::NumericId id, std::nothrow_t);
    static const Credit& credit(Credit::NumericId id);
    static const Credit::dict credits();
    static const Credit::ndict credits(int);

    enum DataFileKey { filename, path };

    static const DataFile& dataFile(const std::string &path
                                    , DataFileKey key);
    static const DataFile* dataFile(const std::string &path
                                    , DataFileKey key, std::nothrow_t);

    static void init(const boost::filesystem::path &confRoot);

    static boost::filesystem::path confRoot();
};

/** Returns default path to registry.
 *  NB: implemented in file generated from config.cpp.in template
 */
boost::filesystem::path defaultPath();

} } // namespace vadstena::registry

#endif // vadstena_libs_registry_hpp_included_
