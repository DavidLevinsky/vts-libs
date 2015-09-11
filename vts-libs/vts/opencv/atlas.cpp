#include <opencv2/highgui/highgui.hpp>

#include "utility/binaryio.hpp"

#include "./atlas.hpp"

namespace vadstena { namespace vts { namespace opencv {

void Atlas::serialize(const storage::OStream::pointer &os
                            , std::size_t index) const
{
    using utility::binaryio::write;
    std::vector<unsigned char> buf;
    cv::imencode(".jpg", images_[index], buf
                 , { cv::IMWRITE_JPEG_QUALITY, quality_ });

    write(os->get(), buf.data(), buf.size());
    os->close();
}

void Atlas::deserialize(const storage::IStream::pointer &is
                              , std::size_t index)
{
    using utility::binaryio::read;
    auto& s(is->get());
    auto size(s.seekg(0, std::ios_base::end).tellg());
    s.seekg(0);
    std::vector<unsigned char> buf;
    buf.resize(size);
    read(s, buf.data(), buf.size());

    auto image(cv::imdecode(buf, CV_LOAD_IMAGE_COLOR));
    is->close();
    set(index, image);
}

void Atlas::set(std::size_t index, const Image &image)
{
    if (images_.size() <= index) {
        images_.resize(index + 1);
    }
    images_[index] = image;
}

} } } // namespace vadstena::vts::opencv
