#include <opencv2/highgui/highgui.hpp>

#include "dbglog/dbglog.hpp"

#include "utility/binaryio.hpp"

#include "imgproc/jpeg.hpp"

#include "../../storage/error.hpp"

#include "./atlas.hpp"

namespace vtslibs { namespace vts { namespace opencv {

namespace {

std::vector<unsigned char> mat2jpeg(const cv::Mat &mat, int quality)
{
    std::vector<unsigned char> buf;
    if (quality > 0) {
        // store as jpeg
        cv::imencode(".jpg", mat, buf
                     , { cv::IMWRITE_JPEG_QUALITY, quality });
    } else {
        // store as png
        cv::imencode(".png", mat, buf
                     , { cv::IMWRITE_PNG_COMPRESSION, 9 });
    }
    return buf;
}

cv::Mat jpeg2mat(const std::vector<unsigned char> &buf
                 , const multifile::Table::Entry *entry = nullptr
                 , const boost::filesystem::path *path = nullptr)
{
    auto image(cv::imdecode(buf, CV_LOAD_IMAGE_COLOR));
    if (!image.data) {
        if (entry) {
            LOGTHROW(err1, storage::BadFileFormat)
                << "Cannot decode image from block(" << entry->start
                << ", " << entry->size << " in file " << *path << ".";
        }
        LOGTHROW(err1, storage::BadFileFormat)
            << "Cannot decode image from memory buffer " << buf.size()
            << " bytes long.";
    }
    return image;
}

} // namespace

multifile::Table Atlas::serialize_impl(std::ostream &os) const
{
    multifile::Table table;
    auto pos(os.tellp());

    for (const auto &image : images_) {
        using utility::binaryio::write;
        auto buf(mat2jpeg(image, quality_));
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

        images.push_back(jpeg2mat(buf, &entry, &path));
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

void Atlas::append(const Atlas &atlas)
{
    images_.insert(images_.end(), atlas.images_.begin(), atlas.images_.end());
}

HybridAtlas::HybridAtlas(std::size_t count, const RawAtlas &rawAtlas
                         , int quality)
    : quality_(quality)
{
    for (std::size_t i(0), e(std::min(rawAtlas.size(), count)); i != e; ++i) {
        entries_.emplace_back(rawAtlas.get(i));
    }
}

HybridAtlas::HybridAtlas(std::size_t count, const HybridAtlas &hybridAtlas
                         , int quality)
    : quality_(quality)
{
    for (std::size_t i(0), e(std::min(hybridAtlas.size(), count)); i != e; ++i)
    {
        entries_.push_back(hybridAtlas.entry(i));
    }
}

// Hybrid Atlas: used to build atlas from raw jpeg data (i.e. copy from another
// atlas) or from color images stored in OpenCV matrices
void HybridAtlas::add(const Image &image)
{
    entries_.emplace_back(image);
}

void HybridAtlas::add(const Raw &raw)
{
    entries_.emplace_back(raw);
}

HybridAtlas::Image HybridAtlas::imageFromRaw(const Raw &raw)
{
    return jpeg2mat(raw);
}

HybridAtlas::Raw HybridAtlas::rawFromImage(const Image &image, int quality)
{
    return mat2jpeg(image, quality);
}

void HybridAtlas::set(std::size_t index, const Image &image)
{
    if (entries_.size() <= index) {
        entries_.resize(index + 1);
    }
    auto &entry(entries_[index]);
    entry.raw.clear();
    entry.image = image;
}

void HybridAtlas::set(std::size_t index, const Raw &raw)
{
    if (entries_.size() <= index) {
        entries_.resize(index + 1);
    }
    auto &entry(entries_[index]);
    entry.raw = raw;
    entry.image = cv::Mat();
}

HybridAtlas::Image HybridAtlas::get(std::size_t index) const
{
    const auto &entry(entries_[index]);
    if (entry.image.data) { return entry.image; }
    return imageFromRaw(entry.raw);
}

void HybridAtlas::append(const HybridAtlas &atlas)
{
    entries_.insert(entries_.end(), atlas.entries_.begin()
                    , atlas.entries_.end());
}

void HybridAtlas::append(const opencv::Atlas &atlas)
{
    for (const auto &image : atlas.get()) {
        entries_.emplace_back(image);
    }
}

HybridAtlas::Entry HybridAtlas::entry(std::size_t index) const
{
    return entries_[index];
}

void HybridAtlas::add(const Entry &entry)
{
    entries_.push_back(entry);
}

multifile::Table HybridAtlas::serialize_impl(std::ostream &os) const
{
    multifile::Table table;
    auto pos(os.tellp());

    for (const auto &entry : entries_) {
        using utility::binaryio::write;
        if (entry.image.data) {
            auto buf(mat2jpeg(entry.image, quality_));
            write(os, buf.data(), buf.size());
            pos = table.add(pos, buf.size());
        } else {
            write(os, entry.raw.data(), entry.raw.size());
            pos = table.add(pos, entry.raw.size());
        }
    }

    return table;
}

void HybridAtlas::deserialize_impl(std::istream &is
                                   , const boost::filesystem::path &path
                                   , const multifile::Table &table)
{
    Entries entries;
    for (const auto &entry : table) {
        using utility::binaryio::read;

        is.seekg(entry.start);
        std::vector<unsigned char> buf;
        buf.resize(entry.size);
        read(is, buf.data(), buf.size());

        entries.emplace_back(jpeg2mat(buf, &entry, &path));
    }
    entries_.swap(entries);
}

math::Size2 HybridAtlas::imageSize_impl(std::size_t index) const
{
    if (index >= entries_.size()) { return {}; }
    const auto &entry(entries_[index]);
    if (entry.image.data) {
        // opencv image
        return math::Size2(entry.image.cols, entry.image.rows);
    }

    // raw data
    return imgproc::jpegSize(entry.raw.data(), entry.raw.size());
}

Atlas::Atlas(const vts::Atlas &atlas, int textureQuality)
    : quality_(textureQuality)
{
    if (const auto *in = dynamic_cast<const RawAtlas*>(&atlas)) {
        for (const auto &image : in->get()) {
            images_.push_back(jpeg2mat(image));
        }
        return;
    }

    if (const auto *in = dynamic_cast<const opencv::Atlas*>(&atlas)) {
        images_ = in->get();
        return;
    }

    if (const auto *in = dynamic_cast<const HybridAtlas*>(&atlas)) {
        for (std::size_t i(0), e(in->size()); i != e; ++i) {
            images_.push_back(in->get(i));
        }
        return;
    }

    LOGTHROW(err1, storage::Unimplemented)
        << "Cannot extract images from provided atlas.";
}

HybridAtlas::HybridAtlas(const Atlas &atlas, int textureQuality)
    : quality_(textureQuality)
{
    if (const auto *in = dynamic_cast<const RawAtlas*>(&atlas)) {
        for (const auto &image : in->get()) {
            entries_.emplace_back(image);
        }
        return;
    }

    if (const auto *in = dynamic_cast<const opencv::Atlas*>(&atlas)) {
        for (const auto &image : in->get()) {
            entries_.emplace_back(image);
        }
        return;
    }

    if (const auto *in = dynamic_cast<const HybridAtlas*>(&atlas)) {
        entries_ = in->entries_;
        return;
    }

    LOGTHROW(err1, storage::Unimplemented)
        << "Cannot extract images from provided atlas.";
}

} } } // namespace vtslibs::vts::opencv
