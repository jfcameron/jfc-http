// © Joseph Cameron - All Rights Reserved

#ifndef JFC_HTTP_POLICY_H
#define JFC_HTTP_POLICY_H

#include <jfc/http/types.h>

namespace jfc::http
{
    /// \brief runs every task on the calling thread, in order, before returning
    [[nodiscard]] task_submitter_type make_serial_task_submitter();

    /// \brief library-wide configuration, supplied once at context construction
    struct policy final {
        task_submitter_type submit = make_serial_task_submitter(); ///< where fetch work goes
    };
}

#endif
