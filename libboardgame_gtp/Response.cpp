//-----------------------------------------------------------------------------
/** @file libboardgame_gtp/Response.cpp
    @author Markus Enzenberger
    @copyright GNU General Public License version 3 or later */
//-----------------------------------------------------------------------------

#include "Response.h"

namespace libboardgame_gtp {

//-----------------------------------------------------------------------------

void Response::clear()
{
    m_stream.str(string());
    m_stream.copyfmt(m_dummy);
}

void Response::write(ostream& out, string& buffer) const
{
    buffer = m_stream.str();
    bool was_newline = false;
    for (auto c : buffer)
    {
        bool is_newline = (c == '\n');
        if (is_newline && was_newline)
            out << ' ';
        out << c;
        was_newline = is_newline;
    }
    if (! was_newline)
        out << '\n';
    out << '\n';
}

//-----------------------------------------------------------------------------

} // namespace libboardgame_gtp
