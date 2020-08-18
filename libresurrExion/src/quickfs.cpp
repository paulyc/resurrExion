//
//  quickfs.cpp
//  resurrExion
//
//  Created by Paul Ciarlo on 20 July 2020.
//
//  Copyright (C) 2020 Paul Ciarlo <paul.ciarlo@gmail.com>
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

#include "quickfs.hpp"

FilesystemStub::FilesystemStub():
    _fd(-1),
    _mmap((uint8_t*)MAP_FAILED),
    _partition_start(nullptr),
    _partition_end(nullptr)
{
    _invalid_file_name_characters = {'"', '*', '/', ':', '<', '>', '?', '\\', '|'};
    for (char16_t ch = 0; ch <= 0x001F; ++ch) {
        _invalid_file_name_characters.push_back(ch);
    }
}

void FilesystemStub::open(const std::string &devpath) {
    //const bool write_changes = false;
    //const int oflags = write_changes ? O_RDWR | O_DSYNC | O_RSYNC : O_RDONLY;
    //_fd = ::open(devpath.c_str(), O_RDONLY | O_DSYNC | O_RSYNC);
    _fd = ::open(devpath.c_str(), O_RDWR | O_DSYNC | O_RSYNC);
    if (_fd == -1) {
        std::cerr << "failed to open device " << devpath << std::endl;
        throw std::system_error(std::error_code(errno, std::system_category()));
    }

    //const int mprot = write_changes ? PROT_READ | PROT_WRITE : PROT_READ;
    //const int mflags = write_changes ? MAP_SHARED : MAP_PRIVATE;
    // Darwin doesn't like this with DiskSize == 3.6TB. Not sure if that's why,
    // but only about 1G ends up actually getting mapped for my disk with ~270000 entities....
   // _mmap = (uint8_t*)mmap(nullptr, DiskSize, PROT_READ, MAP_PRIVATE, _fd, 0);
    _mmap = (uint8_t*)mmap(nullptr, DiskSize, PROT_READ|PROT_WRITE, MAP_SHARED, _fd, 0);
    if (_mmap == (uint8_t*)MAP_FAILED) {
        std::cerr << "error opening mmap" << std::endl;
        throw std::system_error(std::error_code(errno, std::system_category()));
    }

     _partition_start = _mmap + PartitionStartSector * SectorSize;
     _partition_end = _partition_start + (NumSectors + 1) * SectorSize;
}

void FilesystemStub::close() {
    _partition_end = nullptr;
    _partition_start = nullptr;

    if (_mmap != MAP_FAILED) {
        munmap(_mmap, DiskSize);
        _mmap = (uint8_t*)MAP_FAILED;
    }

    if (_fd != -1) {
        ::close(_fd);
        _fd = -1;
    }
}

FilesystemStub::~FilesystemStub()
{
    close();
}

bool FilesystemStub::parseTextLog(const std::string &filename)
{
    //std::unordered_map<Entity*, Entity*> child_to_parent;
    //std::unordered_map<Entity*, uint64_t> entity_to_offset;

    std::regex fde("FDE ([0-9a-fA-F]{16}) (.*)");
    std::regex badsector("BAD_SECTOR ([0-9a-fA-F]{16})");
    std::ifstream logfile(filename);
    size_t count = 1;
    for (std::string line; std::getline(logfile, line); ++count) {
        std::smatch sm;
        if (count % 1000 == 0) {
            printf("count: %zu\n", count);
            if (count == 10000) {
                //break; //a test
            }
        }
        if (std::regex_match(line, sm, fde)) {
            byteofs_t offset;
            std::string filename;
            filename = sm[2];
            try {
                offset = stobyteofs(sm[1], nullptr, 16);
                Entity * ent = this->loadEntityOffset(offset, filename);
                if (ent != nullptr) {
                    //printf("%016lx %s\n", ent->get_offset(), ent->get_name().c_str());
                }
            } catch (std::exception &ex) {
                std::cerr << "Writing file entry to binlog, got exception: [" << typeid(ex).name() << "] with msg: " << ex.what() << std::endl;
                std::cerr << "Invalid textlog FDE line, skipping: " << line << std::endl;
            }
        } else if (std::regex_match(line, sm, badsector)) {
            byteofs_t offset;
            try {
                offset = stobyteofs(sm[1], nullptr, 16);
                std::cerr << "Noted bad sector at offset " << offset << std::endl;
            } catch (std::exception &ex) {
                std::cerr << "Writing bad sector to binlog, got exception " << typeid(ex).name() << " with msg: " << ex.what() << std::endl;
                std::cerr << "Invalid textlog BAD_SECTOR line, skipping: " << line << std::endl;
            }
        } else {
            std::cerr << "Unknown textlog line format: " << line << std::endl;
        }
    }
    return true;
    //return this->checkIntegrity();
}

