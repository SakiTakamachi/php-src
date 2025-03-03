/* num2long.c: bcmath library file. */
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
#include <stddef.h>

/* Convert a number NUM to a long.  The function returns only the integer
   part of the number.  For numbers that are too large to represent as
   a long, this function returns a zero.  This can be detected by checking
   the NUM for zero after having a zero returned. */

#if BC_VECTOR_SIZE == 8
long bc_num2long(bc_num num)
{
	/* Extract the int value, ignore the fraction. */

	/* Just by checking the vector size of the integer part, we can see that it does not fit in LONG. */
	if (UNEXPECTED(num->n_int_vsize > 2)) {
		return 0;
	}

	BC_VECTOR *int_vptr = BC_VECTORS_INT_LOWER_PTR(num);
	BC_VECTOR tmp = 0;
	BC_VECTOR base = 1;
	for (size_t i = 0; i < num->n_int_vsize; i++) {
		tmp += int_vptr[i] * base;
		base *= BC_VECTOR_BOUNDARY_NUM;
	}

	if (num->n_sign == MINUS) {
		if (UNEXPECTED(tmp - 1 > LONG_MAX)) {
			return 0;
		}
		return -tmp;
	} else {
		if (UNEXPECTED(tmp > LONG_MAX)) {
			return 0;
		}
		return tmp;
	}
}
#else
long bc_num2long(bc_num num)
{
	/* Extract the int value, ignore the fraction. */

	/* Just by checking the vector size of the integer part, we can see that it does not fit in LONG. */
	if (UNEXPECTED(num->n_int_vsize > 3)) {
		return 0;
	}

	BC_VECTOR *int_vptr = BC_VECTORS_INT_LOWER_PTR(num);
	BC_VECTOR tmp = 0;
	BC_VECTOR base = 1;
	for (size_t i = 0; i < num->n_int_vsize; i++) {
		tmp += int_vptr[i] * base;
		base *= BC_VECTOR_BOUNDARY_NUM;
	}

	long long_max_high_part = LONG_MAX / (BC_VECTOR_BOUNDARY_NUM * BC_VECTOR_BOUNDARY_NUM);
	long long_max_low_part = LONG_MAX % (BC_VECTOR_BOUNDARY_NUM * BC_VECTOR_BOUNDARY_NUM);

	if (num->n_int_vsize == 3) {
		if (UNEXPECTED(int_vptr[2] > long_max_high_part)) {
			return 0;
		}

		long tmp_high_part = int_vptr[2] * BC_VECTOR_BOUNDARY_NUM * BC_VECTOR_BOUNDARY_NUM;
		if (int_vptr[2] == long_max_high_part) {
			if (num->n_sign == MINUS) {
				if (UNEXPECTED(tmp > long_max_low_part + 1)) {
					return 0;
				}
				return -tmp - tmp_high_part;
			} else {
				if (UNEXPECTED(tmp > long_max_low_part)) {
					return 0;
				}
				return tmp + tmp_high_part;
			}
		}

		tmp += tmp_high_part;
	}

	if (num->n_sign == MINUS) {
		return -tmp;
	} else {
		return tmp;
	}
}
#endif
