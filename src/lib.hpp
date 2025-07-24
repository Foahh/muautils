#pragma once

#include <exception>
#include <filesystem>
#include <fmt/format.h>
#include <string>

namespace fs = std::filesystem;

namespace lib {
class FileError final : public std::exception {
public:
    FileError(const fs::path &path, const std::string &message) {
        m_msg = fmt::format("{} (while opening: {})", message, path.string());
    }

    [[nodiscard]] const char *what() const noexcept override {
        return m_msg.c_str();
    }

private:
    std::string m_msg;
};

} // namespace lib