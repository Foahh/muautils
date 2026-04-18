#pragma once

#include <exception>
#include <filesystem>
#include <fmt/format.h>
#include <string>

namespace fs = std::filesystem;

namespace lib {

[[nodiscard]] inline std::string PathToUtf8(const fs::path &path) {
    const auto u8 = path.u8string();
    if (u8.empty()) {
        return {};
    }
    return std::string(reinterpret_cast<const char *>(u8.data()), u8.size());
}

class FileError final : public std::exception {
  public:
    FileError(const fs::path &path, const std::string &message) {
        m_msg = fmt::format("{} (while opening: {})", message, PathToUtf8(path));
    }

    [[nodiscard]] const char *what() const noexcept override {
        return m_msg.c_str();
    }

  private:
    std::string m_msg;
};

} // namespace lib