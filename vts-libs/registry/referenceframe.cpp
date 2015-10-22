/**
 * \file storage/referenceframe.cpp
 * \author Vaclav Blazek <vaclav.blazek@citationtech.net>
 */

#include <fstream>

#include <boost/lexical_cast.hpp>

#include "dbglog/dbglog.hpp"

#include "jsoncpp/json.hpp"
#include "jsoncpp/as.hpp"

#include "../storage/error.hpp"
#include "./referenceframe.hpp"
#include "./json.hpp"
#include "../registry.hpp"

namespace vadstena { namespace registry {

constexpr char Srs::typeName[];
constexpr char ReferenceFrame::typeName[];
constexpr char Credit::typeName[];
constexpr char BoundLayer::typeName[];

namespace {

constexpr int DEFAULT_RF_VERSION(1);

namespace v1 {

void parse(ReferenceFrame::Model &model, const Json::Value &content)
{
    Json::get(model.physicalSrs, content, "physicalSrs");
    Json::get(model.navigationSrs, content, "navigationSrs");
    Json::get(model.publicSrs, content, "publicSrs");
}


void parse(math::Extents3 &extents, const Json::Value &content)
{
    get(extents.ll(0), content, "ll", 0);
    get(extents.ll(1), content, "ll", 1);
    get(extents.ll(2), content, "ll", 2);
    get(extents.ur(0), content, "ur", 0);
    get(extents.ur(1), content, "ur", 1);
    get(extents.ur(2), content, "ur", 2);
}

void parse(math::Extents2 &extents, const Json::Value &content)
{
    get(extents.ll(0), content, "ll", 0);
    get(extents.ll(1), content, "ll", 1);
    get(extents.ur(0), content, "ur", 0);
    get(extents.ur(1), content, "ur", 1);
}

void parse(ReferenceFrame::Division::Node::Id &id
           , const Json::Value &content)
{
    get(id.lod, content, "lod");
    get(id.x, content, "position", 0);
    get(id.y, content, "position", 1);
}

void parse(ReferenceFrame::Division::Node::Partitioning &partitioning
           , const Json::Value &content)
{
    partitioning.mode = PartitioningMode::manual;

    math::Extents2 e;

    if (content.isMember("00")) {
        parse(e, check(content["00"], Json::objectValue));
        partitioning.n00 = e;
    }

    if (content.isMember("01")) {
        parse(e, check(content["01"], Json::objectValue));
        partitioning.n01 = e;
    }

    if (content.isMember("10")) {
        parse(e, check(content["10"], Json::objectValue));
        partitioning.n10 = e;
    }

    if (content.isMember("11")) {
        parse(e, check(content["11"], Json::objectValue));
        partitioning.n11 = e;
    }
}

void parse(ReferenceFrame::Division::Node &node, const Json::Value &content)
{
    const auto &id(content["id"]);
    if (!id.isObject()) {
        LOGTHROW(err1, Json::Error)
            << "Type of node[id] is not an object.";
    }
    parse(node.id, id);

    const auto &partitioning(content["partitioning"]);
    if (partitioning.isObject()) {
        // object
        parse(node.partitioning, partitioning);
    } else {
        // just mode
        std::string s;
        node.partitioning.mode
            = boost::lexical_cast<PartitioningMode>
            (Json::get(s, content, "partitioning"));
    }

    if (content.isMember("srs")) {
        Json::get(node.srs, content, "srs");
    }

    if (content.isMember("extents")) {
        const auto &extents(content["extents"]);
        if (!extents.isObject()) {
            LOGTHROW(err1, Json::Error)
                << "Type of node(" << node.id
                << ")[extents] is not an object.";
        }
        parse(node.extents, extents);
    }

    if (content.isMember("boundLayerLod")) {
        node.boundLayerLod = 0;
        Json::get(*node.boundLayerLod, content, "boundLayerLod");
    }
}

void parse(ReferenceFrame::Division &division, const Json::Value &content)
{
    const auto &extents(content["extents"]);
    if (!extents.isObject()) {
        LOGTHROW(err1, Json::Error)
            << "Type of division[extents] is not an object.";
    }
    parse(division.extents, extents);

    Json::get(division.rootLod, content, "rootLod");
    Json::get(division.arity, content, "arity");

    const auto &nodes(content["nodes"]);
    if (!nodes.isArray()) {
        LOGTHROW(err1, Json::Error)
            << "Type of division[nodes] is not a list.";
    }

    for (const auto &element : nodes) {
        ReferenceFrame::Division::Node node;
        parse(node, element);
        division.nodes.insert
            (ReferenceFrame::Division::Node::map::value_type
             (node.id, node));
    }
}

void parse(ReferenceFrame &rf, const Json::Value &content)
{
    Json::get(rf.id, content, "id");
    Json::get(rf.description, content, "description");

    const auto &model(content["model"]);
    if (!model.isObject()) {
        LOGTHROW(err1, Json::Error)
            << "Type of referenceframe[model] is not an object.";
    }
    parse(rf.model, model);

    const auto &division(content["division"]);
    if (!division.isObject()) {
        LOGTHROW(err1, Json::Error)
            << "Type of referenceframe[division] is not an object.";
    }
    parse(rf.division, division);

    const auto &parameters(content["parameters"]);
    if (!parameters.isObject()) {
        LOGTHROW(err1, Json::Error)
            << "Type of referenceframe[parameters] is not an object.";
    }
    Json::get(rf.metaBinaryOrder, parameters, "metaBinaryOrder");
    Json::get(rf.navDelta, parameters, "navDelta");
}

} // namespace v1

void parse(ReferenceFrame::dict &rfs, const Json::Value &content)
{
    for (const auto &element : Json::check(content, Json::arrayValue)) {
        ReferenceFrame rf;
        try {
            int version(0);
            Json::get(version, element, "version");

            switch (version) {
            case 1:
                v1::parse(rf, element);
                break;

            default:
                LOGTHROW(err1, storage::FormatError)
                    << "Invalid reference frame file format: "
                    "unsupported version "
                    << version << ".";
            }


        } catch (const Json::Error &e) {
            LOGTHROW(err1, storage::FormatError)
                << "Invalid reference frame file format (" << e.what()
                << ").";
        }
        rfs.add(rf);
    }
}

void build(Json::Value &content, const ReferenceFrame::Model &model)
{
    content = Json::objectValue;
    content["physicalSrs"] = model.physicalSrs;
    content["navigationSrs"] = model.navigationSrs;
    content["publicSrs"] = model.publicSrs;
}

void build(Json::Value &content, const math::Extents3 &extents)
{
    content = Json::objectValue;

    auto &ll(content["ll"] = Json::arrayValue);
    ll.append(extents.ll(0));
    ll.append(extents.ll(1));
    ll.append(extents.ll(2));

    auto &ur(content["ur"] = Json::arrayValue);
    ur.append(extents.ur(0));
    ur.append(extents.ur(1));
    ur.append(extents.ur(2));
}

void build(Json::Value &content, const math::Extents2 &extents)
{
    content = Json::objectValue;

    auto &ll(content["ll"] = Json::arrayValue);
    ll.append(extents.ll(0));
    ll.append(extents.ll(1));

    auto &ur(content["ur"] = Json::arrayValue);
    ur.append(extents.ur(0));
    ur.append(extents.ur(1));
}

void build(Json::Value &content, const ReferenceFrame::Division::Node::Id &id)
{
    content = Json::objectValue;
    content["lod"] = id.lod;
    auto &position(content["position"] = Json::arrayValue);
    position.append(id.x);
    position.append(id.y);
}

void build(Json::Value &content
           , const ReferenceFrame::Division::Node::Partitioning &partitioning)
{
    switch (partitioning.mode) {
    case PartitioningMode::none:
    case PartitioningMode::bisection:
        content = boost::lexical_cast<std::string>(partitioning.mode);
        return;

    default: break;
    }

    // manual
    content = Json::objectValue;

    if (partitioning.n00) { build(content["00"], *partitioning.n00); }
    if (partitioning.n01) { build(content["01"], *partitioning.n01); }
    if (partitioning.n10) { build(content["10"], *partitioning.n10); }
    if (partitioning.n11) { build(content["11"], *partitioning.n11); }
}

void build(Json::Value &content, const ReferenceFrame::Division::Node &node)
{
    content = Json::objectValue;

    build(content["id"], node.id);
    build(content["partitioning"], node.partitioning);

    if (node.partitioning.mode != PartitioningMode::none) {
        content["srs"] = node.srs;
        build(content["extents"], node.extents);
    }

    if (node.boundLayerLod) {
        content["boundLayerLod"] = *node.boundLayerLod;
    }
}

void build(Json::Value &content, const ReferenceFrame::Division &division)
{
    content = Json::objectValue;

    build(content["extents"], division.extents);
    content["rootLod"] = division.rootLod;
    content["arity"] = division.arity;

    auto &nodes(content["nodes"]);
    for (const auto &node : division.nodes) {
        build(nodes.append(Json::nullValue), node.second);
    }
}

void build(Json::Value &content, const ReferenceFrame &rf)
{
    content = Json::objectValue;
    content["version"] = DEFAULT_RF_VERSION;

    content["id"] = rf.id;
    content["description"] = rf.description;
    build(content["model"], rf.model);
    build(content["division"], rf.division);

    auto &parameters(content["parameters"] = Json::objectValue);
    parameters["metaBinaryOrder"] = rf.metaBinaryOrder;
    parameters["navDelta"] = rf.navDelta;
}

void build(Json::Value &content, const ReferenceFrame::dict &rfs)
{
    content = Json::arrayValue;

    for (const auto &rf : rfs) {
        build(content.append(Json::nullValue), rf.second);
    }
}

void parse(Srs &srs, const Json::Value &content)
{
    Json::get(srs.comment, content, "comment");

    // just mode
    std::string s;
    srs.type = boost::lexical_cast<Srs::Type>(Json::get(s, content, "type"));
    srs.srsDef = { Json::get(s, content, "srsDef")
                   , geo::SrsDefinition::Type::proj4};
    if (content.isMember("srsDefEllps")) {
        srs.srsDefEllps = { Json::get(s, content, "srsDefEllps")
                            , geo::SrsDefinition::Type::proj4};
    }

    // parse modifiers
    if (content.isMember("srsModifiers")) {
        for (const auto &modifier
                 : Json::check(content["srsModifiers"], Json::arrayValue))
        {
            s = Json::as<std::string>(modifier, "srsModifiers");
            if (s == "adjustVertical") {
                srs.srsModifiers |= Srs::Modifiers::adjustVertical;
            } else {
                LOGTHROW(err1, Json::Error)
                    << "Invalid value " << s << " in srsModifiers.";
            }
        }
    }

    if (content.isMember("sphereoid")) {
        const auto &sphereoid
            (Json::check(content["sphereoid"], Json::objectValue));
        srs.sphereoid = Sphereoid();
        Json::get(srs.sphereoid->a, sphereoid, "a");
        Json::get(srs.sphereoid->b, sphereoid, "b");
    }

    if (content.isMember("vdatum")) {
        srs.vdatum = boost::lexical_cast<VerticalDatum>
            (Json::get(s, content, "vdatum"));
    }
}

void parse(Srs::dict &srs, const Json::Value &content)
{
    for (const auto &id : Json::check(content, Json::objectValue)
             .getMemberNames())
    {
        try {
            Srs s;
            parse(s, Json::check(content[id], Json::objectValue));
            srs.set(id, s);
        } catch (const Json::Error &e) {
            LOGTHROW(err1, storage::FormatError)
                << "Invalid srs file format (" << e.what()
                << ").";
        }
    }
}

void build(Json::Value &content, const Srs &srs)
{
    content = Json::objectValue;
    content["comment"] = srs.comment;
    content["type"] = boost::lexical_cast<std::string>(srs.type);
    content["srsDef"] = srs.srsDef.as(geo::SrsDefinition::Type::proj4).srs;
    if (srs.srsDefEllps) {
        content["srsDefEllps"]
            = srs.srsDefEllps->as(geo::SrsDefinition::Type::proj4).srs;
    }

    if (srs.sphereoid) {
        auto &sphereoid(content["sphereoid"] = Json::objectValue);
        sphereoid["a"] = srs.sphereoid->a;
        sphereoid["b"] = srs.sphereoid->b;
    }

    if (srs.vdatum) {
        content["vdatum"] = boost::lexical_cast<std::string>(*srs.vdatum);
    }

    if (srs.srsModifiers) {
        auto &srsModifiers(content["srsModifiers"] =  Json::arrayValue);
        if (srs.srsModifiers & Srs::Modifiers::adjustVertical) {
            srsModifiers.append("adjustVertical");
        }
    }
}

void build(Json::Value &content, const Srs::dict &srs)
{
    content = Json::objectValue;

    for (const auto &s : srs) {
        build(content[s.first], s.second);
    }
}

void parse(BoundLayer &bl, const Json::Value &content)
{
    Json::get(bl.numericId, content, "id");

    std::string s;
    bl.type = boost::lexical_cast<BoundLayer::Type>
        (Json::get(s, content, "type"));

    Json::get(bl.url, content, "url");
    Json::get(bl.tileSize.width, content, "tileSize", 0);
    Json::get(bl.tileSize.height, content, "tileSize", 1);
    Json::get(bl.lodRange.min, content, "lodRange", 0);
    Json::get(bl.lodRange.max, content, "lodRange", 1);

    const auto &tileRange(content["tileRange"]);
    if (!tileRange.isArray()) {
        LOGTHROW(err1, Json::Error)
            << "Type of boundLayer[tileRange] is not a list.";
    }
    if (tileRange.size() != 2) {
        LOGTHROW(err1, Json::Error)
            << "boundLayer[tileRange] must have two elements.";
    }
    Json::get(bl.tileRange.ll(0), tileRange[0], 0, "tileRange[0][0]");
    Json::get(bl.tileRange.ll(1), tileRange[0], 1, "tileRange[0][1]");
    Json::get(bl.tileRange.ur(0), tileRange[1], 0, "tileRange[1][0]");
    Json::get(bl.tileRange.ur(1), tileRange[1], 1, "tileRange[1][1]");

    const auto &credits(content["credits"]);
    if (!credits.isArray()) {
        LOGTHROW(err1, Json::Error)
            << "Type of boundLayer[credits] is not a list.";
    }

    for (const auto &element : credits) {
        bl.credits.insert(element.asString());
    }
}

void parse(BoundLayer::dict &bls, const Json::Value &content)
{
    for (const auto &id : Json::check(content, Json::objectValue)
             .getMemberNames())
    {
        try {
            BoundLayer bl;
            bl.id = id;
            parse(bl, Json::check(content[id], Json::objectValue));
            bls.set(id, bl);
        } catch (const Json::Error &e) {
            LOGTHROW(err1, storage::FormatError)
                << "Invalid srs file format (" << e.what()
                << ").";
        }
    }
}

void build(Json::Value &content, const BoundLayer &bl)
{
    content = Json::objectValue;
    content["id"] = bl.numericId;
    content["type"] = boost::lexical_cast<std::string>(bl.type);
    content["url"] = bl.url;

    auto &tileSize(content["tileSize"] = Json::arrayValue);
    tileSize.append(bl.tileSize.width);
    tileSize.append(bl.tileSize.height);

    auto &lodRange(content["lodRange"] = Json::arrayValue);
    lodRange.append(bl.lodRange.min);
    lodRange.append(bl.lodRange.max);

    auto &tileRange(content["tileRange"] = Json::arrayValue);
    auto &tileRangeMin(tileRange.append(Json::arrayValue));
    tileRangeMin.append(bl.tileRange.ll(0));
    tileRangeMin.append(bl.tileRange.ll(1));
    auto &tileRangeMax(tileRange.append(Json::arrayValue));
    tileRangeMax.append(bl.tileRange.ur(0));
    tileRangeMax.append(bl.tileRange.ur(1));

    auto &credits(content["credits"] = Json::arrayValue);
    for (const auto &credit : bl.credits) {
        credits.append(credit);
    };
}

void build(Json::Value &content, const BoundLayer::dict &bls)
{
    content = Json::objectValue;

    for (const auto &bl : bls) {
        build(content[bl.first], bl.second);
    }
}

void parse(Credit &c, const Json::Value &content)
{
    Json::get(c.numericId, content, "id");

    Json::get(c.notice, content, "notice");
    if (content.isMember("url")) {
        c.url = std::string();
        Json::get(*c.url, content, "url");
    }
    if (content.isMember("copyrighted")) {
        get(c.copyrighted, content, "copyrighted");
    } else {
        c.copyrighted = false;
    }
}

void parse(Credit::dict &credits, const Json::Value &content)
{
    for (const auto &id : Json::check(content, Json::objectValue)
             .getMemberNames())
    {
        try {
            Credit c;
            c.id = id;
            parse(c, Json::check(content[id], Json::objectValue));
            credits.set(id, c);
        } catch (const Json::Error &e) {
            LOGTHROW(err1, storage::FormatError)
                << "Invalid credits file format (" << e.what()
                << ").";
        }
    }
}

void build(Json::Value &content, const Credit &c)
{
    content = Json::objectValue;
    content["id"] = c.numericId;
    content["notice"] = c.notice;
    if (c.url) { content["url"] = *c.url; }
    if (!c.copyrighted) { content["url"] = c.copyrighted; }
}

void build(Json::Value &content, const Credit::dict &credits)
{
    content = Json::objectValue;

    for (const auto &c : credits) {
        build(content[c.first], c.second);
    }
}

} // namesapce

ReferenceFrame::dict loadReferenceFrames(std::istream &in)
{
    // load json
    Json::Value content;
    Json::Reader reader;
    if (!reader.parse(in, content)) {
        LOGTHROW(err2, storage::FormatError)
            << "Unable to parse reference frame file: "
            << reader.getFormattedErrorMessages() << ".";
    }

    ReferenceFrame::dict rfs;
    parse(rfs, content);
    return rfs;
}

ReferenceFrame::dict loadReferenceFrames(const boost::filesystem::path &path)
{
    LOG(info1) << "Loading reference frame file from " << path  << ".";
    std::ifstream f;
    f.exceptions(std::ios::badbit | std::ios::failbit);
    try {
        f.open(path.string(), std::ios_base::in);
    } catch (const std::exception &e) {
        LOGTHROW(err1, storage::IOError)
            << "Unable to load reference frame file file " << path << ".";
    }
    auto rfs(loadReferenceFrames(f));
    f.close();
    return rfs;
}

void saveReferenceFrames(std::ostream &out
                         , const ReferenceFrame::dict &rfs)
{
    Json::Value content;
    build(content, rfs);
    out.precision(15);
    Json::StyledStreamWriter().write(out, content);
}

void saveReferenceFrames(const boost::filesystem::path &path
                         , const ReferenceFrame::dict &rfs)
{
    LOG(info1) << "Saving reference frame file file to " << path  << ".";
    std::ofstream f;
    try {
        f.exceptions(std::ios::badbit | std::ios::failbit);
        f.open(path.string(), std::ios_base::out);
    } catch (const std::exception &e) {
        LOGTHROW(err1, storage::IOError)
            << "Unable to save reference frame file file " << path << ".";
    }
    saveReferenceFrames(f, rfs);
    f.close();
}

Srs::dict loadSrs(std::istream &in)
{
    // load json
    Json::Value content;
    Json::Reader reader;
    if (!reader.parse(in, content)) {
        LOGTHROW(err2, storage::FormatError)
            << "Unable to parse srs file: "
            << reader.getFormattedErrorMessages() << ".";
    }

    Srs::dict srs;
    parse(srs, content);
    return srs;
}

Srs::dict loadSrs(const boost::filesystem::path &path)
{
    LOG(info1) << "Loading srs file from " << path  << ".";
    std::ifstream f;
    f.exceptions(std::ios::badbit | std::ios::failbit);
    try {
        f.open(path.string(), std::ios_base::in);
    } catch (const std::exception &e) {
        LOGTHROW(err1, storage::IOError)
            << "Unable to load srs file file " << path << ".";
    }
    auto srs(loadSrs(f));
    f.close();
    return srs;
}

void saveSrs(std::ostream &out, const Srs::dict &srs)
{
    Json::Value content;
    build(content, srs);
    out.precision(15);
    Json::StyledStreamWriter().write(out, content);
}

void saveSrs(const boost::filesystem::path &path
             , const Srs::dict &srs)
{
    LOG(info1) << "Saving srs file file to " << path  << ".";
    std::ofstream f;
    try {
        f.exceptions(std::ios::badbit | std::ios::failbit);
        f.open(path.string(), std::ios_base::out);
    } catch (const std::exception &e) {
        LOGTHROW(err1, storage::IOError)
            << "Unable to save srs file file " << path << ".";
    }
    saveSrs(f, srs);
    f.close();
}

BoundLayer::dict loadBoundLayers(std::istream &in)
{
    // load json
    Json::Value content;
    Json::Reader reader;
    if (!reader.parse(in, content)) {
        LOGTHROW(err2, storage::FormatError)
            << "Unable to parse bound layers file: "
            << reader.getFormattedErrorMessages() << ".";
    }

    BoundLayer::dict boundLayers;
    parse(boundLayers, content);
    return boundLayers;
}

BoundLayer::dict loadBoundLayers(const boost::filesystem::path &path)
{
    LOG(info1) << "Loading bound layers file from " << path  << ".";
    std::ifstream f;
    f.exceptions(std::ios::badbit | std::ios::failbit);
    try {
        f.open(path.string(), std::ios_base::in);
    } catch (const std::exception &e) {
        LOGTHROW(err1, storage::IOError)
            << "Unable to load bound layer file file " << path << ".";
    }
    auto boundLayers(loadBoundLayers(f));
    f.close();
    return boundLayers;
}

void saveBoundLayers(std::ostream &out, const BoundLayer::dict &boundLayers)
{
    Json::Value content;
    build(content, boundLayers);
    out.precision(15);
    Json::StyledStreamWriter().write(out, content);
}

void saveBoundLayers(const boost::filesystem::path &path
                     , const BoundLayer::dict &boundLayers)
{
    LOG(info1) << "Saving bound layers file file to " << path  << ".";
    std::ofstream f;
    try {
        f.exceptions(std::ios::badbit | std::ios::failbit);
        f.open(path.string(), std::ios_base::out);
    } catch (const std::exception &e) {
        LOGTHROW(err1, storage::IOError)
            << "Unable to save bound Layers file file " << path << ".";
    }
    saveBoundLayers(f, boundLayers);
    f.close();
}

Credit::dict loadCredits(std::istream &in)
{
    // load json
    Json::Value content;
    Json::Reader reader;
    if (!reader.parse(in, content)) {
        LOGTHROW(err2, storage::FormatError)
            << "Unable to parse credits file: "
            << reader.getFormattedErrorMessages() << ".";
    }

    Credit::dict credits;
    parse(credits, content);
    return credits;
}

Credit::dict loadCredits(const boost::filesystem::path &path)
{
    LOG(info1) << "Loading credits file from " << path  << ".";
    std::ifstream f;
    f.exceptions(std::ios::badbit | std::ios::failbit);
    try {
        f.open(path.string(), std::ios_base::in);
    } catch (const std::exception &e) {
        LOGTHROW(err1, storage::IOError)
            << "Unable to load credits from file " << path << ".";
    }
    auto credits(loadCredits(f));
    f.close();
    return credits;
}

void saveCredits(std::ostream &out, const Credit::dict &credits)
{
    Json::Value content;
    build(content, credits);
    out.precision(15);
    Json::StyledStreamWriter().write(out, content);
}

void saveCredits(const boost::filesystem::path &path
                     , const Credit::dict &credits)
{
    LOG(info1) << "Saving credits to file " << path  << ".";
    std::ofstream f;
    try {
        f.exceptions(std::ios::badbit | std::ios::failbit);
        f.open(path.string(), std::ios_base::out);
    } catch (const std::exception &e) {
        LOGTHROW(err1, storage::IOError)
            << "Unable to save credits to file " << path << ".";
    }
    saveCredits(f, credits);
    f.close();
}

const ReferenceFrame::Division::Node*
ReferenceFrame::Division::find(const Node::Id &id, std::nothrow_t) const
{
    auto fnodes(nodes.find(id));
    if (fnodes == nodes.end()) { return nullptr; }
    return &fnodes->second;
}

const ReferenceFrame::Division::Node&
ReferenceFrame::Division::find(const Node::Id &id) const
{
    const auto *node(find(id, std::nothrow));
    if (!node) {
        LOGTHROW(err1, storage::KeyError)
            << "No node <" << id << "> in division tree.";
    }
    return *node;
}

math::Extents2 ReferenceFrame::rootExtents() const
{
    return division.find({}).extents;
}

math::Size2f ReferenceFrame::tileSize(Lod lod) const
{
    auto ts(size(rootExtents()));
    ts.width /= (1 << lod);
    ts.height /= (1 << lod);
    return ts;
}

std::string ReferenceFrame::rootSrs() const
{
    return division.find({}).srs;
}

Json::Value asJson(const ReferenceFrame &rf)
{
    Json::Value content;
    build(content, rf);
    return content;
}

Json::Value asJson(const Srs::dict &srs)
{
    Json::Value content;
    build(content, srs);
    return content;
}

Json::Value asJson(const Credit::dict &credits)
{
    Json::Value content;
    build(content, credits);
    return content;
}

Json::Value asJson(const BoundLayer::dict &boundLayers)
{
    Json::Value content;
    build(content, boundLayers);
    return content;
}

Json::Value asJson(const Position &position)
{
    Json::Value value(Json::arrayValue);
    value.append(boost::lexical_cast<std::string>(position.type));
    value.append(position.position(0));
    value.append(position.position(1));
    value.append(position.position(2));
    value.append(position.orientation(0));
    value.append(position.orientation(1));
    value.append(position.orientation(2));
    if (position.type != Position::Type::generic) {
        value.append(position.verticalExtent);
    }
    value.append(position.verticalFov);
    return value;
}

Position fromJson(const Json::Value value)
{
    Position p;

    if (!value.isArray()) {
        LOGTHROW(err1, Json::Error)
            << "Type of position is not a list.";
    }

    p.type = boost::lexical_cast<registry::Position::Type>
        (Json::as<std::string>(value[0]));
    p.position(0) = Json::as<double>(value[1]);
    p.position(1) = Json::as<double>(value[2]);
    p.position(2) = Json::as<double>(value[3]);

    p.orientation(0) = Json::as<double>(value[4]);
    p.orientation(1) = Json::as<double>(value[5]);
    p.orientation(2) = Json::as<double>(value[6]);

    if (p.type == Position::Type::generic) {
        p.verticalFov = Json::as<double>(value[7]);
    } else {
        p.verticalExtent = Json::as<double>(value[7]);
        p.verticalFov = Json::as<double>(value[8]);
    }

    return p;
}

Srs::dict listSrs(const ReferenceFrame &referenceFrame)
{
    Srs::dict srs;

    auto add([&](const std::string &id)
    {
        srs.set(id, Registry::srs(id));
    });

    add(referenceFrame.model.physicalSrs);
    add(referenceFrame.model.navigationSrs);
    add(referenceFrame.model.publicSrs);

    for (const auto &node : referenceFrame.division.nodes) {
        add(node.second.srs);
    }

    return srs;
}

Credit::dict creditsAsDict(const StringIdSet &credits)
{
    Credit::dict c;
    for (const auto &id : credits) {
        c.add(Registry::credit(id));
    };
    return c;
}

Credit::dict creditsAsDict(const IdSet &credits)
{
    Credit::dict c;
    for (const auto &id : credits) {
        c.add(Registry::credit(id));
    };
    return c;
}

BoundLayer::dict boundLayersAsDict(const StringIdSet &boundLayers)
{
    BoundLayer::dict b;
    for (const auto &id : boundLayers) {
        b.add(Registry::boundLayer(id));
    };
    return b;
}

BoundLayer::dict boundLayersAsDict(const IdSet &boundLayers)
{
    BoundLayer::dict b;
    for (const auto &id : boundLayers) {
        b.add(Registry::boundLayer(id));
    };
    return b;
}

const ReferenceFrame::Division::Node&
ReferenceFrame::Division::findSubtreeRoot(const Node::Id &nodeId) const
{
    LOG(info4) << "Finding subtree root for node " << nodeId;
    const Node *candidate(nullptr);
    for (const auto &item : nodes) {
        const auto &treeRoot(item.first);

        if (candidate && (treeRoot.lod <= candidate->id.lod )) {
            // this root would be above or at already found candidate root ->
            // impossible
            continue;
        }

        if (treeRoot.lod > nodeId.lod) {
            // root cannot be bellow node
            continue;
        }

        if (treeRoot == nodeId) {
            // exact match -> standing at the root
            return item.second;
        }

        // node bellow possible root -> check if it can be a candidate
        auto diff(nodeId.lod - treeRoot.lod);
        Node::Id::index_type x(nodeId.x >> diff), y(nodeId.y >> diff);
        if ((treeRoot.x == x) && (treeRoot.y == y)) {
            // match -> nodeId is bellow treeRoot -> remember
            candidate = &item.second;
        }
    }

    if (!candidate) {
        LOGTHROW(err1, storage::KeyError)
            << "Node <" << nodeId << "> has no root in this reference frame.";
    }

    return *candidate;
}

} } // namespace vadstena::registry
