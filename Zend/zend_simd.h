/*
   +----------------------------------------------------------------------+
   | Zend Engine                                                          |
   +----------------------------------------------------------------------+
   | Copyright (c) Zend Technologies Ltd. (http://www.zend.com)           |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.00 of the Zend license,     |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.zend.com/license/2_00.txt.                                |
   | If you did not receive a copy of the Zend license and are unable to  |
   | obtain it through the world-wide-web, please send a note to          |
   | license@zend.com so we can mail you a copy immediately.              |
   +----------------------------------------------------------------------+
   | Authors: Saki Takamachi <saki@php.net>                               |
   +----------------------------------------------------------------------+
*/

#ifndef ZEND_SIMD_H
#define ZEND_SIMD_H

#ifdef __SSE2__
#include <emmintrin.h>
#define ZEND_HAVE_VECTOR_128


#elif defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#define ZEND_HAVE_VECTOR_128

typedef int8x16_t __m128i;

#define _mm_setzero_si128() vdupq_n_s8(0)
#define _mm_set_epi8(x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15) \
	(int8x16_t) { \
		(int8_t) (x15), (int8_t) (x14), (int8_t) (x13), (int8_t) (x12), \
		(int8_t) (x11), (int8_t) (x10), (int8_t) (x9),  (int8_t) (x8), \
		(int8_t) (x7),  (int8_t) (x6),  (int8_t) (x5),  (int8_t) (x4), \
		(int8_t) (x3),  (int8_t) (x2),  (int8_t) (x1),  (int8_t) (x0) }
#define _mm_set1_epi8(x) vdupq_n_s8(x)
#define _mm_set_epi16(x0, x1, x2, x3, x4, x5, x6, x7) \
	vreinterpretq_s8_s16((int16x8_t) { \
		(int16_t) (x7), (int16_t) (x6), (int16_t) (x5), (int16_t) (x4), \
		(int16_t) (x3), (int16_t) (x2), (int16_t) (x1), (int16_t) (x0) })
#define _mm_set_epi32(x0, x1, x2, x3) \
	vreinterpretq_s8_s32((int32x4_t) { (int32_t) (x3), (int32_t) (x2), (int32_t) (x1), (int32_t) (x0) })
#define _mm_set_epi64x(x0, x1) vreinterpretq_s8_s64((int64x2_t) { (int64_t) (x1), (int64_t) (x0) })
#define _mm_load_si128(x) vld1q_s8((const int8_t *) (x))
#define _mm_loadu_si128(x) _mm_load_si128(x)
#define _mm_store_si128(to, x) vst1q_s8((int8_t *) (to), x)
#define _mm_storeu_si128(to, x) _mm_store_si128(to, x)

#define _mm_or_si128(a, b) vorrq_s8(a, b)
#define _mm_xor_si128(a, b) veorq_s8(a, b)
#define _mm_and_si128(a, b) vandq_s8(a, b)

#define _mm_slli_si128(x, imm) \
    ((imm) >= 16 ? vdupq_n_s8(0) : \
     vreinterpretq_s8_u8(vextq_u8(vdupq_n_u8(0), vreinterpretq_u8_s8(x), 16 - (imm))))
#define _mm_srli_si128(x, imm) \
    ((imm) >= 16 ? vdupq_n_s8(0) : \
     vreinterpretq_s8_u8(vextq_u8(vreinterpretq_u8_s8(x), vdupq_n_u8(0), (imm))))

/**
 * In practice, there is no problem, but a runtime error for signed integer overflow is triggered by UBSAN,
 * so perform the calculation as unsigned. Since it is optimized at compile time, there are no unnecessary casts at runtime.
 */
#define _mm_add_epi8(a, b) vreinterpretq_s8_u8(vaddq_u8(vreinterpretq_u8_s8(a), vreinterpretq_u8_s8(b)))
#define _mm_add_epi32(a, b) vreinterpretq_s8_u32(vaddq_u32(vreinterpretq_u32_s8(a), vreinterpretq_u32_s8(b)))

