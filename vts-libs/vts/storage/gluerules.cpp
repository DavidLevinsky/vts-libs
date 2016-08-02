#include <fnmatch.h>

#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_object.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/spirit/include/phoenix_fusion.hpp>
#include <boost/spirit/include/phoenix_container.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/io.hpp>
#include <boost/spirit/include/qi_match.hpp>
#include <boost/spirit/include/qi_match_auto.hpp>
#include <boost/spirit/include/qi_stream.hpp>

#include "./gluerules.hpp"

namespace vadstena { namespace vts {

GlueRuleChecker::GlueRuleChecker(const GlueRule::list &rules)
{
    // buils list of matchers
    for (const auto &rule : rules) {
        matchers_.emplace_back(rule->matcher());
    }
}

bool GlueRuleChecker::operator()(const StoredTileset &tileset) const
{
    for (const auto &matcher : matchers_) {
        if (!matcher->check(tileset)) {
            // check failed
            return false;
        }
    }

    // no obstacle
    return true;
}

bool check(const GlueRule::list &rules
           , const StoredTileset::constptrlist &tilesets)
{
    GlueRuleChecker checker(rules);

    for (const auto &tileset : tilesets) {
        if (!checker(*tileset)) { return false; }
    }

    // no obstacle
    return true;
}

namespace rules {

class UniqueTag
    : public GlueRule
    , public std::enable_shared_from_this<UniqueTag>
{
public:
    typedef std::shared_ptr<const UniqueTag> ThisConstRule;

    UniqueTag(std::string tag) : tag_(std::move(tag)) {}

    static pointer factory(std::string tag) {
        return std::make_shared<UniqueTag>(std::move(tag));
    }

    static const char *name() { return "unique-tag"; }

private:
    struct Matcher : public MatcherBase {
        Matcher(const ThisConstRule &rule): rule(rule), count() {}

        virtual bool check_impl(const StoredTileset &tileset) {
            count += tileset.tags.count(rule->tag_);
            return (count < 2);
        }

        ThisConstRule rule;
        std::size_t count;
    };
    friend struct Matcher;

    virtual MatcherBase::pointer matcher_impl() const {
        return std::make_shared<Matcher>(shared_from_this());
    }

    virtual std::ostream& dump_impl(std::ostream &os
                                    , const std::string &prefix) const
    {
        return os << prefix << name() << '(' << tag_ << ')';
    }

    const std::string tag_;
};

class UniqueTagMatch
    : public GlueRule
    , public std::enable_shared_from_this<UniqueTagMatch>
{
public:
    typedef std::shared_ptr<const UniqueTagMatch> ThisConstRule;

    UniqueTagMatch(std::string pattern) : pattern_(std::move(pattern)) {}

    static pointer factory(std::string pattern) {
        return std::make_shared<UniqueTagMatch>(std::move(pattern));
    }

    static const char *name() { return "unique-tag-match"; }

private:
    struct Matcher : public MatcherBase {
        Matcher(const ThisConstRule &rule) : rule(rule)  {}

        virtual bool check_impl(const StoredTileset &tileset) {
            for (const auto &tag : tileset.tags) {
                if (!::fnmatch(rule->pattern_.c_str(), tag.c_str(), 0)) {
                    // pattern matched
                    if (match.empty()) {
                        // first match, remember
                        match = tag;
                        continue;
                    }

                    if (match != tag) {
                        // patter matches different tag -> error
                        return false;
                    }
                }
            }
            return true;
        }

        ThisConstRule rule;
        std::string match;
    };
    friend struct Matcher;

    virtual MatcherBase::pointer matcher_impl() const {
        return std::make_shared<Matcher>(shared_from_this());
    }

    virtual std::ostream& dump_impl(std::ostream &os
                                    , const std::string &prefix) const
    {
        return os << prefix << name() << '(' << pattern_ << ')';
    }

    const std::string pattern_;
};

} // namespace rules

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;

namespace rule_parser {

struct SingleTagRule {
    typedef std::function<GlueRule::pointer(std::string)> Factory;

    Factory factory;
    std::string arg;
};

class RuleGenerator {
public:
    RuleGenerator(GlueRule::list &rules)
        : rules_(rules)
    {}

