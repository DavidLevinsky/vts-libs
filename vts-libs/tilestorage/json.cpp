#include "jsoncpp/as.hpp"

#include "./json.hpp"

namespace vadstena { namespace tilestorage {

namespace detail {

const int CURRENT_JSON_FORMAT_VERSION(1);

void parse1(Properties &properties, const Json::Value &config)
{
    Json::get(properties.id, config["id"]);

    const auto &foat(config["foat"]);
    Json::get(properties.foat.lod, foat[0]);
    Json::get(properties.foat.easting, foat[1]);
    Json::get(properties.foat.northing, foat[2]);

    const auto &meta(config["meta"]);
    Json::get(properties.metaLevels.lod, meta[0]);
    Json::get(properties.metaLevels.delta, meta[1]);

    Json::get(properties.baseTileSize, config["baseTileSize"]);

    const auto &alignment(config["alignment"]);
    Json::get(properties.alignment(0), alignment[0]);
    Json::get(properties.alignment(1), alignment[1]);

    Json::get(properties.meshTemplate, config["meshTemplate"]);
    Json::get(properties.textureTemplate, config["textureTemplate"]);
    Json::get(properties.metaTemplate, config["metaTemplate"]);

    const auto &defaultPosition(config["defaultPosition"]);
    Json::get(properties.defaultPosition(0), defaultPosition[0]);
    Json::get(properties.defaultPosition(1), defaultPosition[1]);
    Json::get(properties.defaultPosition(2), defaultPosition[2]);

    const auto &defaultOrientation(config["defaultOrientation"]);
    Json::get(properties.defaultOrientation(0), defaultOrientation[0]);
    Json::get(properties.defaultOrientation(1), defaultOrientation[1]);
    Json::get(properties.defaultOrientation(2), defaultOrientation[2]);

    Json::get(properties.textureQuality, config["textureQuality"]);
}

} // namespace detail

void parse(Properties &properties, const Json::Value &config)
{
    try {
        auto version(Json::as<int>(config["version"]));

        switch (version) {
        case 1: return detail::parse1(properties, config);
        }

        LOGTHROW(err2, FormatError)
            << "Invalid config format: unsupported version" << version << ".";

    } catch (const Json::Error &e) {
        LOGTHROW(err2, FormatError)
            << "Invalid config format (" << e.what()
            << "); Unable to work with this storage.";
    }
}

void build(Json::Value &config, const Properties &properties)
{
    config["version"] = Json::Int64(detail::CURRENT_JSON_FORMAT_VERSION);

    config["id"] = properties.id;

    auto &foat(config["foat"] = Json::Value(Json::arrayValue));
    foat.append(Json::UInt64(properties.foat.lod));
    foat.append(Json::UInt64(properties.foat.easting));
    foat.append(Json::UInt64(properties.foat.northing));
    foat.append(Json::UInt64(properties.foatSize));

    auto &meta(config["meta"] = Json::Value(Json::arrayValue));
    meta.append(Json::UInt64(properties.metaLevels.lod));
    meta.append(Json::UInt64(properties.metaLevels.delta));

    config["baseTileSize"] = Json::UInt64(properties.baseTileSize);

    auto &alignment
        (config["alignment"] = Json::Value(Json::arrayValue));
    alignment.append(Json::Int64(properties.alignment(0)));
    alignment.append(Json::Int64(properties.alignment(1)));

    config["meshTemplate"] = properties.meshTemplate;
    config["textureTemplate"] = properties.textureTemplate;
    config["metaTemplate"] = properties.metaTemplate;

    auto &defaultPosition
        (config["defaultPosition"] = Json::Value(Json::arrayValue));
    defaultPosition.append(properties.defaultPosition(0));
    defaultPosition.append(properties.defaultPosition(1));
    defaultPosition.append(properties.defaultPosition(2));

    auto &defaultOrientation
        (config["defaultOrientation"] = Json::Value(Json::arrayValue));
    defaultOrientation.append(properties.defaultOrientation(0));
    defaultOrientation.append(properties.defaultOrientation(1));
    defaultOrientation.append(properties.defaultOrientation(2));

    config["textureQuality"] = Json::Int(properties.textureQuality);
}

} } // namespace vadstena::tilestorage
