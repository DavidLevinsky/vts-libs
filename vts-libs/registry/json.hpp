/**
 * \file registry/json.hpp
 * \author Vaclav Blazek <vaclav.blazek@citationtech.net>
 */

#ifndef vadstena_libs_registry_json_hpp_included_
#define vadstena_libs_registry_json_hpp_included_

#include "jsoncpp/json.hpp"

#include "./referenceframe.hpp"

namespace vadstena { namespace registry {

Json::Value asJson(const ReferenceFrame &rf);
Json::Value asJson(const Srs::dict &srs);
Json::Value asJson(const Credit::dict &credits);
Json::Value asJson(const BoundLayer::dict &boundLayers);
Json::Value asJson(const Position &position);
Position positionFromJson(const Json::Value &value);

Json::Value asJson(const View &view, BoundLayer::dict &boundLayers);
Json::Value asJson(const Roi &roi);
Json::Value asJson(const Roi::list &rois);
Roi::list roisFromJson(const Json::Value &value);

} } // namespace vadstena::registry

#endif // vadstena_libs_registry_json_hpp_included_