#ifdef HAVE_OFFSET_ENTITY_MAPS
void FilesystemStub::log_results(const char *orphanlogfilename) {
    std::cerr << "find_orphans" << std::endl;
    FILE *orphanlog = fopen(orphanlogfilename, "w");
    for (auto it = _offsets_to_entities.begin(); it != _offsets_to_entities.end(); ++it) {
        const byteofs_t offset = it->first;
        const Entity *ent = it->second;
        if (ent == nullptr) {
            fprintf(orphanlog, "NOENT %016lx\n", offset);
        } else {
            const Entity *parent = ent->get_parent();
            if (parent == nullptr) {
                if (typeid(*ent) == typeid(File)) {
                    fprintf(orphanlog, "ORPHANFILE %016lx %s\n", offset, ent->get_name().c_str());
                } else {
                    fprintf(orphanlog, "ORPHANDIR %016lx %s\n", offset, ent->get_name().c_str());
                }
            } else {
                if (typeid(*ent) == typeid(File)) {
                    const File *f = dynamic_cast<const File*>(ent);
                    if (f->is_contiguous()) {
                        fprintf(orphanlog, "FILE %016lx %016lx ", f->get_offset(), f->get_data_offset());
                    } else {
                        fprintf(orphanlog, "FRAGMENT %016lx ", f->get_offset());
                    }
                } else {
                    const Directory *d = dynamic_cast<const Directory*>(ent);
                    fprintf(orphanlog, "DIRECTORY %016lx ", d->get_offset());
                }
                fprintf(orphanlog, " %016lx %s %s\n", parent->get_offset(), ent->get_name().c_str(), parent->get_name().c_str());
            }
        }
    }
    fclose(orphanlog);
}

// this can break them down into exfat-size chunks eventually
/*std::list<std::shared_ptr<Directory>>*/
void FilesystemStub::adopt_orphans() {
    //std::shared_ptr<Directory> adoptive_directory = RootDirectory;
    std::list<std::shared_ptr<Directory>> new_directories;
    for (auto it = _offsets_to_entities.begin(); it != _offsets_to_entities.end(); ++it) {
        const byteofs_t offset = it->first;
        Entity *ent = it->second;
        if (ent != nullptr) {
            const Entity *parent = ent->get_parent();
            if (parent == nullptr) {
                /**if (!adoptive_directory) {
                    adoptive_directory = RootDirectory;
                } *//**ignore for now else if (adoptive_directory->is_full()) {
                    new_directories.push_back(adoptive_directory);
                    adoptive_directory = RootDirectory;
                }*/
                fprintf(stderr,
                        "ORPHAN offset[%016lx] name[%s]\n",
                        offset, ent->get_name().c_str()
                );
                RootDirectory->add_child(ent, true);
            }
        }
    }
    //if (adoptive_directory) {
    //    new_directories.push_back(adoptive_directory);
    //}
    //return new_directories;
}
#endif

// confirmed working by checksumming a known file...
std::vector<uint8_t> FilesystemStub::read_file_contents(File *f) {
    std::vector<uint8_t> contents(f->get_data_length());
    uint8_t *data = f->get_data_offset() + _mmap;
    memcpy(contents.data(), data, f->get_data_length());
    return contents;
}

void FilesystemStub::dump_directory(Directory *d, const std::string &dirname, std::function<void(File*)> yield, bool actually_write) {
    std::string abs_dir = "/home/paulyc/elements/" + dirname + std::string("-") + std::to_string(d->_offset);
    if (actually_write) {
        mkdir(abs_dir.c_str(), 0777);
    }
    d->dump_files(_mmap, abs_dir, yield, actually_write);
}

