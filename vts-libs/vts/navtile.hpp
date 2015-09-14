#ifndef vadstena_libs_vts_navtile_hpp
#define vadstena_libs_vts_navtile_hpp

#include <cstdlib>
#include <memory>

#include "math/geometry_core.hpp"

#include "../storage/streams.hpp"
#include "../storage/range.hpp"

namespace vadstena { namespace vts {

class NavTile {
public:
    typedef std::shared_ptr<NavTile> pointer;
    typedef storage::Range<std::int16_t> HeightRange;

    static const math::Size2i size() { return math::Size2i(256, 256); };

    NavTile() {}

    virtual ~NavTile() {}

    virtual void serialize(const storage::OStream::pointer &os) const = 0;

    virtual void deserialize(const HeightRange &heightRange
                             , const storage::IStream::pointer &is) = 0;

    HeightRange heightRange() const { return heightRange_; }

protected:
    HeightRange heightRange_;
};

} } // namespace vadstena::vts

#endif // vadstena_libs_vts_navtile_hpp