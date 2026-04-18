#include "cli/cli.hpp"

#if defined(_WIN32)
int wmain(const int argc, wchar_t **argv) {
    return mua::cli::run(argc, argv);
}
#else
int main(const int argc, char **argv) {
    return mua::cli::run(argc, argv);
}
#endif