    RuleGenerator& operator+=(const SingleTagRule &r) {
        rules_.push_back(r.factory(r.arg));
        return *this;
    }

private:
    GlueRule::list &rules_;
};

// close namespace to inject fusion adapters
} } } // namespace vadstena::vts::rule_parser

BOOST_FUSION_ADAPT_STRUCT(
    ::vadstena::vts::rule_parser::SingleTagRule,
    (::vadstena::vts::rule_parser::SingleTagRule::Factory, factory)
    (std::string, arg)
)

namespace vadstena { namespace vts { namespace rule_parser {
// reopen namespace after fusion adapters injection

template <typename Iterator, typename Skipper>
struct single_tag_rule_parser : qi::grammar<Iterator, SingleTagRule(), Skipper>
{
    single_tag_rule_parser() : single_tag_rule_parser::base_type(start) {
        // fill in symbol table
        symbols.add
            (rules::UniqueTag::name()
             , &rules::UniqueTag::factory)
            (rules::UniqueTagMatch::name()
             , &rules::UniqueTagMatch::factory)
            ;

        start %= symbols >> qi::omit[qi::char_('(')]
                         >> qi::lexeme[+(qi::alnum
                                         | qi::char_('-')
                                         | qi::char_('.')
                                         | qi::char_('_')
                                         | qi::char_('*')
                                         | qi::char_('?'))]
                         >> qi::omit[qi::char_(')')];
    }

    qi::symbols<char, SingleTagRule::Factory> symbols;
    qi::rule<Iterator, SingleTagRule(), Skipper> start;
};

template <typename Iterator>
struct skipper : qi::grammar<Iterator>
{
    typedef skipper<Iterator> type;

    skipper() : skipper::base_type(start)  {
        comment %= '#' >> *(qi::char_ - qi::char_("\r\n"))
                       >> (qi::eol | qi::eoi);

        start %= ascii::space | comment;
    }

    qi::rule<Iterator> start;
    qi::rule<Iterator> comment;
};

template <typename Iterator, typename Skipper>
struct Rule_parser : qi::grammar<Iterator, RuleGenerator(), Skipper>
{
    Rule_parser() : Rule_parser::base_type(start)  {
        using qi::omit;

        single_tag_rule %= single_tag_rule_parser<Iterator, Skipper>();

        start %= omit[*(single_tag_rule[qi::_val += qi::_1]
                        )]
            [qi::_val];
    }

    qi::rule<Iterator, RuleGenerator(), Skipper> start;

    single_tag_rule_parser<Iterator, Skipper> single_tag_rule;
};

template <typename Iterator>
bool parse(Iterator begin, Iterator end, GlueRule::list &rules)
{
    typedef skipper<Iterator> skipper_type;

    Rule_parser<Iterator, skipper_type> qrammar;
    RuleGenerator rg(rules);

    bool r(phrase_parse(begin, end, qrammar, skipper_type(), rg));

    return (r && (begin == end));
}

template<typename CharT, typename Traits>
bool parse(std::basic_istream<CharT, Traits> &is
           , GlueRule::list &rules)
{
    is.unsetf(std::ios::skipws);
    typedef boost::spirit::istream_iterator iterator_type;
    return parse(iterator_type(is), iterator_type(), rules);
}

} // namespace rule_parser

GlueRule::list loadGlueRules(std::istream &in
                             , const boost::filesystem::path &path)
{
    GlueRule::list rules;
    if (!rule_parser::parse(in, rules)) {
        LOGTHROW(err2, std::runtime_error)
            << "Unable to parse glue rules file " << path << ".";
    }

    return rules;
}

GlueRule::list loadGlueRules(const boost::filesystem::path &path
                             , bool ignoreNoexistent)
{
    LOG(info1) << "Loading glue rules from " << path  << ".";
    std::ifstream f;
    f.exceptions(std::ios::badbit | std::ios::failbit);
    try {
        f.open(path.string(), std::ios_base::in);
        f.peek();
    } catch (const std::exception &e) {
        if (ignoreNoexistent) { return {}; }

        LOGTHROW(err1, vadstena::storage::NoSuchStorage)
            << "Unable to load glue rules from " << path << ".";
    }
    auto rules(loadGlueRules(f, path));

    f.close();
    return rules;
}

} } // namespace vadstena::vts
