#ifndef vadstena_libs_vts_po_hpp_included_
#define vadstena_libs_vts_po_hpp_included_

#include <boost/program_options.hpp>

#include "./types.hpp"

namespace vadstena { namespace vts {

inline void validate(boost::any &v
                     , const std::vector<std::string>& values
                     , Locator*, int)
{
    namespace po = boost::program_options;

    po::validators::check_first_occurrence(v);
    v = boost::any(Locator(po::validators::get_single_string(values)));
}

} } // namespace vadstena::vts

#endif // vadstena_libs_vts_io_hpp_included_
