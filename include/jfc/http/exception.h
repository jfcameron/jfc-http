// © Joseph Cameron - MIT License

#ifndef JFC_HTTP_EXCEPTION_H
#define JFC_HTTP_EXCEPTION_H

#include <jfc/http/types.h>

#include <exception>
#include <memory>
#include <string>

namespace jfc::http
{
    /// \brief root exception type for this project
    class exception : public std::exception
    {
        std::shared_ptr<const std::string> m_pWhat;

    public:
        explicit exception(const std::string &aWhat);
        explicit exception(const char *const aWhat);

        exception(const exception &aOther) noexcept = default;

        exception &operator=(const exception &aOther) noexcept = default;

        const char *what() const noexcept override;
    };
}

#endif
