// © Joseph Cameron - All Rights Reserved

#include <jfc/http/request.h>

#include <string>

using namespace jfc;

std::string http::user_agent_header(const http::request_config &aConfig) {
    if (aConfig.user_agent.empty()) return std::string(http::LIBRARY_PRODUCT_TOKEN);

    return aConfig.user_agent + " " + http::LIBRARY_PRODUCT_TOKEN;
}
