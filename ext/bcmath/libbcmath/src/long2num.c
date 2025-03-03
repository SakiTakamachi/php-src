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
#include <stdbool.h>
#include "convert.h"

#define BC_LONG_MAX_DIGITS (sizeof(LONG_MIN_DIGITS) - 1)

bc_num bc_long2num(zend_long lval)
{
	bc_num num;

	if (UNEXPECTED(lval == 0)) {
		num = bc_copy_num(BCG(_zero_));
		return num;
	}

	bool negative = lval < 0;
	BC_VECTOR low_carry = 0;
	if (UNEXPECTED(lval == ZEND_LONG_MIN)) {
		lval = ZEND_LONG_MAX;
		low_carry = 1;
	} else if (negative) {
		lval = -lval;
	}

	BC_VECTOR tmp[3];

	tmp[0] = lval % BC_VECTOR_BOUNDARY_NUM + low_carry;
	lval /= BC_VECTOR_BOUNDARY_NUM;

	tmp[1] = lval % BC_VECTOR_BOUNDARY_NUM;
	tmp[2] = lval / BC_VECTOR_BOUNDARY_NUM;

	size_t vector_size = 3;
	if (tmp[2] == 0) {
		vector_size = 2;
		if (tmp[1] == 0) {
			vector_size = 1;
		}
	}

	size_t len = vector_size * BC_VECTOR_SIZE;
	BC_VECTOR checker = tmp[vector_size - 1];
	for (size_t i = BC_VECTOR_SIZE / 2; i > 0; i /= 2) {
		BC_VECTOR upper = checker / BC_POW_10_LUT[i];

		if (upper > 0) {
			checker = upper;
		} else {
			checker %= BC_POW_10_LUT[i];
			len -= i;
		}
	}

	num = bc_new_num_nonzeroed_with_vsize(vector_size, len > 0 ? len : 0, 0, 0);
	BC_VECTOR *vptr = (BC_VECTOR *) num->n_vectors;

	for (size_t i = 0; i < vector_size; i++) {
		vptr[i] = tmp[i];
	}

	num->n_sign = negative ? MINUS : PLUS;

	return num;
}
