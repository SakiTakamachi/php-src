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
#ifdef __SSE2__
# include <emmintrin.h>
#endif


/* Compare two bc numbers.  Return value is 0 if equal, -1 if N1 is less
   than N2 and +1 if N1 is greater than N2.  If USE_SIGN is false, just
   compare the magnitudes. */

static zend_always_inline int _bc_abs_do_compare(bc_num n1, bc_num n2)
{
	char *n1ptr, *n2ptr;

	/* Now compare the magnitude. */
	if (n1->n_len != n2->n_len) {
		return (n1->n_len > n2->n_len) ? 1 : -1;
	}

	/* If we get here, they have the same number of integer digits.
	   check the integer part and the equal length part of the fraction. */
	size_t count = n1->n_len + MIN (n1->n_scale, n2->n_scale);
	n1ptr = n1->n_value;
	n2ptr = n2->n_value;

#ifdef __SSE2__
	while (count >= sizeof(__m128i)) {
		__m128i n1bytes = _mm_loadu_si128((const __m128i *) n1ptr);
		__m128i n2bytes = _mm_loadu_si128((const __m128i *) n2ptr);

		n1bytes = _mm_cmpeq_epi8(n1bytes, n2bytes);

		int mask = _mm_movemask_epi8(n1bytes);
		if (mask != 0xffff) {
#ifdef PHP_HAVE_BUILTIN_CTZL
			int shift = __builtin_ctz(~mask);
			return (n1ptr[shift] > n2ptr[shift]) ? 1 : -1;
#else
			break;
#endif
		}

		count -= sizeof(__m128i);
		n1ptr += sizeof(__m128i);
		n2ptr += sizeof(__m128i);
	}
#endif

	while ((count > 0) && (*n1ptr == *n2ptr)) {
		n1ptr++;
		n2ptr++;
		count--;
	}

	if (count != 0) {
		return (*n1ptr > *n2ptr) ? 1 : -1;
	}

	/* They are equal up to the last part of the equal part of the fraction. */
	if (n1->n_scale != n2->n_scale) {
		if (n1->n_scale > n2->n_scale) {
			for (count = n1->n_scale - n2->n_scale; count > 0; count--) {
				if (*n1ptr++ != 0) {
					/* Magnitude of n1 > n2. */
					return 1;
				}
			}
		} else {
			for (count = n2->n_scale - n1->n_scale; count > 0; count--) {
				if (*n2ptr++ != 0) {
					/* Magnitude of n1 < n2. */
					return -1;
				}
			}
		}
	}

	/* They must be equal! */
	return 0;
}


/* This is the "user callable" routine to compare numbers N1 and N2. */
int bc_compare(bc_num n1, bc_num n2)
{
	/* First, compare signs. */
	if (n1->n_sign != n2->n_sign) {
		if (n1->n_sign == PLUS) {
			/* Positive N1 > Negative N2 */
			return 1;
		} else {
			/* Negative N1 < Positive N1 */
			return -1;
		}
	}

	int ret = _bc_abs_do_compare(n1, n2);
	return (n1->n_sign == MINUS) ? -ret : ret;
}

int bc_abs_compare(bc_num n1, bc_num n2)
{
	return _bc_abs_do_compare(n1, n2);
}
