# ymalloc
Yet another implementation of malloc and free

#### Implementation Details

Some notable details about the internal representation

- Allocated memory comes from the data segment using `sbrk`
- An implicit list of blocks is maintained by storing block sizes in (8 byte) headers and footers immediately before and after each allocation
- An explicit free list is maintained by doubly linked pointers in freed blocks
  - The minimum allocation therefore must be at least the size of two pointers (16 bytes)
- Adjacent freed blocks are coalesced and appended to the free list

#### Improvements

- Use `mmap` instead of `sbrk`
- Slightly more space can be used to store a checksum of block sizes and pointers in order to detect block corruption
- Switch to using an RB tree or similar structure, rather than doubly linked list, for more efficient block fitting
