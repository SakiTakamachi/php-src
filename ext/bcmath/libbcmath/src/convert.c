/*
   +----------------------------------------------------------------------+
   | Copyright (c) The PHP Group                                          |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | https://www.php.net/license/3_01.txt                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Niels Dossche <nielsdos@php.net>                            |
   +----------------------------------------------------------------------+
*/

#include "bcmath.h"
#include "convert.h"
#include "private.h"
#ifdef __SSE2__
# include <emmintrin.h>
#endif


/* This is based on the technique described in https://kholdstare.github.io/technical/2020/05/26/faster-integer-parsing.html.
 * This function transforms AABBCCDD into 1000 * AA + 100 * BB + 10 * CC + DD,
 * with the caveat that all components must be in the interval [0, 25] to prevent overflow
 * due to the multiplication by power of 10 (10 * 25 = 250 is the largest number that fits in a byte).
 * The advantage of this method instead of using shifts + 3 multiplications is that this is cheaper
 * due to its divide-and-conquer nature.
 */
#if SIZEOF_SIZE_T == 4
static BC_VECTOR bc_parse_chunk_chars(BC_VECTOR vector)
{
#if !BC_LITTLE_ENDIAN
	vector = BC_BSWAP(vector);
#endif

	BC_VECTOR lower_digits = (vector & 0x0f000f00) >> 8;
	BC_VECTOR upper_digits = (vector & 0x000f000f) * 10;

	vector = lower_digits + upper_digits;

	lower_digits = (vector & 0x00ff0000) >> 16;
	upper_digits = (vector & 0x000000ff) * 100;

	return lower_digits + upper_digits;
}
#elif SIZEOF_SIZE_T == 8
static BC_VECTOR bc_parse_chunk_chars(BC_VECTOR vector)
{
#if !BC_LITTLE_ENDIAN
	vector = BC_BSWAP(vector);
#endif

	BC_VECTOR lower_digits = (vector & 0x0f000f000f000f00) >> 8;
	BC_VECTOR upper_digits = (vector & 0x000f000f000f000f) * 10;

	vector = lower_digits + upper_digits;

	lower_digits = (vector & 0x00ff000000ff0000) >> 16;
	upper_digits = (vector & 0x000000ff000000ff) * 100;

	vector = lower_digits + upper_digits;

	lower_digits = (vector & 0x0000ffff00000000) >> 32;
	upper_digits = (vector & 0x000000000000ffff) * 10000;

	return lower_digits + upper_digits;
}
#endif

static inline BC_VECTOR *bc_convert_str_to_vector(BC_VECTOR *vectors, const char *source, BC_VECTOR base, size_t length)
{
	*vectors = 0;
	for (size_t i = 0; i < length; i++) {
		*vectors += (*source - '0') * base;
		base *= BASE;
		source--;
	}
	return ++vectors;
}

static inline BC_VECTOR *bc_bulk_convert_str_to_vector(BC_VECTOR *vectors, const char *source, size_t vector_size, size_t end_size)
{
	const BC_VECTOR bulk_shift = SWAR_REPEAT('0');
	source++;

#ifdef __SSE2__
	/* SIMD SSE2 bulk shift + copy */
	__m128i shift_vector = _mm_set1_epi8('0');
	while (vector_size > sizeof(__m128i) / BC_VECTOR_SIZE) {
		source -= sizeof(__m128i);

		__m128i bytes = _mm_loadu_si128((const __m128i *) source);
		bytes = _mm_sub_epi8(bytes, shift_vector);

		__m128i lower_digits = _mm_srli_si128(_mm_and_si128(bytes, _mm_set1_epi16(0x0f00)), 1); // >> 8
		__m128i upper_digits = _mm_mullo_epi16(_mm_and_si128(bytes, _mm_set1_epi16(0x000f)), _mm_set1_epi16(10)); // * 10
		bytes = _mm_add_epi8(lower_digits, upper_digits);

		lower_digits = _mm_srli_si128(_mm_and_si128(bytes, _mm_set1_epi32(0x00ff0000)), 2); // >> 16
		upper_digits = _mm_mullo_epi16(_mm_and_si128(bytes, _mm_set1_epi32(0x000000ff)), _mm_set1_epi32(100)); // * 100
		bytes = _mm_add_epi16(lower_digits, upper_digits);

		lower_digits = _mm_srli_si128(_mm_and_si128(bytes, _mm_set1_epi64x(0x0000ffff00000000)), 4); // >> 32
		upper_digits = _mm_mul_epu32(_mm_and_si128(bytes, _mm_set1_epi64x(0x000000000000ffff)), _mm_set1_epi64x(10000)); // * 10000
		bytes = _mm_add_epi32(lower_digits, upper_digits);

		bytes = _mm_or_si128(_mm_srli_si128(bytes, 8), _mm_slli_si128(bytes, 8)); // swap

		_mm_storeu_si128((__m128i *) vectors, bytes);

		vectors += sizeof(__m128i) / BC_VECTOR_SIZE;
		vector_size -= sizeof(__m128i) / BC_VECTOR_SIZE;
	}
#endif

	while (vector_size > end_size) {
		BC_VECTOR bytes;
		source -= BC_VECTOR_SIZE;

		memcpy(&bytes, source, BC_VECTOR_SIZE);
		bytes -= bulk_shift;
		*vectors = bc_parse_chunk_chars(bytes);

		vectors++;
		vector_size--;
	}

	return vectors;
}

