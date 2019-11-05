//
//  circbuf.hpp - Circular buffer
//  resurrExion
//
//  Created by Paul Ciarlo on 24 Sep 2019.
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

#ifndef _github_paulyc_circbuf_hpp_
#define _github_paulyc_circbuf_hpp_

#if 0

#include <fcntl.h>
#include <linux/aio_abi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

#define BUF_SZ (4096)

inline static int io_setup(unsigned nr, aio_context_t *ctxp)
{
    return syscall(__NR_io_setup, nr, ctxp);
}

inline static int io_destroy(aio_context_t ctx)
{
    return syscall(__NR_io_destroy, ctx);
}

inline static int io_submit(aio_context_t ctx, long nr, struct iocb **iocbpp)
{
    return syscall(__NR_io_submit, ctx, nr, iocbpp);
}

inline static int io_getevents(aio_context_t ctx, long min_nr, long max_nr,
                   struct io_event *events,
                   struct timespec *timeout)
{
    // This might be improved.
    return syscall(__NR_io_getevents, ctx, min_nr, max_nr, events, timeout);
}

#define BUF_SZ (4096)

template <int ChunkSize=4096, int NumChunks=128>
class FdCircbuf
{
public:
    FdCircbuf(const char *fname) : _ctx(0), _rdp(0), _wrp(0) {
        _fd = open(fname, O_RDONLY);
        int r = io_setup(128, &_ctx);
        if (r < 0) {
            throw "io_setup()";
        }
    }
    char peek();
    char get();
    size_t peek(char *pkbuf, size_t len);
    size_t get(char *getbuf, size_t len);

    size_t buffer_chunk(size_t ofs) const {
        const size_t buf_ofs = ofs % BufSize;
        return buf_ofs % ChunkSize;
    }

    void foo() {
        char buf[BUF_SZ];
            struct iocb cb = {.aio_fildes = fd,
                      .aio_lio_opcode = IOCB_CMD_PREAD,
                      .aio_buf = (uint64_t)&buf[0],
                      .aio_nbytes = BUF_SZ};
            struct iocb *list_of_iocb[1] = {&cb};

            r = io_submit(ctx, 1, list_of_iocb);
            if (r != 1) {
                PFATAL("io_submit()");
            }

            struct io_event events[1] = {};
            r = io_getevents(ctx, 1, 1, events, NULL);
            if (r != 1) {
                PFATAL("io_getevents()");
            }

            printf("read %lld bytes from /etc/passwd\n", events[0].res);
            io_destroy(ctx);
            close(fd);
    }
private:
    int *_fd;
    aio_context_t _ctx;
    volatile size_t _rdp;
    volatile size_t _wrp;
    const static size_t BufSize = ChunkSize*NumChunks;
    char _buf[BufSize];
};

#endif

#endif /* _github_paulyc_circbuf_hpp_ */
