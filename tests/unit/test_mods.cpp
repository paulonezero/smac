// The mods folder (platform::set_mods_dir, win32.h): a file there replaces the game's of the
// same name or adds one; everything else resolves as without it.
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "platform/win32.h"
#include "unit.h"

namespace fs = std::filesystem;

namespace {
// A game folder and a mods folder below the system's temporary directory; the game folder is the
// current directory while the fixture lives (as in a running game), all undone by the destructor.
struct Folders {
    fs::path root, game, mods, cwd;
    Folders() : root(fs::temp_directory_path() / "opensmac_unit_mods"), cwd(fs::current_path()) {
        fs::remove_all(root);
        game = root / "game";
        mods = root / "mods";
        fs::create_directories(game);
        fs::create_directories(mods);
        fs::current_path(game);
    }
    ~Folders() {
        platform::set_mods_dir("");
        platform::set_saves_dir("");
        std::error_code ec;
        fs::current_path(cwd, ec);
        fs::remove_all(root, ec);
    }
};

void write(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << text;
}

std::string resolve(const char* path) {
    char out[1024];
    return platform::resolve_path(path, out, sizeof(out));
}

// What the game reads when it opens path.
std::string content(const char* path) {
    uint32_t size = 0;
    uint8_t* data = win32::read_file(path, &size);
    if (!data) return "<none>";
    std::string text((const char*)data, size);
    win32::free_file(data);
    return text;
}

// The same file, whatever the spelling of the path to it (/tmp is a link on macOS).
bool same(const std::string& a, const fs::path& b) {
    std::error_code ec;
    return fs::equivalent(a, b, ec) && !ec;
}
}  // namespace

TEST_CASE("mods: a file in the mods folder replaces the game's") {
    Folders f;
    write(f.game / "alphax.txt", "game rules");
    write(f.game / "labels.txt", "game labels");
    write(f.mods / "ALPHAX.TXT", "our rules");  // any case
    // Without a mods folder: the game's file.
    CHECK(content("alphax.txt") == "game rules");
    std::string plain = resolve("labels.txt");
    platform::set_mods_dir(f.mods.string().c_str());
    CHECK(content("alphax.txt") == "our rules");
    CHECK(content("AlphaX.txt") == "our rules");
    CHECK(same(resolve("alphax.txt"), f.mods / "ALPHAX.TXT"));
    // A file the mods folder does not have resolves exactly as before.
    CHECK(resolve("labels.txt") == plain);
    CHECK(content("labels.txt") == "game labels");
    // No mods folder again: the game's file.
    platform::set_mods_dir("");
    CHECK(content("alphax.txt") == "game rules");
}

TEST_CASE("mods: sub-folders, added files and missing files") {
    Folders f;
    write(f.game / "movies" / "opening.wve", "game movie");
    write(f.mods / "Movies" / "Opening.WVE", "our movie");
    write(f.mods / "newfaction.txt", "a new faction");
    write(f.mods / "fx" / "new.wav", "a new sound");
    platform::set_mods_dir((f.mods.string() + "/").c_str());  // a trailing separator is fine
    CHECK(content("movies\\opening.wve") == "our movie");  // the game's separators
    CHECK(content("movies/opening.wve") == "our movie");
    CHECK(content("./movies/opening.wve") == "our movie");
    // Files only the mods folder has.
    CHECK(content("newfaction.txt") == "a new faction");
    CHECK(win32::file_exists("fx\\new.wav"));
    // In neither: not found, and the path comes back as given.
    CHECK(!win32::file_exists("nothing.txt"));
    CHECK(resolve("nothing.txt") == "nothing.txt");
}

TEST_CASE("mods: what is never taken from the mods folder") {
    Folders f;
    write(f.game / "saves" / "auto.sav", "game save");
    write(f.mods / "saves" / "auto.sav", "planted save");
    write(f.game / "texture.pcx", "game texture");
    fs::create_directories(f.mods / "texture.pcx");  // a folder of a file's name
    write(f.root / "outside.txt", "outside");
    write(f.mods / "inner" / "x.txt", "x");
    platform::set_mods_dir(f.mods.string().c_str());
    // Saved games, with and without the saves redirect.
    CHECK(content("saves\\auto.sav") == "game save");
    fs::path redirected = f.root / "user" / "saves";
    write(redirected / "auto.sav", "user save");
    platform::set_saves_dir(redirected.string().c_str());
    CHECK(content("saves\\auto.sav") == "user save");
    platform::set_saves_dir("");
    // A folder is not a file.
    CHECK(content("texture.pcx") == "game texture");
    // ".." never reaches through the mods folder; it means what it meant without one.
    CHECK(content("..\\outside.txt") == "outside");
    CHECK(same(resolve("../outside.txt"), f.root / "outside.txt"));
    CHECK(content("inner\\..\\..\\outside.txt") == "<none>");
    // An absolute path is the file it names.
    CHECK(content((f.game / "texture.pcx").string().c_str()) == "game texture");
}

TEST_CASE("mods: the folder's files as the game names them") {
    Folders f;
    write(f.mods / "README.md", "notes");
    write(f.mods / ".DS_Store", "finder");
    write(f.mods / ".git" / "config", "git");
    write(f.mods / "alphax.txt", "rules");
    write(f.mods / "movies" / "opening.wve", "movie");
    write(f.mods / "movies" / "README.md", "a file of the game's name space");
    std::vector<std::string> files = platform::mods_files(f.mods.string().c_str());
    CHECK(files == std::vector<std::string>{"alphax.txt", "movies/README.md", "movies/opening.wve"});
    CHECK(platform::mods_files((f.root / "absent").string().c_str()).empty());
    CHECK(platform::mods_files("").empty());
}
