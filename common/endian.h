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

namespace endian
{

template <typename T>
T swap(const T&);
	
template <>
inline int8_t swap(const int8_t& val)
{
	return val;
}

template <>
inline int16_t swap(const int16_t& val)
{
	return SWAP_16(val);
}

template <>
inline int32_t swap(const int32_t& val)
{
	return SWAP_32(val);
}

template <>
inline int64_t swap(const int64_t& val)
{
	return SWAP_64(val);
}

}

#endif

