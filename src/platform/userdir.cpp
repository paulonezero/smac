// The per-user directory of a copy from before the rename to OpenSMAC (userdir.h).
#include "platform/userdir.h"

#include <cctype>
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

namespace userdir {

const char* app_name() {
#if defined(_WIN32) || defined(__APPLE__)
    return "OpenSMAC";
#else
    return "opensmac";
#endif
}

std::string legacy_dir(const std::string& udir) {
    fs::path p = fs::u8path(udir);
    if (!p.has_filename()) p = p.parent_path();  // the trailing separator
    if (!p.has_parent_path() || p.parent_path() == p) return "";
    fs::path old = p.parent_path() / kLegacyAppName;
    old += fs::path::preferred_separator;
    return old.u8string();
}

Migrated migrate_dir(const std::string& old_dir, const std::string& new_dir, std::string* error) {
    std::error_code ec;
    fs::path from = fs::u8path(old_dir), to = fs::u8path(new_dir);
    if (!from.has_filename()) from = from.parent_path();
    if (!to.has_filename()) to = to.parent_path();
    if (!fs::is_directory(from, ec)) return Migrated::kNothing;
    if (fs::exists(to, ec)) {
        if (!fs::is_directory(to, ec) || !fs::is_empty(to, ec)) return Migrated::kNothing;
        fs::remove(to, ec);
    }
    fs::rename(from, to, ec);
    if (!ec) return Migrated::kMoved;
    // Another file system (or a rename refused): a copy, the old directory kept.
    fs::create_directories(to, ec);
    if (!ec) fs::copy(from, to, fs::copy_options::recursive | fs::copy_options::copy_symlinks, ec);
    if (!ec) return Migrated::kCopied;
    if (error) *error = ec.message();
    std::error_code ignored;
    fs::remove_all(to, ignored);
    fs::create_directories(to, ignored);  // as SDL_GetPrefPath left it
    return Migrated::kFailed;
}

void migrate_files(const std::string& udir) {
    std::error_code ec;
    fs::path dir = fs::u8path(udir);
    fs::path cfg = dir / kConfig, legacy = dir / kLegacyConfig;
    if (!fs::exists(cfg, ec) && fs::is_regular_file(legacy, ec)) {
        fs::rename(legacy, cfg, ec);
        if (ec) fs::copy_file(legacy, cfg, ec);
    }
    fs::remove(dir / "smacxnet.txt", ec);
}

std::string latest_save(const std::string& saves_dir) {
    std::error_code ec;
    fs::path best;
    fs::file_time_type best_time{};
    bool best_auto = false;
    for (fs::recursive_directory_iterator it(saves_dir, ec), end; !ec && it != end; it.increment(ec)) {
        std::string ext = it->path().extension().string();
        for (char& c : ext) c = (char)std::tolower((unsigned char)c);
        std::error_code fe;
        if (ext != ".sav" || !it->is_regular_file(fe)) continue;
        const fs::file_time_type time = it->last_write_time(fe);
        if (fe) continue;
        const fs::path rel = it->path().lexically_relative(saves_dir);
        std::string top = rel.has_parent_path() ? rel.begin()->string() : std::string();
        for (char& c : top) c = (char)std::tolower((unsigned char)c);
        const bool is_auto = top == "auto";
        // (the same time and kind: the first by name, whatever the directory's order)
        if (!best.empty() && (time < best_time || (time == best_time && (is_auto > best_auto || (is_auto == best_auto && rel > best)))))
            continue;
        best = rel;
        best_time = time;
        best_auto = is_auto;
    }
    if (best.empty()) return {};
    std::string name = "saves";
    for (const fs::path& part : best) name += "\\" + part.string();
    return name;
}

}  // namespace userdir
