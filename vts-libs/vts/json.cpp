#include <typeinfo>
#include <cstdint>

#include "dbglog/dbglog.hpp"
#include "jsoncpp/as.hpp"

#include "./json.hpp"
#include "../storage/error.hpp"

namespace vadstena { namespace vts {

namespace detail { namespace tileset {

const int CURRENT_JSON_FORMAT_VERSION(1024);

DriverProperties parseDriver(const Json::Value &driver)
{
    DriverProperties dp;
    Json::get(dp.type, driver, "type");
    if (!driver.isMember("options")) { return dp; }

    const auto &options(driver["options"]);
    if (!options.isObject()) {
        LOGTHROW(err1, Json::Error)
            << "Type of driver[options] is not an object.";
    }
    for (const auto &member : options.getMemberNames()) {
        DriverProperties::Options::value_type vt(member, {});
        const auto &v(options[member]);

        switch (v.type()) {
        case Json::ValueType::intValue:
            if (v.asInt64() >= 0) {
                vt.second = std::uint64_t(v.asUInt64());
            } else {
                vt.second = std::int64_t(v.asInt64());
            }
            break;
        case Json::ValueType::uintValue:
            vt.second = std::uint64_t(v.asUInt64()); break;
        case Json::ValueType::realValue: vt.second = v.asDouble(); break;
        case Json::ValueType::stringValue: vt.second = v.asString(); break;
        case Json::ValueType::booleanValue: vt.second = v.asBool(); break;
        default: continue; // ignoring compound types and null
        }

        dp.options.insert(vt);
    }

    return dp;
}

Json::Value buildDriver(const DriverProperties &dp)
{
    Json::Value driver;
    driver["type"] = dp.type;
    if (dp.options.empty()) { return driver; }

    auto &options(driver["options"] = Json::Value(Json::objectValue));
    for (const auto &vt : dp.options) {
        auto &v(options[vt.first]);
        const auto &ti(vt.second.type());

        if (ti == typeid(std::int64_t)) {
            v = Json::Value
                (Json::Int64(boost::any_cast<std::int64_t>(vt.second)));
        } else if (ti == typeid(std::uint64_t)) {
            v = Json::Value
                (Json::UInt64(boost::any_cast<std::uint64_t>(vt.second)));
        } else if (ti == typeid(double)) {
            v = Json::Value(boost::any_cast<double>(vt.second));
        } else if (ti == typeid(std::string)) {
            v = Json::Value(boost::any_cast<std::string>(vt.second));
        } else if (ti == typeid(bool)) {
            v = Json::Value(boost::any_cast<bool>(vt.second));
        } else {
            LOGTHROW(err1, std::runtime_error)
                << "Driver properties: unable to serialize C++ type "
                "with type_info::name <" << ti.name() << ">.";
        }
    }

    return driver;
}

Properties parse1024(const Json::Value &config)
{
    Properties properties;
    Json::get(properties.id, config, "id");

    Json::get(properties.hasData, config, "hasData");

    Json::get(properties.metaLevels.lod, config, "meta", 0);
    Json::get(properties.metaLevels.delta, config, "meta", 1);

    Json::get(properties.meshTemplate, config, "meshTemplate");
    Json::get(properties.textureTemplate, config, "textureTemplate");
    Json::get(properties.metaTemplate, config, "metaTemplate");

    for (int i(0); i < 3; ++i) {
        Json::get(properties.defaultPosition(i), config, "defaultPosition", i);
    }

    for (int i(0); i < 3; ++i) {
        Json::get(properties.defaultOrientation(i)
                  , config, "defaultOrientation", i);
    }

    Json::get(properties.textureQuality, config, "textureQuality");

    if (config.isMember("driver")) {
        properties.driver = parseDriver(config["driver"]);
    }

    Json::get(properties.srs, config, "srs");

    Json::get(properties.extents.ll(0), config, "extents", 0);
    Json::get(properties.extents.ll(1), config, "extents", 1);
    Json::get(properties.extents.ur(0), config, "extents", 2);
    Json::get(properties.extents.ur(1), config, "extents", 3);

    Json::get(properties.texelSize, config, "texelSize");

    Json::get(properties.verticalAdjustment, config, "verticalAdjustment");

    return properties;
}

} } // namespace detail::tileset

void parse(Properties &properties, const Json::Value &config)
{
    try {
        int version(0);
        Json::get(version, config, "version");

        switch (version) {
        case 1024:
            properties = detail::tileset::parse1024(config);
            return;
        }

        LOGTHROW(err1, storage::FormatError)
            << "Invalid tileset config format: unsupported version "
            << version << ".";

    } catch (const Json::Error &e) {
        LOGTHROW(err1, storage::FormatError)
            << "Invalid tileset config format (" << e.what()
            << "); Unable to work with this tileset.";
    }
}

void build(Json::Value &config, const Properties &properties)
{
    config["version"]
        = Json::Int64(detail::tileset::CURRENT_JSON_FORMAT_VERSION);

    config["id"] = properties.id;

    config["hasData"] = properties.hasData;

    auto &meta(config["meta"] = Json::Value(Json::arrayValue));
    meta.append(Json::Int64(properties.metaLevels.lod));
    meta.append(Json::UInt64(properties.metaLevels.delta));

    config["srs"] = properties.srs;

    auto &extents(config["extents"] = Json::Value(Json::arrayValue));
    extents.append(properties.extents.ll(0));
    extents.append(properties.extents.ll(1));
    extents.append(properties.extents.ur(0));
    extents.append(properties.extents.ur(1));

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
    config["texelSize"] = Json::Value(properties.texelSize);

    config["driver"] = detail::tileset::buildDriver(properties.driver);

    config["verticalAdjustment"] = Json::Value(properties.verticalAdjustment);
}

} } // namespace vadstena::vts
