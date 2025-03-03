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

size_t bc_get_int_protruded_length(bc_num num)
{
	BC_VECTOR *vptr = BC_VECTORS_UPPER_PTR(num);
	for (size_t i = 0; i < num->n_int_vsize; i++) {
		if (*vptr != 0) {
			break;
		}
		vptr--;
	}

	BC_VECTOR tmp = *vptr;
	BC_VECTOR higher;
	size_t protruded_len = BC_VECTOR_SIZE;
	for (size_t i = BC_VECTOR_SIZE / 2; i > 0; i /= 2) {
		higher = tmp / BC_POW_10_LUT[i];

		if (higher > 0) {
			tmp = higher;
		} else {
			tmp %= BC_POW_10_LUT[i];
			protruded_len -= i;
		}
	}

	return protruded_len % BC_VECTOR_SIZE;
}
