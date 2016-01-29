#ifndef vadstena_libs_vts_opencv_navtile_hpp
#define vadstena_libs_vts_opencv_navtile_hpp

#include <boost/optional.hpp>

#include <opencv2/core/core.hpp>

#include "../navtile.hpp"

namespace vadstena { namespace vts { namespace opencv {

class NavTile : public vts::NavTile {
public:
    typedef cv::Mat Data;
    typedef float DataType;
    static constexpr int CvDataType = CV_32F;
    typedef std::shared_ptr<NavTile> pointer;

    NavTile() : data_(createData()) {}
    NavTile(const Data &d) { data(d); }

    virtual HeightRange heightRange() const;

    void data(const Data &d);
    const Data& data() const { return data_; }
    Data& data() { return data_; }

    /** Fills in data and mask. Coverage mask is generated from non-zero pixels
     *  in mask. Mask must have the same size as the coverage mask.
     */
    void data(const Data &data, const Data &mask);

    /** Helper function to create data matrix of proper size and type.
     *
     * \param value all pixels in return matrix are set to given value if valid
     * \return create matrix
     */
    static Data createData(boost::optional<DataType> value = boost::none);

private:
    virtual multifile::Table serialize_impl(std::ostream &os) const;

    virtual void deserialize_impl(const HeightRange &heightRange
                                  , std::istream &is
                                  , const boost::filesystem::path &path
                                  , const multifile::Table &table);

    Data data_;
};

cv::Mat renderCoverage(const NavTile &navtile);

} } } // namespace vadstena::vts::opencv

#endif // vadstena_libs_vts_opencv_navtile_hpp
