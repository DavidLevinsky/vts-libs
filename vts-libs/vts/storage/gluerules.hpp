/**
 * \file vts/storage/gluerules.hpp
 * \author Vaclav Blazek <vaclav.blazek@citationtech.net>
 *
 * Tile set glue creation rules
 */

#ifndef vadstena_libs_vts_storage_gluerules_hpp_included_
#define vadstena_libs_vts_storage_gluerules_hpp_included_

#include <memory>

#include "../storage.hpp"

namespace vadstena { namespace vts {

/** Glue creation rule.
 */
class GlueRule {
public:
    typedef std::shared_ptr<GlueRule> pointer;
    typedef std::vector<pointer> list;

    virtual ~GlueRule() {}

    class MatcherBase {
    public:
        typedef std::shared_ptr<MatcherBase> pointer;
        typedef std::vector<pointer> list;

        virtual ~MatcherBase() {}

        /** Checks whether given tag set is allowed.
         */
        bool check(const StoredTileset &tileset);

    private:
        virtual bool check_impl(const StoredTileset &tileset) = 0;
    };

    /** Generate Matcher
     */
    MatcherBase::pointer matcher() const;

    /** Dump information into stream. Should follow the same representation as
     *  is parsed from input.
     */
    std::ostream& dump(std::ostream &os, const std::string &prefix) const;

private:
    virtual MatcherBase::pointer matcher_impl() const = 0;

    virtual std::ostream& dump_impl(std::ostream &os
                                    , const std::string &prefix) const = 0;
};

/** Iterative glue rule checker.
 */
class GlueRuleChecker {
public:
    /** Creates glue rule checker.
     */
    GlueRuleChecker(const GlueRule::list &rules);

    /** Check whether this tileset can be added to glue.
     */
    bool operator()(const StoredTileset &tileset) const;

private:
    GlueRule::MatcherBase::list matchers_;
};

/** Check whether given list of stored tilesets can be glued together.
 */
bool check(const GlueRule::list &rules
           , const StoredTileset::constptrlist &tilesets);

/** Load glue rules file.
 *
 * \param path path to glue rules file
 * \param ignoreNoexistent ignore non-existent file (return empty list)
 * \return list of parsed rules
 */
GlueRule::list loadGlueRules(const boost::filesystem::path &path
                             , bool ignoreNoexistent = false);

// inlines

inline GlueRule::MatcherBase::pointer GlueRule::matcher() const
{
    return matcher_impl();
}

inline std::ostream& GlueRule::dump(std::ostream &os
                                    , const std::string &prefix) const
{
    return dump_impl(os, prefix);
}

inline bool GlueRule::MatcherBase::check(const StoredTileset &tags)
{
    return check_impl(tags);
}

} } // namespace vadstena::vts

#endif // vadstena_libs_vts_storage_gluerules_hpp_included_
