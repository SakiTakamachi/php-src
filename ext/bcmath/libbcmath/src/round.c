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
   | Authors: Saki Takamachi <saki@php.net>                               |
   +----------------------------------------------------------------------+
*/

#include "bcmath.h"
#include "private.h"
#include <stddef.h>

void bc_round(bc_num num, zend_long precision, zend_long mode, bc_num *result)
{
	/* clear result */
	bc_free_num(result);

	size_t leading_zeros = num->n_int_vsize * BC_VECTOR_SIZE - num->n_len;
	size_t num_vsize = num->n_int_vsize + num->n_frac_vsize;

	/*
	* The following cases result in an early return:
	*
	* - When rounding to an integer part which is larger than the number
	*   e.g. Rounding 21.123 to 3 digits before the decimal point.
	* - When rounding to a greater decimal precision then the number has, the number is unchanged
	*   e.g. Rounding 21.123 to 4 digits after the decimal point.
	* - If the fractional part ends with zeros, the zeros are omitted and the number of digits in num is reduced.
	*   Meaning we might end up in the previous case.
	*/

	/* e.g. value is 0.1 and precision is -3, ret is 0 or 1000  */
	if (precision < 0 && num->n_len < (size_t) (-(precision + Z_L(1))) + 1) {
		switch (mode) {
			case PHP_ROUND_HALF_UP:
			case PHP_ROUND_HALF_DOWN:
			case PHP_ROUND_HALF_EVEN:
			case PHP_ROUND_HALF_ODD:
			case PHP_ROUND_TOWARD_ZERO:
				*result = bc_copy_num(BCG(_zero_));
				return;

			case PHP_ROUND_CEILING:
				if (num->n_sign == MINUS) {
					*result = bc_copy_num(BCG(_zero_));
					return;
				}
				break;

			case PHP_ROUND_FLOOR:
				if (num->n_sign == PLUS) {
					*result = bc_copy_num(BCG(_zero_));
					return;
				}
				break;

			case PHP_ROUND_AWAY_FROM_ZERO:
				break;

			EMPTY_SWITCH_DEFAULT_CASE()
		}

		if (bc_is_zero(num)) {
			*result = bc_copy_num(BCG(_zero_));
			return;
		}

		/* If precision is -3, it becomes 1000. */
		if (UNEXPECTED(precision == ZEND_LONG_MIN)) {
			*result = bc_new_num((size_t) ZEND_LONG_MAX + 2, 0);
		} else {
			*result = bc_new_num(-precision + 1, 0);
		}
		BC_VECTOR *vptr = BC_VECTORS_UPPER_PTR(*result);
		*vptr = BC_POW_10_LUT[-precision % BC_VECTOR_SIZE];
		(*result)->n_sign = num->n_sign;
		return;
	}

	/* Just like bcadd('1', '1', 4) becomes '2.0000', it pads with zeros at the end if necessary. */
	if (precision >= 0 && num->n_scale <= precision) {
		if (num->n_scale == precision) {
			*result = bc_copy_num(num);
		} else if(num->n_scale < precision) {
			size_t result_frac_vsize = BC_LENGTH_TO_VECTOR_SIZE(precision);
			*result = bc_new_num_nonzeroed_with_vsize(num->n_int_vsize, num->n_len, result_frac_vsize, precision);
			(*result)->n_sign = num->n_sign;

			size_t i = 0;
			/* copy */
			for (; i < num_vsize; i++) {
				(*result)->n_vectors[i] = num->n_vectors[i];
			}
			/* padding zeros */
			for (; i < result_frac_vsize - num_vsize; i++) {
				(*result)->n_vectors[i] = 0;
			}
		}
		return;
	}

	/*
	 * If the calculation result is a negative value, there is an early return,
	 * so no underflow will occur.
	 */
	size_t rounded_len = num->n_len + precision;
	size_t rounded_vsize = BC_LENGTH_TO_VECTOR_SIZE(rounded_len + leading_zeros);
	size_t rounded_protruded_len = BC_PROTRUNDED_LEN_FROM_LEN(rounded_len + leading_zeros);

	/*
	 * Initialize result
	 * For example, if rounded_len is 0, it means trying to round 50 to 100 or 0.
	 * If the result of rounding is carried over, it will be added later, so first set it to 0 here.
	 */
	if (rounded_len == 0) {
		*result = bc_copy_num(BCG(_zero_));
	} else {
		*result = bc_new_num(num->n_len, precision > 0 ? precision : 0);
		size_t i = 0;
		if (rounded_protruded_len > 0) {
			(*result)->n_vectors[0] = BC_REPLACE_LOWER_WITH_ZEROS((*result)->n_vectors[0], rounded_protruded_len);
			i++;
		}
		for (; i < rounded_vsize; i--) {
			(*result)->n_vectors[i] = num->n_vectors[i];
		}

	}
	(*result)->n_sign = num->n_sign;

	BC_VECTOR check_vector;
	BC_VECTOR check_val;
	size_t check_voffset;
	if (rounded_protruded_len == 0) {
		check_voffset = num_vsize - rounded_vsize - 1;
		check_vector = num->n_vectors[check_voffset];
		check_val = BC_EXTRACT_UPPER_DIGIT(check_vector, 1);
	} else {
		check_voffset = num_vsize - rounded_vsize;
		check_vector = num->n_vectors[check_voffset];
		check_val = BC_EXTRACT_UPPER_DIGIT(check_vector, rounded_protruded_len + 1) % 10;
	}

	/* Check cases that can be determined without looping. */
	switch (mode) {
		case PHP_ROUND_HALF_UP:
			if (check_val >= 5) {
				goto up;
			} else if (check_val < 5) {
				goto check_zero;
			}
			break;

		case PHP_ROUND_HALF_DOWN:
		case PHP_ROUND_HALF_EVEN:
		case PHP_ROUND_HALF_ODD:
			if (check_val > 5) {
				goto up;
			} else if (check_val < 5) {
				goto check_zero;
			}
			/* if check_val == 5, we need to look-up further digits before making a decision. */
			break;

		case PHP_ROUND_CEILING:
			if (num->n_sign != PLUS) {
				goto check_zero;
			} else if (check_val > 0) {
				goto up;
			}
			/* if check_val == 0, a loop is required for judgment. */
			break;

		case PHP_ROUND_FLOOR:
			if (num->n_sign != MINUS) {
				goto check_zero;
			} else if (check_val > 0) {
				goto up;
			}
			/* if check_val == 0, a loop is required for judgment. */
			break;

		case PHP_ROUND_TOWARD_ZERO:
			goto check_zero;

		case PHP_ROUND_AWAY_FROM_ZERO:
			if (check_val > 0) {
				goto up;
			}
			/* if check_val == 0, a loop is required for judgment. */
			break;

		EMPTY_SWITCH_DEFAULT_CASE()
	}

	/* Loop through the remaining digits. */
	if (rounded_protruded_len + 1 != BC_VECTOR_SIZE) {
		size_t lower_check_len = BC_VECTOR_SIZE - rounded_protruded_len - 1;
		if (BC_EXTRACT_LOWER_DIGIT(check_vector, lower_check_len) != 0) {
			goto up;
		}
		if (check_voffset > 0) {
			check_voffset--;
		}
	}

	for (; check_voffset > 0; check_voffset--) {
		if (num->n_vectors[check_voffset] != 0) {
			goto up;
		}
	}

	switch (mode) {
		case PHP_ROUND_HALF_DOWN:
		case PHP_ROUND_CEILING:
		case PHP_ROUND_FLOOR:
		case PHP_ROUND_AWAY_FROM_ZERO:
			goto check_zero;

		case PHP_ROUND_HALF_EVEN:
			check_vector = *(*result)->n_vectors;
			check_val = BC_EXTRACT_UPPER_DIGIT(check_vector, rounded_protruded_len);
			if (rounded_len == 0 || check_val % 2 == 0) {
				goto check_zero;
			}
			break;

		case PHP_ROUND_HALF_ODD:
			check_vector = *(*result)->n_vectors;
			check_val = BC_EXTRACT_UPPER_DIGIT(check_vector, rounded_protruded_len);
			if (rounded_len != 0 && check_val % 2 == 1) {
				goto check_zero;
			}
			break;

		EMPTY_SWITCH_DEFAULT_CASE()
	}

up:
	{
		bc_num tmp;

		if (rounded_len == 0) {
			tmp = bc_new_num(num->n_len + 1, 0);
			BC_VECTOR *vptr = BC_VECTORS_UPPER_PTR(tmp);
			if (leading_zeros == 0) {
				*vptr = 1;
			} else {
				*vptr = BC_POW_10_LUT[BC_VECTOR_SIZE - leading_zeros];
			}
			tmp->n_sign = num->n_sign;
		} else {
			bc_num scaled_one = bc_new_num_with_vsize((*result)->n_int_vsize, (*result)->n_len, (*result)->n_frac_vsize, (*result)->n_scale);
			BC_VECTOR *vptr = scaled_one->n_vectors;
			if (rounded_protruded_len > 0) {
				*vptr = BC_POW_10_LUT[BC_VECTOR_SIZE - rounded_protruded_len];
			} else {
				*vptr = 1;
			}

			tmp = _bc_do_add(*result, scaled_one);
			tmp->n_sign = (*result)->n_sign;
			bc_free_num(&scaled_one);
		}

		bc_free_num(result);
		*result = tmp;
	}

check_zero:
	if (bc_is_zero(*result)) {
		(*result)->n_sign = PLUS;
	}
}
