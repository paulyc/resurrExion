#include "strings.hpp"
#include <locale>
#include <codecvt>

namespace {

std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> cvt;

}

namespace github {
namespace paulyc {
namespace resurrExion {

std::string get_utf8_filename(exfat::file_directory_entry_t *fde, struct exfat::stream_extension_entry_t *sde)
{
    const size_t entry_count = fde->continuations + 1;
    // i dont know that this means anything without decoding the string because UTF-16 is so dumb re: the "emoji problem"
    const size_t namelen = sde->name_length;
    struct exfat::file_name_entry_t *n = reinterpret_cast<struct exfat::file_name_entry_t*>(fde);
    std::basic_string<char16_t> u16s;
    for (size_t c = 2; c < entry_count && u16s.length() <= namelen; ++c) {
        if (n[c].type == exfat::FILE_NAME) {
            for (size_t i = 0; i < exfat::file_name_entry_t::FS_FILE_NAME_ENTRY_SIZE && u16s.length() <= namelen; ++i) {
                u16s.push_back((char16_t)n[c].name[i]);
            }
        }
    }
    return cvt.to_bytes(u16s);
}

}
}
}
