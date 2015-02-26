/**
 * \file tilestorage/config.hpp
 * \author Vaclav Blazek <vaclav.blazek@citationtech.net>
 */

#ifndef vadstena_libs_tilestorage_config_hpp_included_
#define vadstena_libs_tilestorage_config_hpp_included_

#include <iostream>

#include "./properties.hpp"

namespace vadstena { namespace tilestorage {

Properties loadConfig(std::istream &in);

void saveConfig(std::ostream &out, const Properties &properties);

} } // namespace vadstena::tilestorage

#endif // vadstena_libs_tilestorage_config_hpp_included_
