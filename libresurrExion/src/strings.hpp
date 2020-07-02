#ifndef STRINGS_HPP
#define STRINGS_HPP

#include <string>

#include "exfat_structs.hpp"

namespace github {
namespace paulyc {
namespace resurrExion {

std::string get_utf8_filename(struct exfat::file_directory_entry_t *fde, struct exfat::stream_extension_entry_t *sde);

inline bool is_valid_filename_char(char16_t ch) {
    switch (ch) {
    case '\'':
    case '*':
    case '/':
    case ':':
    case '<':
    case '>':
    case '?':
    case '\\':
    case '|':
        return false;
    default:
        return ch >= 0x0020;
    }
}

}
}
}

#endif // STRINGS_HPP
