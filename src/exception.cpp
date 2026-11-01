// © Joseph Cameron - MIT License

#include <jfc/http/exception.h>

using namespace jfc;

http::exception::exception(const std::string &aWhat)
: m_pWhat(std::make_shared<const std::string>(aWhat))
{}

http::exception::exception(const char *const aWhat)
: m_pWhat(std::make_shared<const std::string>(aWhat))
{}

const char *http::exception::what() const noexcept { return m_pWhat->c_str(); }
