#ifndef vadstena_libs_vts_atlas_hpp
#define vadstena_libs_vts_atlas_hpp

#include <cstdlib>

#include "../storage/streams.hpp"

namespace vadstena { namespace vts {

class Atlas {
public:
    Atlas() {}

    virtual ~Atlas() {}

    virtual std::size_t count() const = 0;

    virtual void serialize(const storage::OStream::pointer &os
                           , std::size_t index
                           , int textureQuality) const = 0;

    virtual void deserialize(const storage::IStream::pointer &is
                             , std::size_t index) = 0;
};

} } // namespace vadstena::vts

#endif // vadstena_libs_vts_atlas_hpp
