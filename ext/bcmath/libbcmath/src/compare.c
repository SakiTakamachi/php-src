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

static inline bcmath_compare_result bc_compare_determine_return_val(bool check, sign n1_sign, bool use_sign)
{
	if (check) {
		/* Magnitude of n1 > n2. */
		if (!use_sign || n1_sign == PLUS) {
			return BCMATH_LEFT_GREATER;
		} else {
			return BCMATH_RIGHT_GREATER;
		}
	} else {
		/* Magnitude of n1 < n2. */
		if (!use_sign || n1_sign == PLUS) {
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
	BC_VECTOR *n1ptr, *n2ptr, *n_ex_ptr;

	/* First, compare signs. */
	if (use_sign && n1->n_sign != n2->n_sign) {
		/*
		 * scale and n->n_scale differ only in Number objects.
		 * This happens when explicitly specify the scale in a Number method.
		 */
		if ((n1->n_scale > scale || n2->n_scale > scale) &&
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

	size_t n1_scale = MIN(n1->n_scale, scale);
	size_t n2_scale = MIN(n2->n_scale, scale);

	/* If we get here, they have the same number of integer digits.
	   check the integer part and the equal length part of the fraction.
	   Since scale may not be a multiple of BC_VECTOR_SIZE, the protruded
	   scale is compared separately. */
	size_t min_scale = MIN(n1_scale, n2_scale);
	size_t min_protruded_scale = min_scale % BC_VECTOR_SIZE;
	size_t count = min_scale / BC_VECTOR_SIZE;
	n1ptr = BC_VECTORS_UPPER_PTR(n1);
	n2ptr = BC_VECTORS_UPPER_PTR(n2);

	while ((count > 0) && (*n1ptr == *n2ptr)) {
		n1ptr--;
		n2ptr--;
		count--;
	}

	if (count != 0) {
		/* Can see that the two values are different without taking into account the protruded scale. */
		return bc_compare_determine_return_val(*n1ptr > *n2ptr, n1->n_sign, use_sign);
	}

	if (min_protruded_scale > 0) {
		/* Compare the protruded scale. */
		size_t base = BC_POW_10_LUT[BC_VECTOR_SIZE - min_protruded_scale];
		n1ptr--;
		n2ptr--;
		BC_VECTOR n1_tmp = *n1ptr / base;
		BC_VECTOR n2_tmp = *n2ptr / base;

		if (n1_tmp != n2_tmp) {
			return bc_compare_determine_return_val(n1_tmp > n2_tmp, n1->n_sign, use_sign);
		}

		if (n1_scale == n2_scale) {
			/* No more numbers to check. */
			return BCMATH_EQUAL;
		}

		/**
		 * If the scales of n1 and n2 are not equal, check whether the longer value is 0.
		 * However, since comparisons beyond the scale are meaningless, impose a limit.
		 * Also, the comparison here is performed within the range of BC_VECTOR_SIZE.
		 */
		size_t scale_diff;
		if (n1_scale > n2_scale) {
			n_ex_ptr = n1ptr;
			scale_diff = n1_scale - n2_scale;
		} else {
			n_ex_ptr = n2ptr;
			scale_diff = n2_scale - n1_scale;
		}

		/* If the difference between the scales of n1 and n2 is too large, check within the BC_VECTOR_SIZE range. */
		size_t scale_remaining_in_vector = BC_VECTOR_SIZE - min_protruded_scale;
		base = BC_POW_10_LUT[MIN(scale_diff, scale_remaining_in_vector)];

		if ((*n_ex_ptr - n1_tmp) / base != 0) {
			return bc_compare_determine_return_val(n_ex_ptr == n1ptr, n1->n_sign, use_sign);
		}

		if (scale_diff <= scale_remaining_in_vector) {
			/* No more numbers to check. */
			return BCMATH_EQUAL;
		}
	}

	if (n1_scale == n2_scale) {
		/* No more numbers to check. */
		return BCMATH_EQUAL;
	}

	/**
	 * If the processing has progressed this far, the difference in value between n1_scale and n2_scale is
	 * large and the comparison is not yet complete.
	 * Since only the fractional part of either n1 or n2 remains, check whether it is 0.
	 * Will check the protruded scale later, so will check up to that point.
	 */
	size_t checked_frac_vsize = BC_LENGTH_TO_VECTOR_SIZE(min_scale);
	size_t max_scale;
	if (n1_scale > n2_scale) {
		n_ex_ptr = n1ptr;
		max_scale = n1_scale;
	} else {
		n_ex_ptr = n2ptr;
		max_scale = n2_scale;
	}
	count = max_scale / BC_VECTOR_SIZE - checked_frac_vsize;
	size_t max_protruded_scale = max_scale % BC_VECTOR_SIZE;

	while (count > 0 && *n_ex_ptr != 0) {
		n_ex_ptr--;
		count--;
	}

	if (count != 0) {
		/* Can see that the two values are different without taking into account the protruded scale. */
		return bc_compare_determine_return_val(n1_scale > n2_scale, n1->n_sign, use_sign);
	}

	/**
	 * This is the last check.
	 * Check the remaining protruded scale.
	 */
	n_ex_ptr--;
	if (*n_ex_ptr / BC_POW_10_LUT[BC_VECTOR_SIZE - max_protruded_scale] != 0) {
		return bc_compare_determine_return_val(n1_scale > n2_scale, n1->n_sign, use_sign);
	}

	return BCMATH_EQUAL;
}


/* This is the "user callable" routine to compare numbers N1 and N2. */
bcmath_compare_result bc_compare(bc_num n1, bc_num n2, size_t scale)
{
	return _bc_do_compare(n1, n2, scale, true);
}
