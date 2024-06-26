/* div.c: bcmath library file. */
/*
    Copyright (C) 1991, 1992, 1993, 1994, 1997 Free Software Foundation, Inc.
    Copyright (C) 2000 Philip A. Nelson

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.  (LICENSE)

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to:

      The Free Software Foundation, Inc.
      59 Temple Place, Suite 330
      Boston, MA 02111-1307 USA.

    You may contact the author by:
       e-mail:  philnelson@acm.org
      us-mail:  Philip A. Nelson
                Computer Science Department, 9062
                Western Washington University
                Bellingham, WA 98226-9062

*************************************************************************/

#include "bcmath.h"
#include "private.h"
#include "convert.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "zend_alloc.h"

static const BC_VECTOR POW_10_LUT[8] = {
	1, 10, 100, 1000, 10000, 100000, 1000000, 10000000
};

/*
 * Use this function when the number of arrays in n2 is 1.
 * In this case, there is no need to add back.
 */
static inline void bc_fast_div(BC_VECTOR *n1_vector, size_t n1_arr_size, BC_VECTOR n2_vector, BC_VECTOR *quot_vector, size_t quot_arr_size)
{
	size_t n1_top_index = n1_arr_size - 1;
	size_t quot_top_index = quot_arr_size - 1;
	for (size_t i = 0; i < quot_arr_size - 1; i++) {
		if (n1_vector[n1_top_index - i] < n2_vector) {
			quot_vector[quot_top_index - i] = 0;
			n1_vector[n1_top_index - i - 1] += n1_vector[n1_top_index - i] * BC_VECTOR_BOUNDARY_NUM;
			continue;
		}
		quot_vector[quot_top_index - i] = n1_vector[n1_top_index - i] / n2_vector;
		n1_vector[n1_top_index - i] -= n2_vector * quot_vector[quot_top_index - i];
		n1_vector[n1_top_index - i - 1] += n1_vector[n1_top_index - i] * BC_VECTOR_BOUNDARY_NUM;
		n1_vector[n1_top_index - i] = 0;
	}
	/* last */
	quot_vector[0] = n1_vector[0] / n2_vector;
}

/*
 * Used when the number of elements in the n2 vector is 2 or more.
 */