Entity * FilesystemStub::loadEntityOffset(byteofs_t entity_offset, const std::string &suggested_name) {
    //fprintf(stderr, "loadEntityOffset[%016lld]\n", entity_offset);
//    auto loaded_entity = _offsets_to_entities.find(entity_offset);
//    if (loaded_entity != _offsets_to_entities.end()) {
//        return loaded_entity->second;
//    }

    uint8_t *entity_start = _mmap + entity_offset;

    exfat::file_directory_entry_t *fde = (exfat::file_directory_entry_t*)(entity_start);
    exfat::stream_extension_entry_t *streamext = (exfat::stream_extension_entry_t*)(entity_start+32);
    exfat::file_name_entry_t *n = (exfat::file_name_entry_t*)(entity_start+64);

    if (fde->type != exfat::FILE_DIR_ENTRY || streamext->type != exfat::STREAM_EXTENSION || n->type != exfat::FILE_NAME) {
        fprintf(stderr, "bad number of continuations at offset %016lx\n", entity_start - _mmap);
        return nullptr;
    }

    const uint8_t continuations = fde->continuations;
    if (continuations < 2 || continuations > 18) {
        fprintf(stderr, "bad number of continuations at offset %016lx\n", entity_start - _mmap);
        return nullptr;
    }

    size_t i;
    uint16_t chksum = 0;

    for (i = 0; i < sizeof(exfat::raw_entry_t); ++i) {
        if (i != 2 && i != 3) {
            chksum = uint16_t((chksum << 15) | (chksum >> 1)) + entity_start[i];
        }
    }

    for (; i < (continuations+1) * sizeof(exfat::raw_entry_t); ++i) {
        chksum = uint16_t((chksum << 15) | (chksum >> 1)) + entity_start[i];
    }

    if (chksum != fde->checksum) {
        fprintf(stderr, "bad file entry checksum at offset %016lx\n", entity_offset);
        return nullptr;
    }

    Entity *ent = make_entity(entity_offset, fde, suggested_name);
    //_entities_to_offsets[ent] = entity_offset;
    //_offsets_to_entities[entity_offset] = ent;
    if (typeid(*ent) == typeid(Directory)) {
        Directory *d = dynamic_cast<Directory*>(ent);
        this->load_directory(d);

    } else if (typeid(*ent) == typeid(File)) {
        File *f = dynamic_cast<File*>(ent);
    }
    return ent;
}

void FilesystemStub::write_file_data(File *f, const std::string &filename) {
    std::vector<uint8_t> content = this->read_file_contents(f);
    FILE *ff = fopen(filename.c_str(), "wb");
    fwrite(content.data(), 1, content.size(), ff);
    fclose(ff);
}

void FilesystemStub::load_directory(Directory *d) {
    uint32_t first_cluster = d->get_first_cluster();
    fprintf(stdout, "load_directory @cluster %08x name %s\n", first_cluster, d->_name.c_str());
    if (first_cluster == 0) {
        fprintf(stderr, "  first cluster is zero! cant handle, return\n");
        return;
    }
    byteofs_t dataoffset = cluster_number_to_offset(first_cluster);
    byteofs_t datalen = d->get_data_length();
    uint8_t *data = _mmap + dataoffset;
    uint8_t *enddata = data + datalen;
    struct exfat::secondary_directory_entry_t *dirent = (struct exfat::secondary_directory_entry_t *)(data);
    struct exfat::raw_entry_t *ent = (struct exfat::raw_entry_t*)(data);
    struct exfat::raw_entry_t *end = (struct exfat::raw_entry_t*)(enddata);
    for (;ent < end; ++ent) {
        if (ent->type == exfat::FILE_DIR_ENTRY) {
            Entity *e = this->loadEntityOffset(reinterpret_cast<uint8_t*>(ent) - _mmap, "noname");
            if (e != nullptr) {
                if (typeid(*e) == typeid(Directory)) {
                    Directory *d = dynamic_cast<Directory*>(e);
                    fprintf(stderr,
                            "SUBDIR offset[%016lx] parent_offset[%016lx] name[%s]\n",
                            e->get_offset(), e->get_parent_offset(), e->get_name().c_str()
                    );
                    // TODO UNSURE IF CAN DO THIS!
                    //this->load_directory(d);
                } else if (typeid(*e) == typeid(File)) {
                    File *f = dynamic_cast<File*>(e);
                    fprintf(stderr,
                            "FILE offset[%016lx] parent_offset[%016lx] name[%s] data_offset[%016lx] data_len[%016lx] contiguous[%d]\n",
                            f->get_offset(), f->get_parent_offset(), f->get_name().c_str(), f->get_data_offset(), f->get_data_length(), f->is_contiguous()
                    );
                }
                d->add_child(e);
                //fprintf(stderr, "  child %s\n", e->get_name().c_str());
                ent += e->get_num_continuations();
            }
        } else if (ent->type == 0) {
            //printf("  end directory listing\n");
            break;
        } else {
            switch (ent->type) {
            case exfat::STREAM_EXTENSION:
            case exfat::FILE_NAME:
            case exfat::FILE_DIR_DELETED:
            case exfat::STREAM_EXT_DELETED:
            case exfat::FILE_NAME_DELETED:
                break;
            default:
                fprintf(stderr, "  unknown raw ent type %02x\n", ent->type);
                break;
            }
        }
    }
    fprintf(stderr,
            "DIRECTORY offset[%016lx] parent_offset[%016lx] name[%s]\n",
            d->get_offset(), d->get_parent_offset(), d->get_name().c_str()
    );
}
