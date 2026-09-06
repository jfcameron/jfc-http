// © Joseph Cameron - All Rights Reserved

#ifndef JFC_HTTP_EXCEPTION_H
#define JFC_HTTP_EXCEPTION_H

#include <jfc/http/types.h>

#include <stdexcept>
#include <string>

namespace jfc::http
{
    /// \brief root exception type for this project
    class exception : public std::runtime_error
    {
    public:
        explicit exception(const std::string &aWhat);
        explicit exception(const char *const aWhat);
    };
}

#endif
