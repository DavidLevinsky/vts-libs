/**
 * \file registry/datafile.hpp
 * \author Vaclav Blazek <vaclav.blazek@citationtech.net>
 */

#ifndef vadstena_libs_registry_datafile_hpp_included_
#define vadstena_libs_registry_datafile_hpp_included_

#include "./dict.hpp"

namespace vadstena { namespace registry {

/** Registry data file.
 */
struct DataFile {
    /** Absolute path on the filesystem
     */
    boost::filesystem::path path;

    /** File's content-type
     */
    const char* contentType;

    static constexpr char typeName[] = "data file";

    /** Mapping dictionary
     */
    enum class Key { filename, path };
    typedef StringDictionary<DataFile> dict;

    DataFile(const boost::filesystem::path &path);
};

} } // namespace vadstena::registry

#endif // vadstena_libs_registry_datafile_hpp_included_
