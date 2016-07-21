#include <cstdlib>
#include <string>
#include <iostream>
#include <algorithm>

#include "dbglog/dbglog.hpp"
#include "utility/streams.hpp"

#include "utility/gccversion.hpp"
#include "utility/progress.hpp"
#include "utility/streams.hpp"

#include "service/cmdline.hpp"

#include "../tilestorage.hpp"
#include "../tilestorage/io.hpp"
#include "../tilestorage/po.hpp"
#include "../tilestorage/tileset-advanced.hpp"

#include "../registry.hpp"
#include "../vts0.hpp"
#include "../vts0/io.hpp"
#include "../tilestorage/po.hpp"
#include "../vts0/tileset-advanced.hpp"

#include "../registry/po.hpp"

namespace po = boost::program_options;
namespace ts = vadstena::tilestorage;
namespace vts = vadstena::vts0;
namespace vr = vadstena::registry;
namespace fs = boost::filesystem;

namespace {

struct FilterSettings {
    boost::optional<ts::Extents> extents;
    boost::optional<vts::LodRange> lodRange;
    boost::optional<fs::path> mask;
};

class TileSet2Vts : public service::Cmdline
{
public:
    TileSet2Vts()
        : service::Cmdline("ts2vts", BUILD_TARGET_VERSION)
        , createMode_(vts::CreateMode::failIfExists)
    {
    }

private:
    virtual void configuration(po::options_description &cmdline
                               , po::options_description &config
                               , po::positional_options_description &pd)
        UTILITY_OVERRIDE;

    virtual void configure(const po::variables_map &vars)
        UTILITY_OVERRIDE;

    virtual bool help(std::ostream &out, const std::string &what) const
        UTILITY_OVERRIDE;

    virtual int run() UTILITY_OVERRIDE;

    vts::CreateMode createMode_;
    ts::Locator input_;
    fs::path output_;
    std::string referenceFrame_;
};

void TileSet2Vts::configuration(po::options_description &cmdline
                                , po::options_description &config
                                , po::positional_options_description &pd)
{
    vr::registryConfiguration(cmdline, vr::defaultPath());

    cmdline.add_options()
        ("input", po::value(&input_)->required()
         , "Locator of input old tile set.")
        ("output", po::value(&output_)->required()
         , "Path of output vts tile set.")
        ("overwrite", "Overwrite existing dataset.")
        ("referenceFrame", po::value(&referenceFrame_)->required()
         , "Reference frame.")
        ;

    pd.add("input", 1);
    pd.add("output", 1);

    (void) config;
}

void TileSet2Vts::configure(const po::variables_map &vars)
{
    vr::registryConfigure(vars);

    createMode_ = (vars.count("overwrite")
                   ? vts::CreateMode::overwrite
                   : vts::CreateMode::failIfExists);
}

bool TileSet2Vts::help(std::ostream &out, const std::string &what) const
{
    if (what.empty()) {
        out << R"RAW(ts2vts conversion tool
usage
    ts2vts INPUT OUTPUT [OPTIONS]

This command converts an existing old tileset to new vts format.
)RAW";
    }
    return false;
}

inline vts::TileId asVts(const ts::Index &i)
{
    // since alignment was switched to upper-left corner we have to negate
    // northing; original TileId is tile's lower-left corner therefore we must
    // move to its upper-left corner by subtracting 1
    return { i.lod, i.easting, -1 - i.northing };
}

inline vts::TileId asVts(const ts::Alignment &alignment, long baseTileSize
                         , const ts::TileId &tid)
{
    return asVts(tileIndex(alignment, baseTileSize, tid));
}

inline vts::TileFile asVts(const ts::TileFile &f)
{
    switch (f) {
    case ts::TileFile::mesh: return vts::TileFile::mesh;
    case ts::TileFile::atlas: return vts::TileFile::atlas;
    case ts::TileFile::meta: return vts::TileFile::meta;
    default: throw "Unexpected TileFile value. Go fix your program.";
    }
    throw;
}

