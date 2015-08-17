/**
 * \file vts/config.hpp
 * \author Vaclav Blazek <vaclav.blazek@citationtech.net>
 */

#ifndef vadstena_libs_vts_config_hpp_included_
#define vadstena_libs_vts_config_hpp_included_

#include <iostream>

#include <boost/filesystem/path.hpp>

#include "./properties.hpp"

namespace vadstena { namespace vts {

Properties loadConfig(std::istream &in);

void saveConfig(std::ostream &out, const Properties &properties);

Properties loadConfig(const boost::filesystem::path &path);

void saveConfig(const boost::filesystem::path &path
                , const Properties &properties);

} } // namespace vadstena::vts

#endif // vadstena_libs_vts_config_hpp_included_
