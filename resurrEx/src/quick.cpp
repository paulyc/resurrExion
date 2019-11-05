//
//  quick.cpp
//  resurrExion
//
//  Created by Paul Ciarlo on 11/5/19.
//
//  Copyright (C) 2019 Paul Ciarlo <paul.ciarlo@gmail.com>.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//  SOFTWARE.
//

#include <unordered_map>
#include <typeinfo>

#include "fsconfig.hpp"
#include "filesystem.hpp"
#include "recoverylog.hpp"

using namespace github::paulyc::resurrExion;
using github::paulyc::Loggable;

typedef ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors> filesystem_t;
typedef RecoveryLog<filesystem_t> log_t;
typedef log_t::Entity_T entity_t;

int main(int argc, char *argv[]) {
    std::unordered_map<std::streamoff, std::streamoff> child_to_parent;
    filesystem_t fs(argv[1], DiskSize, PartitionStartSector, false);
    log_t log;
    log.parseTextLog(argv[1], fs, [&fs, &log, &child_to_parent](std::streamoff offset, entity_t ent, std::optional<std::exception> except){
        // go through the log, add every file, update directory children,
        // anything left with no parent goes in lost+found
        if (except.has_value()) {
            const std::exception &ex = except.value();
            fprintf(stderr, "[WARN] entry_info had exception type %s with message %s\n", typeid(ex).name(), ex.what());
        } else if (ent != nullptr) {
            switch (ent->get_type()) {
            case BaseEntity::File:
            {
                std::shared_ptr<FileEntity> f = std::dynamic_pointer_cast<FileEntity>(ent);
                if (!f) {
                    fprintf(stderr, "[WARN] Invalid file entry at offset %016llx\n", offset);
                    return;
                }
                break;
            }
            case BaseEntity::Directory:
            {
                std::shared_ptr<DirectoryEntity> d = std::dynamic_pointer_cast<DirectoryEntity>(ent);
                if (!d) {
                    fprintf(stderr, "[WARN] Invalid directory entry at offset %016llx\n", offset);
                    return;
                }
                for (auto &child: d->get_children()) {
                    child->set_parent(ent);
                }
                break;
            }
            default:
                fprintf(stderr, "[WARN] Unknown entity type\n");
                break;
            }
        } else {
            // Bad sector, mark it in the fat
        }
    });

    FILE *orphanlog = fopen("orphan.log", "w");
    fs.find_orphans([&orphanlog](std::streamoff offset, std::shared_ptr<BaseEntity> ent) {
#warning this is not a real offset its just a mmaped pointer!
        fprintf(orphanlog, "ORPHAN %016ullx %s", offset, ent->get_name().c_str());
    });

    return 0;
}