vts::MetaNode asVts(const ts::MetaNode &src)
{
    vts::MetaNode dst;
    dst.zmin = src.zmin;
    dst.zmax = src.zmax;
    dst.gsd = src.gsd;
    dst.coarseness = src.coarseness;

    std::copy(&src.heightmap[0][0]
              , &src.heightmap[ts::MetaNode::HMSize - 1][ts::MetaNode::HMSize]
              , &dst.heightmap[0][0]);
    std::copy(&src.pixelSize[0][0]
              , &src.pixelSize[1][2]
              , &dst.pixelSize[0][0]);
    return dst;
}

vts::LodLevels asVts(const ts::LodLevels &src)
{
    vts::LodLevels dst;
    dst.lod = src.lod;
    dst.delta = src.delta;
    return dst;
}

void clone(const ts::Properties &srcProps
           , ts::TileSet &src, vts::TileSet &dst)
{
    // move alignment to upper-left corner
    auto alignment(srcProps.alignment);
    alignment(1) += srcProps.baseTileSize;

    auto asrc(src.advancedApi());
    auto adst(dst.advancedApi());

    const utility::Progress::ratio_t reportRatio(1, 100);
    utility::Progress progress(asrc.tileCount());

    // process all tiles
    asrc.traverseTiles([&](const ts::TileId &tileId)
    {
        auto dTileId(asVts(alignment, srcProps.baseTileSize, tileId));
        const auto *metanode(asrc.findMetaNode(tileId));
        if (!metanode) {
            LOG(warn2)
                << "Cannot find metanode for tile " << tileId << "; "
                << "skipping.";
            return;
        }

        // copy mesh and atlas
        for (auto type : { ts::TileFile::mesh, ts::TileFile::atlas }) {
            copyFile(asrc.input(tileId, type)
                     , adst.output(dTileId, asVts(type)));
        }

        adst.setMetaNode(dTileId, asVts(*metanode));
        (++progress).report(reportRatio, "convert ");
    });
}

int TileSet2Vts::run()
{
    LOG(info3)
        << "Converting tileset <" << input_ << "> into <" << output_ << ">.";

    // open source tileset
    auto src(ts::openTileSet(input_));
    const auto props(src->getProperties());

    auto rootExtents
        (math::Extents2
         (props.alignment
          , math::Point2(props.alignment(0) + props.baseTileSize
                         , props.alignment(1) + props.baseTileSize)));

    auto rf(vr::system.referenceFrames(referenceFrame_));

    // check
    if (rootExtents != rf.rootExtents()) {
        LOGTHROW(err3, std::runtime_error)
            << "Reference frame has different extents than extents computed "
            "from alignment and baseTileSize.";
    }

    // create destination tileset
    vts::CreateProperties cprops;

    auto staticProperties(cprops.staticSetter());
    staticProperties.id(props.id);
    staticProperties.srs(props.srs);
    staticProperties.metaLevels(asVts(props.metaLevels));
    staticProperties.extents
        (math::Extents2
         (props.alignment
          , math::Point2(props.alignment(0) + props.baseTileSize
                         , props.alignment(1) + props.baseTileSize)));
    staticProperties.referenceFrame(referenceFrame_);

    auto settableProperties(cprops.settableSetter());
    settableProperties.textureQuality(props.textureQuality);
    settableProperties.defaultPosition(props.defaultPosition);
    settableProperties.defaultOrientation(props.defaultOrientation);
    settableProperties.texelSize(props.texelSize);

    auto dst(vts::createTileSet(output_, cprops, createMode_));

    clone(props, *src, *dst);

    dst->flush();

    LOG(info3)
        << "Tileset <" << input_ << "> converted into <" << output_ << ">.";
    return EXIT_SUCCESS;
}

} // namespace

int main(int argc, char *argv[])
{
    return TileSet2Vts()(argc, argv);
}
