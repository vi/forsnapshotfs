forsnapshotfs allows storing multiple big files which are expect to have many common blocks (like LVM snapshots or virtual machine images) efficiently.

Usage
---

Storing the first file:

    fsfs-write /path/to/storage/directory name1 < /dev/mapper/vg-vn1
    
Storing the modified version of the file (based on "name1"):

    fsfs-write /path/to/storage/directory name2 name1 < /dev/mapper/vg-vn1

Extracting:

    fsfs-read /path/to/storage/directory name2 > output
    
Accessing the stored file without extrantion:

    fsfs-mount path/to/storage/directory /path/to/mountpoint
    mount -o ro,loop /path/to/mountpoint/name1 /mnt/name1
    
* There is built-in simple in-memory copy-on-write for replaying 
journaled filesystems. Changes are preserved until the file is closed.

Storage format
---

Storage directory have files with extensions "\*.dat", "\*.dsc", "\*.idx" and "\*.hsh".

dsc file have two decimal numbers (block size and number of blocks in a block group), followed by newline and maybe by dependency name.

    4096 1020
    root_20130429

idx file contains 2048-byte entries, 
each descibing 1020 blocks. It is 8 bytes of big endian base 
offset in the dat file, for this block group, followed by 1020 2-byte 
signed big endian numbers (0 - unallocated block (near the end of file), 
positive - length of this block compressed, negative - this block is 
not stored here and should be retrieved from a dependency, 0x8000 - 
zero block, 0x8001 - uncompressed block). Blocks are usually 4096 bytes each.

dat file contains lzo1x-compressed blocks contatenated, referenced by idx file.

hsh file contains 8-bit pearsong-style hashes of blocks. i'th byte of 
this file correspond to i'th block.

Example:

    test.dsc:
        4096 1020

    test.idx:

        0x0000: 0000000000000000 0030 0090 0100 1034 06F3
             (then 1015 times of 0000, for example)
        0x0800: 00000000000018E7 0055 0000 0000 0000 .....
        0x1000: end of file
        
    test.dat:
    
        0x0000: (0x30 bytes of compressed data for block 0)
        0x0030: (0x90 bytes of compressed data for block 1)
        0x00C0: (0x100 bytes of compressed data for block 2)
        0x01C0: (0x1034 bytes of compressed data for block 3)
        0x11F4: (0x6F3 bytes of compressed dta for block 4)
        0x18E7: (0x55 bytes of compressed data for block 1020)
        0x193C: end of file
        
    test.hsh:
    
        0x0000: 53 99 32 11 67 00 00 00 00 00 00 .. 
        0x03FC: 34 (end of file)
        
    
        
    "qqq" data corresponds to the "test", but after 
        pwrite(fd, "qqq", 3, 0x302C)
        
        
    qqq.dsc:
        4096 1020
        test
        
    qqq.idx:
    
        0x0000: 0000000000000000 FFFF FFFF FFFF 1036 FFFF 8000 8000 ...
        0x0800: 0000000000001036 FFFF 0000 0000 ...
        0x1000: end of file
        
    qqq.dat:
    
        0x0000: (0x1036 bytes of compressed data for block 3)
        0x1036: end of file
       
    qqq.hsh:
        0x0000: 53 99 32 9C 67 00 00 00 00 00 00 .. 
        0x03FC: 34 (end of file)

    
        
    "qqq2" is based on "qqq" and have the same data as "qqq":
    
    qqq2.dsc:
        4096 1020
        qqq
        
    qqq2.idx:
    
        0x0000: 0000000000000000 FFFE FFFE FFFE FFFF FFFE 8000 8000 ...
        0x0800: 0000000000000000 FFFE 0000 0000 ...
        0x1000: end of file
        
        FFFE means "refer to the second dependency"
        FFFF means "refer to the first(directly specified in dsc) dependency
        
    qqq2.dat:
    
        0x0000: end of file
       
    qqq2.hsh:
        0x0000: 53 99 32 9C 67 00 00 00 00 00 00 .. 
        0x03FC: 34 (end of file)
    
Currently maximum the number of dependencies is limited to 64.

Comparison with [bup](https://github.com/bup/bup)
---
If you are interested in forsnapshotfs, you may also like [bup](https://github.com/bup/bup).

Differences:

* fsfs is simpler. it is implemented in C. The algorithm is simpler than bup's one;
* `bup` compress better;
* fsfs require you to specify previous backup's name when creating new one;
* `bup` also can manage many little files, file modes, etc;
* `bup` have remote access features;
* `fsfs-mount` have little additional cosy feature: simplistic in-memory copy-on-write to allow journal replay when mounting a filesystem.

Similaritues:

* Compression support;
* Efficient storage of incremental snapshots (fsfs's deduplication is simpler and won't work when offsets do change);
* Random access to stored files, FUSE interface;
* Both can accept snapshot input from stdin;
* Newer backups depend on earlier ones.