static inline void bc_standard_div(BC_VECTOR *n1_vector, size_t n1_arr_size, BC_VECTOR *n2_vector, size_t n2_arr_size, BC_VECTOR *quot_vector, size_t quot_arr_size)
{
	size_t i, j;
	size_t n1_top_index = n1_arr_size - 1;
	size_t n2_top_index = n2_arr_size - 1;
	size_t quot_top_index = quot_arr_size - 1;

	BC_VECTOR div_carry = 0;

	/*
	 * If calculate the quotient normally, there will be a maximum error of 10.
	 * Therefore, when calculating the temporary quotient, use the value multiplied by 10 to keep the error within 1.
	 *
	 * Detail:
	 * The error of the quotient can be expressed as follows. (E is max error)
	 * n1/n2 - n1/(n2 + 1) <= E
	 * (n1(n2 + 1) - n1 * n2) / n2(n2 + 1) <= E
	 * n1/(n2 * (n2 + 1)) <= E
	 *
	 * n2 always takes the maximum number of digits below the radix. In other words, if the base is 10000, n2 has been
	 * adjusted to have a value in the range 1000 to 9999.
	 *
	 * First, let's transform the formula, assuming that it will be calculated without any special effort.
	 * To maximize E, set the value so that the numerator is large and the denominator is small.
	 * Considering carry-back, the maximum possible value of n1, assuming the base number is B, is "B(n2 - 1) + A".
	 * A is a value less than the radix. The maximum value of A is "B - 1". And the minimum value of n2 is B/10 since
	 * have adjusted it as already mentioned.
	 *
	 * Therefore, the formula can be expressed as:
	 * (B(n2 - 1) + A) / (n2 * (n2 + 1)) = E
	 * (B(B/10 - 1) + B - 1) / (B/10 * (B/10 + 1)) = E
	 * (10B^2 - 1) / (B^2 + 10B) = E
	 *
	 * Here, B is 10000 for 32-bit and 100000000 for 64-bit.
	 * 32-bit: 999999999 / 100100000 = 9.99000998001998001998
	 * 64-bit: 99999999999999999 / 10000001000000000 = 9.99999900000009989999
	 * Therefore, in both cases, the maximum error value is approximately 10.
	 *
	 * To keep the error within 1, increase the base by one digit only when calculating the quotient.
	 * The important thing here is that only the number of carry digits that are carried down does not change.
	 * (B(n2 - 1) + A) / (n2 * (n2 + 1)) = E
	 *
	 * increase base number. Assume that A' takes the maximum value at "10B - 1". Only "n2 - 1", which is a carry-down, remains unchanged.
	 * (10B(n2 - 1) + A') / (10n2 * (10n2 + 1)) = E
	 * (10B(B/10 - 1) + 10B - 1) / (B/10 * (B/10 + 1)) = E
	 * (B^2 - 1) / (B^2 + 10B) = E
	 *
	 * Able to reduce the number of digits in the numerator by one. In other words, the error is within 1.
	 */
	BC_VECTOR n2_top = n2_vector[n2_top_index] * 10 + n2_vector[n2_top_index - 1] / (BC_VECTOR_BOUNDARY_NUM / 10);

	for (i = 0; i < quot_arr_size; i++) {
		BC_VECTOR n1_top = n1_vector[n1_top_index - i] * 10 + n1_vector[n1_top_index - i - 1] / (BC_VECTOR_BOUNDARY_NUM / 10);

		/* If it is clear that n2 is greater in this loop, then the quotient is 0. */
		if (div_carry == 0 && n1_top < n2_top) {
			quot_vector[quot_top_index - i] = 0;
			div_carry = n1_vector[n1_top_index - i];
			n1_vector[n1_top_index - i] = 0;
			continue;
		}

		/* Determine the temporary quotient. */
		n1_top += div_carry * BC_VECTOR_BOUNDARY_NUM * 10;
		BC_VECTOR quot_guess = n1_top / n2_top;

		/* For sub, add the remainder to the high-order digit */
		n1_vector[n1_top_index - i] += div_carry * BC_VECTOR_BOUNDARY_NUM;

		/*
		 * The temporary quotient can only have errors in the "too large" direction.
		 * So if the temporary quotient is 0 here, the quotient is 0.
		 */
		if (quot_guess == 0) {
			quot_vector[quot_top_index - i] = 0;
			div_carry = n1_vector[n1_top_index - i];
			n1_vector[n1_top_index - i] = 0;
			continue;
		}

		/* sub */
		BC_VECTOR sub;
		BC_VECTOR borrow = 0;
		BC_VECTOR *n1_calc_bottom = n1_vector + n1_arr_size - n2_arr_size - i;
		for (j = 0; j < n2_arr_size - 1; j++) {
			sub = n2_vector[j] * quot_guess + borrow;
			BC_VECTOR sub_low = sub % BC_VECTOR_BOUNDARY_NUM;
			borrow = sub / BC_VECTOR_BOUNDARY_NUM;

			if (n1_calc_bottom[j] >= sub_low) {
				n1_calc_bottom[j] -= sub_low;
			} else {
				n1_calc_bottom[j] += BC_VECTOR_BOUNDARY_NUM - sub_low;
				borrow++;
			}
		}
		/* last digit sub */
		sub = n2_vector[j] * quot_guess + borrow;
		bool neg_remainder = n1_calc_bottom[j] < sub;
		n1_calc_bottom[j] -= sub;

		/* If the temporary quotient is too large, add back n2 */
		BC_VECTOR carry = 0;
		if (neg_remainder) {
			quot_guess--;
			for (j = 0; j < n2_arr_size - 1; j++) {
				n1_calc_bottom[j] += n2_vector[j] + carry;
				carry = n1_calc_bottom[j] / BC_VECTOR_BOUNDARY_NUM;
				n1_calc_bottom[j] %= BC_VECTOR_BOUNDARY_NUM;
			}
			/* last add */
			n1_calc_bottom[j] += n2_vector[j] + carry;
		}

		quot_vector[quot_top_index - i] = quot_guess;
		div_carry = n1_vector[n1_top_index - i];
		n1_vector[n1_top_index - i] = 0;
	}
}

