#ifndef ENDIAN_H
#define ENDIAN_H

#ifdef IS_BIG_ENDIAN
#	define SWAP_16(x) (__builtin_bswap16(x))
#	define SWAP_32(x) (__builtin_bswap32(x))
#	define SWAP_64(x) (__builtin_bswap64(x))
#else
#	define SWAP_16(x) x
#	define SWAP_32(x) x
#	define SWAP_64(x) x
#endif


#endif

