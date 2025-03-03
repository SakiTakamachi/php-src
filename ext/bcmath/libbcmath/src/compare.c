/* compare.c: bcmath library file. */
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

#include <stdbool.h>
#include "bcmath.h"
#include "private.h"
#include <stddef.h>

static inline bcmath_compare_result bc_compare_determine_return_val(bool check, sign sign, bool use_sign)
{
	if (check) {
		/* Magnitude of n1 > n2. */
		if (!use_sign || sign == PLUS) {
			return BCMATH_LEFT_GREATER;
		} else {
			return BCMATH_RIGHT_GREATER;
		}
	} else {
		/* Magnitude of n1 < n2. */
		if (!use_sign || sign == PLUS) {
			return BCMATH_RIGHT_GREATER;
		} else {
			return BCMATH_LEFT_GREATER;
		}
	}
}

/* Compare two bc numbers.  Return value is 0 if equal, -1 if N1 is less
   than N2 and +1 if N1 is greater than N2.  If USE_SIGN is false, just
   compare the magnitudes. */
bcmath_compare_result _bc_do_compare(bc_num n1, bc_num n2, size_t scale, bool use_sign)
{
	/* First, compare signs. */
	if (use_sign && n1->n_sign != n2->n_sign) {
		/*
		 * scale and n->n_scale differ only in Number objects.
		 * This happens when explicitly specify the scale in a Number method.
		 */
		if ((n1->n_scale > scale || n2->n_scale > scale) &&
			n1->n_len == 1 && n2->n_len == 1 &&
			n1->n_vectors[0] == 0 && n2->n_vectors[0] == 0 &&
			bc_is_zero_for_scale(n1, scale) && bc_is_zero_for_scale(n2, scale)
		) {
			/* e.g. 0.00 <=> -0.00 */
			return BCMATH_EQUAL;
		}
		if (n1->n_sign == PLUS) {
			/* Positive N1 > Negative N2 */
			return BCMATH_LEFT_GREATER;
		} else {
			/* Negative N1 < Positive N1 */
			return BCMATH_RIGHT_GREATER;
		}
	}

	/* Now compare the magnitude. */
	if (n1->n_int_vsize != n2->n_int_vsize) {
		return bc_compare_determine_return_val(n1->n_int_vsize > n2->n_int_vsize, n1->n_sign, use_sign);
	}

	size_t num_min_scale;
	size_t num_max_scale;
	size_t min_frac_vsize;
	size_t max_frac_vsize;
	size_t int_vsize = n1->n_int_vsize; // n1->n_int_vsize == n2->n_int_vsize
	if (n1->n_scale > n2->n_scale) {
		num_min_scale = n2->n_scale;
		num_max_scale = n1->n_scale;
		min_frac_vsize = n2->n_frac_vsize;
		max_frac_vsize = MIN(scale / BC_VECTOR_SIZE, n1->n_frac_vsize);
	} else {
		num_min_scale = n1->n_scale;
		num_max_scale = n2->n_scale;
		min_frac_vsize = n1->n_frac_vsize;
		max_frac_vsize = MIN(scale / BC_VECTOR_SIZE, n2->n_frac_vsize);
	}

	size_t check_scale;
	size_t check_vsize;
	if (num_min_scale <= scale) {
		check_scale = num_min_scale;
		check_vsize = int_vsize + min_frac_vsize;
	} else {
		check_scale = scale;
		check_vsize = int_vsize + check_scale / BC_VECTOR_SIZE;
	}

	BC_VECTOR *n1ptr = BC_VECTORS_UPPER_PTR(n1);
	BC_VECTOR *n2ptr = BC_VECTORS_UPPER_PTR(n2);

	size_t count = check_vsize;
	for (; count > 0; count--) {
		if (*n1ptr != *n2ptr) {
			return bc_compare_determine_return_val(*n1ptr > *n2ptr, n1->n_sign, use_sign);
		}
		n1ptr--;
		n2ptr--;
	}

	size_t protruded_scale = BC_PROTRUNDED_LEN_FROM_LEN(scale);

	if (num_min_scale > scale) {
		/**
		 * scale < num_min_scale <= num_max_scale
		 * Compare protruded scale. Since the scale to be checked for n1 and n2 is the same,
		 * the check is completed at the same time.
		 */
		if (protruded_scale > 0) {
			BC_VECTOR n1_tmp = BC_EXTRACT_UPPER_DIGIT(*n1ptr, protruded_scale);
			BC_VECTOR n2_tmp = BC_EXTRACT_UPPER_DIGIT(*n2ptr, protruded_scale);

			if (n1_tmp != n2_tmp) {
				return bc_compare_determine_return_val(n1_tmp > n2_tmp, n1->n_sign, use_sign);
			}
		}
		return BCMATH_EQUAL;
	}

	if (num_min_scale == num_max_scale) {
		/* num_min_scale == num_max_scale <= scale */
		return BCMATH_EQUAL;
	}

	/**
	 * Checking for shorter num is done.
	 * Scales of n1 and n2 are not equal, check whether the longer value is 0.
	 */
	count = int_vsize + max_frac_vsize - check_vsize;
	BC_VECTOR *n_ex_ptr = n1->n_scale > n2->n_scale ? n1ptr : n2ptr;
	for (; count > 0; count--) {
		if (*n_ex_ptr != 0) {
			return bc_compare_determine_return_val(n1->n_scale > n2->n_scale, n1->n_sign, use_sign);
		}
		n_ex_ptr--;
	}

	/* num_min_scale <= scale < num_max_scale, so last check */
	if (protruded_scale > 0) {
		BC_VECTOR n_ex_tmp = BC_EXTRACT_UPPER_DIGIT(*n_ex_ptr, protruded_scale);
		if (n_ex_tmp != 0) {
			return bc_compare_determine_return_val(n1->n_scale > n2->n_scale, n1->n_sign, use_sign);
		}
	}
	return BCMATH_EQUAL;
}


/* This is the "user callable" routine to compare numbers N1 and N2. */
bcmath_compare_result bc_compare(bc_num n1, bc_num n2, size_t scale)
{
	return _bc_do_compare(n1, n2, scale, true);
}
