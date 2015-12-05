Compile using provided makefile.

Default allocation policy is first fit (allocates first hole which is big enough), and is indicated by 0.
Optional allocation policy is best fit (allocates smallest hole which is big enough), and is indicated by 1.

Note: changed return value of my_free from void to int, to better be able to detect errors.

- A limitation of this implementation is an inability to check whether a pointer to be freed is some random one within the valid range which does not point to the beginning of an allocated block, without increasing the heap. Attempting to free a pointer within the valid range but not pointing to the beginning of a previously allocated block may cause a segmentation fault. Thus it is the responsibility of the program using this API to avoid this situation.
