// © Joseph Cameron - MIT License

#include <jfc/http/exception.h>

using namespace jfc;

http::exception::exception(const std::string &aWhat)
: std::runtime_error(aWhat)
{}

http::exception::exception(const char *const aWhat)
: std::runtime_error(aWhat)
{}
