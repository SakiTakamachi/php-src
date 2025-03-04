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

void bc_set_new_scale(bc_num num, size_t scale)
{
	/* There is no point in making it the greater, so return early. */
	if (num->n_scale < scale) {
		return;
	}

	size_t new_frac_vsize = BC_LENGTH_TO_VECTOR_SIZE(scale);

	num->n_scale = scale;
	num->n_vectors += num->n_frac_vsize - new_frac_vsize;
	num->n_frac_vsize = new_frac_vsize;

	size_t protruded_scale = BC_PROTRUNDED_LEN_FROM_LEN(scale);
	if (protruded_scale == 0) {
		return;
	}

	/* If vector contains unwanted fractional parts, set them to zero. */
	*num->n_vectors = BC_REPLACE_LOWER_WITH_ZEROS(*num->n_vectors, protruded_scale);
}
