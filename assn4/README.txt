Compile using "gcc mapi.c -o my_malloc"

Default allocation policy is first fit (allocates first hole which is big enough), and is indicated by 0
Optional allocation policy is best fit (allocates smallest hole which is big enough), and is indicated by 1

Note: changed return value of my_free from void to int, to better be able to detect errors.