void bc_convert_int_str_to_vector(BC_VECTOR *vectors, const char *source, size_t vector_size, size_t protruded_len)
{
	size_t bulk_vsize = vector_size;
	size_t protruded_vsize = 0;
	if (protruded_len > 0) {
		bulk_vsize--;
		protruded_vsize++;
	}

	if (bulk_vsize > 0) {
		vectors = bc_bulk_convert_str_to_vector(vectors, source, vector_size, protruded_len ? 1 : 0);
		source -= BC_VECTOR_SIZE * bulk_vsize;
	}

	if (protruded_vsize == 1) {
		bc_convert_str_to_vector(vectors, source, 1, protruded_len);
	}
}

void bc_convert_frac_str_to_vector(BC_VECTOR *vectors, const char *source, size_t vector_size, size_t protruded_len)
{
	if (protruded_len) {
		vectors = bc_convert_str_to_vector(vectors, source, BC_POW_10_LUT[BC_VECTOR_SIZE - protruded_len], protruded_len);
		vector_size--;
		source -= protruded_len;
	}

	bc_bulk_convert_str_to_vector(vectors, source, vector_size, 0);
}

#if BC_LITTLE_ENDIAN
# define BC_ENCODE_LUT(A, B) ((A) | (B) << 4)
#else
# define BC_ENCODE_LUT(A, B) ((B) | (A) << 4)
#endif

#define LUT_ITERATE(_, A) \
	_(A, 0), _(A, 1), _(A, 2), _(A, 3), _(A, 4), _(A, 5), _(A, 6), _(A, 7), _(A, 8), _(A, 9)

/* This LUT encodes the decimal representation of numbers 0-100
 * such that we can avoid taking modulos and divisions which would be slow. */
static const unsigned char LUT[100] = {
	LUT_ITERATE(BC_ENCODE_LUT, 0),
	LUT_ITERATE(BC_ENCODE_LUT, 1),
	LUT_ITERATE(BC_ENCODE_LUT, 2),
	LUT_ITERATE(BC_ENCODE_LUT, 3),
	LUT_ITERATE(BC_ENCODE_LUT, 4),
	LUT_ITERATE(BC_ENCODE_LUT, 5),
	LUT_ITERATE(BC_ENCODE_LUT, 6),
	LUT_ITERATE(BC_ENCODE_LUT, 7),
	LUT_ITERATE(BC_ENCODE_LUT, 8),
	LUT_ITERATE(BC_ENCODE_LUT, 9),
};

static inline unsigned short bc_expand_lut(unsigned char c)
{
	return (c & 0x0f) | (c & 0xf0) << 4;
}

/* Writes the character representation of the number encoded in value.
 * E.g. if value = 1234, then the string "1234" will be written to str. */
void bc_write_bcd_representation(uint32_t value, char *str)
{
	uint32_t upper = value / 100; /* e.g. 12 */
	uint32_t lower = value % 100; /* e.g. 34 */

#if BC_LITTLE_ENDIAN
	/* Note: little endian, so `lower` comes before `upper`! */
	uint32_t digits = bc_expand_lut(LUT[lower]) << 16 | bc_expand_lut(LUT[upper]);
#else
	/* Note: big endian, so `upper` comes before `lower`! */
	uint32_t digits = bc_expand_lut(LUT[upper]) << 16 | bc_expand_lut(LUT[lower]);
#endif
	memcpy(str, &digits, sizeof(digits));
}
