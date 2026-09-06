// © Joseph Cameron - All Rights Reserved

#include <jfc/http/types.h>

#include <algorithm>
#include <cctype>

using namespace jfc;

bool http::header_name_equals(const http::header_view_type aLeft,
    const http::header_view_type aRight
) {
    return std::equal(aLeft.begin(), aLeft.end(), aRight.begin(), aRight.end(),
        [](const char aLeftChar, const char aRightChar) {
            return std::tolower(static_cast<unsigned char>(aLeftChar))
                == std::tolower(static_cast<unsigned char>(aRightChar));
        });
}
