#ifndef WFORGE_RUNTIME_PLATFORM_H
#define WFORGE_RUNTIME_PLATFORM_H

#include <quickjs.h>

namespace wf::js {

void bindPlatformApi(JSContext *ctx, JSValueConst waveforge);

} // namespace wf::js

#endif // WFORGE_RUNTIME_PLATFORM_H