static zend_always_inline __m128i _mm_sad_epu8(__m128i a, __m128i b)
{
    uint16x8_t abs_diffs_16 = vpaddlq_u8(vabdq_u8(vreinterpretq_u8_s8(a), vreinterpretq_u8_s8(b)));
	uint32x4_t abs_diffs_32 = vpaddlq_u16(abs_diffs_16);
	uint64x2_t abs_diffs_64 = vpaddlq_u32(abs_diffs_32);

	return vreinterpretq_s8_u16((uint16x8_t) {
		(int16_t) vgetq_lane_u64(abs_diffs_64, 0), 0, 0, 0,
		(int16_t) vgetq_lane_u64(abs_diffs_64, 1), 0, 0, 0
	});
}

static zend_always_inline __m128i _mm_madd_epi16(__m128i a, __m128i b)
{
	int32x4_t mul_lo = vmull_s16(vget_low_s16(vreinterpretq_s16_s8(a)), vget_low_s16(vreinterpretq_s16_s8(b)));
	int32x4_t mul_hi = vmull_s16(vget_high_s16(vreinterpretq_s16_s8(a)), vget_high_s16(vreinterpretq_s16_s8(b)));

	return vreinterpretq_s8_s32(vcombine_s32(
		vpadd_s32(vget_low_s32(mul_lo), vget_high_s32(mul_lo)),
		vpadd_s32(vget_low_s32(mul_hi), vget_high_s32(mul_hi))
	));
}

#define _mm_cmpeq_epi8(a, b) (vreinterpretq_s8_u8(vceqq_s8(a, b)))
#define _mm_cmplt_epi8(a, b) (vreinterpretq_s8_u8(vcltq_s8(a, b)))
#define _mm_cmpgt_epi8(a, b) (vreinterpretq_s8_u8(vcgtq_s8(a, b)))

#define _mm_unpacklo_epi8(a, b) vzip1q_s8(a, b)
#define _mm_unpackhi_epi8(a, b) vzip2q_s8(a, b)

#define _mm_extract_epi16(x, imm) vgetq_lane_s16(vreinterpretq_s16_s8(x), imm)
#define _mm_cvtsi128_si32(x) vgetq_lane_s32(vreinterpretq_s32_s8(x), 0)

#define _MM_SHUFFLE(a, b, c, d) (((a) << 6) | ((b) << 4) | ((c) << 2) | (d))
#define _mm_shuffle_epi32(x, imm) \
	vreinterpretq_s8_s32((int32x4_t) { \
		(int32_t) vgetq_lane_s32(vreinterpretq_s32_s8(x), (imm >> 0) & 0x3), \
		(int32_t) vgetq_lane_s32(vreinterpretq_s32_s8(x), (imm >> 2) & 0x3), \
		(int32_t) vgetq_lane_s32(vreinterpretq_s32_s8(x), (imm >> 4) & 0x3), \
		(int32_t) vgetq_lane_s32(vreinterpretq_s32_s8(x), (imm >> 6) & 0x3) \
	})

static zend_always_inline int _mm_movemask_epi8(__m128i x)
{
    /**
     * based on code from
     * https://community.arm.com/arm-community-blogs/b/servers-and-cloud-computing-blog/posts/porting-x86-vector-bitmask-optimizations-to-arm-neon
     */
    uint16x8_t high_bits = vreinterpretq_u16_u8(vshrq_n_u8(vreinterpretq_u8_s8(x), 7));
    uint32x4_t paired16 = vreinterpretq_u32_u16(vsraq_n_u16(high_bits, high_bits, 7));
    uint64x2_t paired32 = vreinterpretq_u64_u32(vsraq_n_u32(paired16, paired16, 14));
    uint8x16_t paired64 = vreinterpretq_u8_u64(vsraq_n_u64(paired32, paired32, 28));
    return vgetq_lane_u8(paired64, 0) | ((int) vgetq_lane_u8(paired64, 8) << 8);
}

#endif

#endif       /* ZEND_SIMD_H */
