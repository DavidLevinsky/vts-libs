#include <algorithm>

#include <boost/range/adaptor/reversed.hpp>

#include "./glue.hpp"
#include "./tileop.hpp"

namespace vadstena { namespace vts {

TileSetGlues::list glueOrder(const TileSetGlues::list &in)
{
    // creata tileset alphabet
    std::map<TilesetId, int> alphabet;
    {
        int d(0);
        for (const auto &tsg : boost::adaptors::reverse(in)) {
            alphabet[tsg.tilesetId] = d;
            LOG(info2) << "Depth <" << tsg.tilesetId << ">: " << d << ".";
            ++d;
        }
    }

    // build tileset stack
    TilesetIdList stack;
    for (const auto &tsg : in) { stack.push_back(tsg.tilesetId); }

    TileSetGlues::list out;

    for (const auto &tsg : in) {
        auto compareGlues([&](const Glue &l, const Glue &r)-> bool
        {
            auto lr(boost::adaptors::reverse(l.id));
            auto rr(boost::adaptors::reverse(r.id));
            auto lrb(begin(lr)), lre(end(lr));
            auto rrb(begin(rr)), rre(end(rr));

            while ((lrb != lre) && (rrb != rre)) {
                // grab characters
                auto lc(alphabet[*lrb++]);
                auto rc(alphabet[*rrb++]);

                if (lc < rc) {
                    return true;
                } else if (rc < lc) {
                    return false;
                }
                // same character at the same position -> next one
            }

            // one id is prefix or the other (or both are the same, which is
            // unlikely) -> longest is less
            return r.id.size() <= l.id.size();
        });

        out.push_back(tsg);
        std::sort(out.back().glues.begin(), out.back().glues.end()
                  , compareGlues);

        for (auto &glue : out.back().glues) {
            std::size_t i(0);
            for (const auto &id : glue.id) {
                if (i >= stack.size()) {
                    LOGTHROW(err2, vadstena::storage::Error)
                        << "Glue <" << utility::join(id, ", ")
                        << "> doesn't belong into tileset stack stack <"
                        << utility::join(stack, ",") << ">.";
                }

                while (i < stack.size()) {
                    if (stack[i] == id) {
                        glue.indices.push_back(i);
                        ++i;
                        break;
                    } else {
                        ++i;
                    }
                }
            }
        }
    }

    return out;
}

} } // namespace vadstena::vts
