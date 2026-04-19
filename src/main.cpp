#include "cli/app.hpp"

#if defined(_WIN32)
int wmain(const int argc, wchar_t **argv) {
    return mua::app::run(argc, argv);
}
#else
int main(const int argc, char **argv) {
    return mua::app::run(argc, argv);
}
#endif
