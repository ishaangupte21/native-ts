#ifndef NTSC_USEROPTS_H
#define NTSC_USEROPTS_H

/*
    This file defines the static interface for storing user CLI options.
*/

namespace ntsc {
struct UserOpts {
    // This option controls whether to enable TypeScript strict mode. It is
    // enabled by default.
    static inline auto strictModeEnabled = true;
};
} // namespace ntsc

#endif