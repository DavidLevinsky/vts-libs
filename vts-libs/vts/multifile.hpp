#ifndef vadstena_libs_vts_multifile_hpp
#define vadstena_libs_vts_multifile_hpp

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <istream>
#include <vector>

#include <boost/filesystem/path.hpp>

namespace vadstena { namespace vts { namespace multifile {

struct Table {
    struct Entry {
        std::size_t start;
        std::size_t size;

        Entry(std::size_t start = 0, std::size_t size = 0)
            : start(start), size(size)
        {}
    };

    typedef std::vector<Entry> Entries;

    Entries entries;
    std::uint16_t version;
    std::string magic;

    Table(std::uint16_t version = 1, const std::string &magic = "")
        : version(version), magic(magic)
    {}

    Table& set(std::uint16_t version, const std::string &magic) {
        this->version = version;
        this->magic = magic;
        return *this;
    }

    const Table& versionAtMost(std::uint16_t version
                               , const boost::filesystem::path &path) const;

    const Table& checkEntryCount(Entries::size_type count
                                 , const boost::filesystem::path &path) const;

    typedef Entries::const_iterator const_iterator;
    typedef Entries::iterator iterator;
    const_iterator begin() const { return entries.begin(); }
    const_iterator end() const { return entries.end(); }
    const_iterator cbegin() { return entries.begin(); }
    const_iterator cend() { return entries.end(); }
    iterator begin() { return entries.begin(); }
    iterator end() { return entries.end(); }

    Entries::size_type size() const { return entries.size(); }

    const Entry& operator[](Entries::size_type i) const { return entries[i]; }
};

Table readTable(std::istream &is, const std::string &expectMagic
                , const boost::filesystem::path &path
                = "unknown");

void writeTable(const Table &table, std::ostream &os);

} } } // namespace vadstena::vts::multifile

#endif // vadstena_libs_vts_multifile_hpp