static void bc_do_div(char *n1, size_t n1_readable_len, size_t n1_bottom_extension, char *n2, size_t n2_len, bc_num *quot, size_t quot_len)
{
	size_t i;

	/* Want to left-justify n2 and convert it into a vector, so fill the right side with zeros. Fill in n1 by the same amount. */
	size_t n2_arr_size = (n2_len + BC_VECTOR_SIZE - 1) / BC_VECTOR_SIZE;
	size_t n2_vector_pad_bottom_zeros = n2_arr_size * BC_VECTOR_SIZE - n2_len;
	size_t n1_vector_pad_bottom_zeros = n1_bottom_extension + n2_vector_pad_bottom_zeros;
	size_t n1_arr_size = (n1_readable_len + n1_vector_pad_bottom_zeros + BC_VECTOR_SIZE - 1) / BC_VECTOR_SIZE;
	size_t quot_arr_size = n1_arr_size - n2_arr_size + 1;
	size_t quot_real_arr_size = MIN(quot_arr_size, (quot_len + BC_VECTOR_SIZE - 1) / BC_VECTOR_SIZE);

	BC_VECTOR *n1_vector = safe_emalloc(n1_arr_size + n2_arr_size + quot_arr_size, sizeof(BC_VECTOR), 0);
	BC_VECTOR *n2_vector = n1_vector + n1_arr_size;
	BC_VECTOR *quot_vector = n2_vector + n2_arr_size;

	/* Fill with zeros and convert as many vector elements as need */
	size_t n1_vector_count = 0;
	while (n1_vector_pad_bottom_zeros >= BC_VECTOR_SIZE) {
		n1_vector[n1_vector_count] = 0;
		n1_vector_pad_bottom_zeros -= BC_VECTOR_SIZE;
		n1_vector_count++;
	}

	size_t n1_bottom_read_len = BC_VECTOR_SIZE - n1_vector_pad_bottom_zeros;
	size_t n2_bottom_read_len = BC_VECTOR_SIZE - n2_vector_pad_bottom_zeros;

	size_t base;
	size_t n1_read = 0;
	if (n1_bottom_read_len < BC_VECTOR_SIZE) {
		n1_read = MIN(n1_bottom_read_len, n1_readable_len);
		base = POW_10_LUT[n1_vector_pad_bottom_zeros];
		n1_vector[n1_vector_count] = 0;
		for (i = 0; i < n1_read; i++) {
			n1_vector[n1_vector_count] += *n1 * base;
			base *= BASE;
			n1--;
		}
		n1_vector_count++;
	}
	size_t n2_read = 0;
	size_t n2_vector_count = 0;
	if (n2_bottom_read_len < BC_VECTOR_SIZE) {
		base = POW_10_LUT[n2_vector_pad_bottom_zeros];
		n2_read = n2_bottom_read_len;
		n2_vector[0] = 0;
		for (i = 0; i < n2_read; i++) {
			n2_vector[n2_vector_count] += *n2 * base;
			base *= BASE;
			n2--;
		}
		n2_vector_count++;
	}

	/* Bulk convert n1 and n2 to vectors */
	if (n1_readable_len > n1_read) {
		bc_convert_to_vector(n1_vector + n1_vector_count, n1, n1_readable_len - n1_read);
	}
	if (n2_len > n2_read) {
		bc_convert_to_vector(n2_vector + n2_vector_count, n2, n2_len - n2_read);
	}

	/* Do the divide */
	if (n2_arr_size == 1) {
		bc_fast_div(n1_vector, n1_arr_size, n2_vector[0], quot_vector, quot_arr_size);
	} else {
		bc_standard_div(n1_vector, n1_arr_size, n2_vector, n2_arr_size, quot_vector, quot_arr_size);
	}

	/* Convert to bc_num */
	char *qptr = (*quot)->n_value;
	char *qend = qptr + quot_len - 1;

	i = 0;
	for (i = 0; i < quot_real_arr_size - 1; i++) {
#if BC_VECTOR_SIZE == 4
		bc_write_bcd_representation(quot_vector[i], qend - 3);
		qend -= 4;
#else
		bc_write_bcd_representation(quot_vector[i] / 10000, qend - 7);
		bc_write_bcd_representation(quot_vector[i] % 10000, qend - 3);
		qend -= 8;
#endif
	}

	while (qend >= qptr) {
		*qend-- = quot_vector[i] % BASE;
		quot_vector[i] /= BASE;
	}

	efree(n1_vector);
}

