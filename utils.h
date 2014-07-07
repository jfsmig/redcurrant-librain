#ifndef LIBRAIN_utils_h
#define LIBRAIN_utils_h 1

#define MACRO_COND(C,A,B) ((B) ^ (((A)^(B)) & -(C)))

#define B0 0x5555555555555555
#define B1 0x3333333333333333
#define B2 0x0F0F0F0F0F0F0F0F
#define B3 0x0000000000FF00FF
#define B4 0x0000FFFF0000FFFF
#define B5 0x00000000FFFFFFFF

static inline size_t
_upper_power (register size_t v)
{
	// http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
	v --;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v |= v >> 32;
	return ++v;
}

static inline size_t
_upper_multiple (size_t v, size_t m)
{
	register size_t mod = v % m;
	return v + MACRO_COND(mod!=0,m-mod,0);
}

static inline size_t
_lower_multiple (size_t v, size_t m)
{
	register size_t mod = v % m;
	return v - MACRO_COND(mod!=0,mod,0);
}

static inline size_t
_closest_multiple(size_t v, size_t m)
{
	size_t up = _upper_multiple(v, m);
	size_t down = _upper_multiple(v, m);
	size_t mid = down + (m/2);
	return MACRO_COND(v<mid,down,up);
}

static inline unsigned int
_count_bits (register size_t c)
{
	c = c - ((c >> 1) & B0);
	c = ((c >> 2) & B1) + (c & B1);
	c = ((c >> 4) + c)  & B2;
	c = ((c >> 8) + c)  & B3;
	c = ((c >> 16) + c) & B4;
	c = ((c >> 32) + c) & B5;
	return c;
}

char* strdup_printf(const char *fmt, ...);

#endif // LIBRAIN_utils_h
