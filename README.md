# ExFATRestore

Tool for restoring your ExFAT files and filesystem when you accidentally format the wrong disk.

Unless of course the disk was encrypted, then the key would possibly be nuked and the data useless.
Fortunately mine was not. Good reason not to blindly trust encryption: You're more likely to lock 
yourself out than any malicious third-party. I personally tend to favor encrypting individual files
over a whole disk unless that disk is on a machine likely to be lost or stolen like a smartphone.

mmap()s the whole disk into memory, this is a huge improvement over the naive read()ing and 
buffering algorithms I was using before. mmap, it's like magic!

Have successfully used it to restore (contiguous) files! But only into a flat directory.
Restoring the directory tree is currently a work in progress.

Some fragmented files may never be able to be reconstructed, since the FAT is nuked, 
but I'm hoping when I've removed all the contiguous files I can basically reverse engineer the 
fragmenting algorithms used by different drivers to restore at least some of them,
or simply go by file sizes and internal checksums to brute-force all possible reconstructions
until they're all correct.

Unfortunately ExFAT does not checksum individual clusters or whole files, which would have allowed 
reconstruction by brute-forcing the checksums rather than huge blocks of data. One advantage of using
checksums over hashes to ensure data integrity.
