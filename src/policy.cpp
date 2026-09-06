// © Joseph Cameron - All Rights Reserved

#include <jfc/http/policy.h>

#include <utility>
#include <vector>

using namespace jfc;

http::task_submitter_type http::make_serial_task_submitter()
{
    return [](std::vector<http::task_type> &&aTasks)
    {
        for (auto &task : aTasks) task();
    };
}
