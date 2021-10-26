#include "Uri.h"
#include <boost/format.hpp>
#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>


std::string Uri::GetIpnUriString(const uint64_t eidNodeNumber, const uint64_t eidServiceNumber) {
    static const boost::format fmtTemplate("ipn:%d.%d");
    boost::format fmt(fmtTemplate);
    fmt % eidNodeNumber % eidServiceNumber;
    return fmt.str();
}
std::string Uri::GetIpnUriStringAnyServiceNumber(const uint64_t eidNodeNumber) {
    static const boost::format fmtTemplate("ipn:%d.*");
    boost::format fmt(fmtTemplate);
    fmt % eidNodeNumber;
    return fmt.str();
}

bool Uri::ParseIpnUriString(const std::string & uri, uint64_t & eidNodeNumber, uint64_t & eidServiceNumber) {
    if ((uri.length() < 7) || (uri[0] != 'i') || (uri[1] != 'p') || (uri[2] != 'n') || (uri[3] != ':')) {
        return false;
    }
    typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
    static const boost::char_separator<char> sep(".", "", boost::keep_empty_tokens);
    tokenizer tokens(uri.cbegin() + 4, uri.cend(), sep);
    tokenizer::const_iterator it = tokens.begin();
    if (it == tokens.end()) {
        return false;
    }
    try {
        eidNodeNumber = boost::lexical_cast<uint64_t>(*it);
    }
    catch (boost::bad_lexical_cast &) {
        return false;
    }
    ++it;
    if (it == tokens.end()) {
        return false;
    }
    try {
        eidServiceNumber = boost::lexical_cast<uint64_t>(*it);
    }
    catch (boost::bad_lexical_cast &) {
        return false;
    }
    ++it;
    return (it == tokens.end()); //should be end at this point
}
