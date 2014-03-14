#include "math/geometry.hpp"
#include "math/transform.hpp"
#include "imgproc/scanconversion.hpp"

#include "./merge.hpp"

namespace ublas = boost::numeric::ublas;

#define DEBUG 1

namespace vadstena { namespace tilestorage {

namespace {

double triangleArea(const math::Point3 &a, const math::Point3 &b,
                    const math::Point3 &c)
{
    return norm_2(math::crossProduct(b - a, c - a)) * 0.5;
}

double tileQuality(const Tile &tile)
{
    // calculate the total area of the faces in both the XYZ and UV spaces
    double xyzArea(0), uvArea(0);
    const Mesh &mesh(tile.mesh);

    for (const auto &face : mesh.facets)
    {
        xyzArea += triangleArea(mesh.vertices[face.v[0]],
                                mesh.vertices[face.v[1]],
                                mesh.vertices[face.v[2]]);

        uvArea += triangleArea(mesh.texcoords[face.t[0]],
                               mesh.texcoords[face.t[1]],
                               mesh.texcoords[face.t[2]]);
    }
    uvArea *= tile.atlas.cols * tile.atlas.rows;

    // return the number of texels per unit area
    return uvArea / xyzArea;
}


void rasterizeTile(const Tile &tile, const math::Matrix4 &trafo,
                   int index, cv::Mat &qbuffer)
{
    std::vector<imgproc::Scanline> scanlines;

    // draw all faces into the qbuffer
    for (const auto face : tile.mesh.facets)
    {
        cv::Point3f tri[3];
        for (int i = 0; i < 3; i++) {
            math::Point3d pt(transform(trafo, tile.mesh.vertices[face.v[i]]));
            tri[i] = {float(pt(0)), float(pt(1)), float(pt(2))};
        }

        scanlines.clear();
        imgproc::scanConvertTriangle(tri, 0, qbuffer.rows, scanlines);

        for (const auto& sl : scanlines)
        {
            imgproc::processScanline(sl, 0, qbuffer.cols,
                [&](int x, int y, float)
                   { qbuffer.at<int>(y, x) = index; } );
        }
    }
}


math::Matrix4 tileToBuffer(long tileSize, int bufferSize)
{
    math::Matrix4 trafo(ublas::identity_matrix<double>(4));
    trafo(0,0) = trafo(1,1) = double(bufferSize) / tileSize;
    trafo(0,3) = trafo(1,3) = double(bufferSize) / 2;
    return trafo;
}

} // namespace


Tile merge(long tileSize, const Tile::list &tiles
           , const Tile &fallback, int fallbackQuad)
{
#if 1
    // sort tiles by quality
    typedef std::pair<unsigned, double> TileQuality;
    std::vector<TileQuality> qualities;

    for (unsigned i = 0; i < tiles.size(); i++) {
        qualities.emplace_back(i, tileQuality(tiles[i]));
    }
    std::sort(qualities.begin(), qualities.end(),
              [](const TileQuality &a, const TileQuality &b)
                { return a.second < b.second; } );

    // create qbuffer, rasterize meshes in increasing order of quality
    const int QBSize(256);
    cv::Mat qbuffer(QBSize, QBSize, CV_32S, cv::Scalar(-1));

    math::Matrix4 trafo(tileToBuffer(tileSize, QBSize));
    for (const TileQuality &tq : qualities) {
        rasterizeTile(tiles[tq.first], trafo, tq.first, qbuffer);
    }

#if DEBUG
    std::ofstream("qbuffer.m") << "QB = " << qbuffer << ";\n";
#endif

    return tiles[0];

#else
    // simple no-merge algo :P

    LOG(info2) << "merging " << tiles.size() << " tiles";

    // find tile with largest triangle area
    const Tile *t(nullptr);
    double area(0);
    for (const auto &tile : tiles) {
        auto a(tileQuality(tile));
        if (a > area) {
            area = a;
            t = &tile;
        }
    }

    if (t) { return *t; }
    return {};
#endif

    (void) tileSize;
    (void) fallback;
    (void) fallbackQuad;
    (void) tiles;
}

} } // namespace vadstena::tilestorage
