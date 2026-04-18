#pragma once

namespace mua::cli {

int run(int argc, char **argv);

#if defined(_WIN32)
int run(int argc, wchar_t **argv);
#endif

} // namespace mua::cli
