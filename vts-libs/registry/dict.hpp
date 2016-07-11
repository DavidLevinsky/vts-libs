/**
 * \file registry/dict.hpp
 * \author Vaclav Blazek <vaclav.blazek@citationtech.net>
 */

#ifndef vadstena_libs_registry_dict_hpp_included_
#define vadstena_libs_registry_dict_hpp_included_

#include <map>
#include <string>
#include <algorithm>

#include "./dict.hpp"

namespace vadstena { namespace registry {

template <typename T, typename Key = std::string>
class Dictionary
{
private:
    typedef Key key_type;
    typedef std::map<Key, T> map;

public:
    Dictionary() {}

    void set(const key_type &id, const T &value);
    void replace(const key_type &id, const T &value);
    const T* get(const key_type &id, std::nothrow_t) const;
    const T& get(const key_type &id) const;
    bool has(const key_type &id) const;

    inline void add(const T &value) { set(value.id, value); }
    inline void add(const T *value) { if (value) { add(*value); } }
    inline void replace(const T &value) { replace(value.id, value); }

    typedef typename map::value_type value_type;
    typedef typename map::const_iterator const_iterator;
    const_iterator begin() const { return map_.begin(); }
    const_iterator end() const { return map_.end(); }

    void update(const Dictionary &other);

    template <typename Op> void for_each(Op op)
    {
        std::for_each(map_.begin(), map_.end()
                      , [&](typename map::value_type &pair) {
                          op(pair.second);
                      });
    }

    template <typename Op> void for_each(Op op) const
    {
        std::for_each(map_.begin(), map_.end()
                      , [&](const typename map::value_type &pair) {
                          op(pair.second);
                      });
    }

    bool empty() const { return map_.empty(); }

private:
    map map_;
};

template <typename T, typename Key>
void Dictionary<T, Key>::set(const key_type &id, const T &value)
{
    map_.insert(typename map::value_type(id, value));
}

template <typename T, typename Key>
void Dictionary<T, Key>::replace(const key_type &id, const T &value)
{
    auto res(map_.insert(typename map::value_type(id, value)));
    if (!res.second) {
        // already present, replace
        res.first->second = value;
    }
}

template <typename T, typename Key>
const T* Dictionary<T, Key>::get(const key_type &id, std::nothrow_t) const
{
    auto fmap(map_.find(id));
    if (fmap == map_.end()) { return nullptr; }
    return &fmap->second;
}

template <typename T, typename Key>
const T& Dictionary<T, Key>::get(const key_type &id) const
{
    const auto *value(get(id, std::nothrow));
    if (!value) {
        LOGTHROW(err1, storage::KeyError)
            << "<" << id << "> is not known " << T::typeName << ".";
    }
    return *value;
}

template <typename T, typename Key>
bool Dictionary<T, Key>::has(const key_type &id) const
{
    return (map_.find(id) != map_.end());
}

template <typename T, typename Key>
void Dictionary<T, Key>::update(const Dictionary &other)
{
    for (const auto &item : other) { map_.insert(item); }
}

} } // namespace vadstena::registry

#endif // vadstena_libs_registry_dict_hpp_included_
