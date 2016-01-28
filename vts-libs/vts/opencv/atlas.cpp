#include <opencv2/highgui/highgui.hpp>

#include "dbglog/dbglog.hpp"

#include "utility/binaryio.hpp"

#include "../../storage/error.hpp"

#include "./atlas.hpp"

namespace vadstena { namespace vts { namespace opencv {

multifile::Table Atlas::serialize_impl(std::ostream &os) const
{
    multifile::Table table;
    auto pos(os.tellp());

    for (const auto &image : images_) {
        using utility::binaryio::write;
        std::vector<unsigned char> buf;
        cv::imencode(".jpg", image, buf
                     , { cv::IMWRITE_JPEG_QUALITY, quality_ });

        write(os, buf.data(), buf.size());
        pos = table.add(pos, buf.size());
    }

    return table;
}

void Atlas::deserialize_impl(std::istream &is
                             , const boost::filesystem::path &path
                             , const multifile::Table &table)
{
    Images images;
    for (const auto &entry : table) {
        using utility::binaryio::read;

        is.seekg(entry.start);
        std::vector<unsigned char> buf;
        buf.resize(entry.size);
        read(is, buf.data(), buf.size());

        auto image(cv::imdecode(buf, CV_LOAD_IMAGE_COLOR));
        if (!image.data) {
            LOGTHROW(err1, storage::BadFileFormat)
                << "Cannot decode image from block(" << entry.start
                << ", " << entry.size << " in file " << path << ".";
        }
        images.push_back(image);
    }
    images_.swap(images);
}

void Atlas::set(std::size_t index, const Image &image)
{
    if (images_.size() <= index) {
        images_.resize(index + 1);
    }
    images_[index] = image;
}

math::Size2 Atlas::imageSize_impl(std::size_t index) const
{
    if (index >= images_.size()) { return {}; }
    const auto &image(images_[index]);
    return math::Size2(image.cols, image.rows);
}

} } } // namespace vadstena::vts::opencv