bool bc_divide(bc_num n1, bc_num n2, bc_num *quot, int scale)
{
	/* divide by zero */
	if (bc_is_zero(n2)) {
		return false;
	}

	bc_free_num(quot);

	/* If n1 is zero, the quotient is always zero. */
	if (bc_is_zero(n1)) {
		*quot = bc_copy_num(BCG(_zero_));
		return true;
	}

	char *n1ptr = n1->n_value;
	char *n1end = n1ptr + n1->n_len + n1->n_scale - 1;
	size_t n1_len = n1->n_len;
	size_t n1_scale = n1->n_scale;

	char *n2ptr = n2->n_value;
	char *n2end = n2ptr + n2->n_len + n2->n_scale - 1;
	size_t n2_len = n2->n_len;
	size_t n2_scale = n2->n_scale;
	size_t n2_int_right_zeros = 0;

	/* remove n2 trailing zeros */
	while (*n2end == 0 && n2_scale > 0) {
		n2end--;
		n2_scale--;
	}
	while (*n2end == 0) {
		n2end--;
		n2_int_right_zeros++;
	}

	if (*n1ptr == 0 && n1_len == 1) {
		n1ptr++;
		n1_len = 0;
	}

	size_t n1_top_extension = 0;
	size_t n1_bottom_extension = 0;
	if (n2_scale > 0) {
		/*
		 * e.g. n2_scale = 4
		 * n2 = .0002, to be 2 or n2 = 200.001, to be 200001
		 * n1 = .03, to be 300 or n1 = .000003, to be .03
		 * n1 may become longer than the original data length due to the addition of
		 * trailing zeros in the integer part.
		 */
		n1_len += n2_scale;
		n1_bottom_extension = n1_scale < n2_scale ? n2_scale - n1_scale : 0;
		n1_scale = n1_scale > n2_scale ? n1_scale - n2_scale : 0;
		n2_len += n2_scale;
		n2_scale = 0;
	} else if (n2_int_right_zeros > 0) {
		/*
		 * e.g. n2_int_right_zeros = 4
		 * n2 = 2000, to be 2
		 * n1 = 30, to be .03 or n1 = 30000, to be 30
		 * Also, n1 may become longer than the original data length due to the addition of
		 * leading zeros in the fractional part.
		 */
		n1_top_extension = n1_len < n2_int_right_zeros ? n2_int_right_zeros - n1_len : 0;
		n1_len = n1_len > n2_int_right_zeros ? n1_len - n2_int_right_zeros : 0;
		n1_scale += n2_int_right_zeros;
		n2_len -= n2_int_right_zeros;
		n2_scale = 0;
	}

	/* remove n1 leading zeros */
	while (*n1ptr == 0 && n1_len > 0) {
		n1ptr++;
		n1_len--;
	}
	/* remove n2 leading zeros */
	while (*n2ptr == 0) {
		n2ptr++;
		n2_len--;
	}

	/* Considering the scale specification, the quotient is always 0 if this condition is met */
	if (n2_len > n1_len + scale) {
		*quot = bc_copy_num(BCG(_zero_));
		return true;
	}

	/* set scale to n1 */
	if (n1_scale > scale) {
		size_t scale_diff = n1_scale - scale;
		if (n1_bottom_extension > scale_diff) {
			n1_bottom_extension -= scale_diff;
		} else {
			n1_bottom_extension = 0;
			n1end -= scale_diff - n1_bottom_extension;
		}
	} else {
		n1_bottom_extension += scale - n1_scale;
	}
	n1_scale = scale;

	/* Length of n1 data that can be read */
	size_t n1_readable_len = n1end - n1ptr + 1;

	/* If n2 is 1 here, return the result of adjusting the decimal point position of n1. */
	if (n2_len == 1 && *n2ptr == 1) {
		if (n1_len == 0) {
			n1_len = 1;
			n1_top_extension++;
		}
		size_t quot_scale = n1_scale > n1_bottom_extension ? n1_scale - n1_bottom_extension : 0;
		n1_bottom_extension = n1_scale < n1_bottom_extension ? n1_bottom_extension - n1_scale : 0;

		*quot = bc_new_num_nonzeroed(n1_len, quot_scale);
		char *qptr = (*quot)->n_value;
		for (size_t i = 0; i < n1_top_extension; i++) {
			*qptr++ = 0;
		}
		memcpy(qptr, n1ptr, n1_readable_len);
		qptr += n1_readable_len;
		for (size_t i = 0; i < n1_bottom_extension; i++) {
			*qptr++ = 0;
		}
		(*quot)->n_sign = n1->n_sign == n2->n_sign ? PLUS : MINUS;
		return true;
	}

	size_t quot_full_len;
	if (n2_len > n1_len) {
		*quot = bc_new_num_nonzeroed(1, scale);
		quot_full_len = 1 + scale;
	} else {
		*quot = bc_new_num_nonzeroed(n1_len - n2_len + 1, scale);
		quot_full_len = n1_len - n2_len + 1 + scale;
	}

	/* do divide */
	bc_do_div(n1end, n1_readable_len, n1_bottom_extension, n2end, n2_len, quot, quot_full_len);
	(*quot)->n_sign = n1->n_sign == n2->n_sign ? PLUS : MINUS;
	_bc_rm_leading_zeros(*quot);

	return true;
}
