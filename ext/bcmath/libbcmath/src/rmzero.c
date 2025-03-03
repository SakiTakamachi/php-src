/* rmzero.c: bcmath library file. */
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

/* For many things, we may have leading zeros in a number NUM.
   _bc_rm_leading_zeros just moves the data "value" pointer to the
   correct place and adjusts the length. */

void _bc_rm_leading_zeros(bc_num num)
{
	BC_VECTOR *vptr = BC_VECTORS_UPPER_PTR(num);
	while (*vptr == 0 && num->n_int_vsize > 1) {
		vptr--;
		num->n_int_vsize--;
		num->n_len -= BC_VECTOR_SIZE;
	}

	BC_VECTOR tmp = *vptr;
	for (size_t i = BC_VECTOR_SIZE / 2; i > 0; i /= 2) {
		BC_VECTOR upper = tmp / BC_POW_10_LUT[i];

		if (upper > 0) {
			tmp = upper;
		} else {
			tmp %= BC_POW_10_LUT[i];
			num->n_len -= i;
		}
	}
}

void bc_rm_trailing_zeros(bc_num num)
{
	if (num->n_frac_vsize == 0) {
		return;
	}

	while (*num->n_vectors == 0 && num->n_frac_vsize > 0) {
		num->n_vectors++;
		num->n_frac_vsize--;
		num->n_scale -= BC_VECTOR_SIZE;
	}

	if (num->n_frac_vsize == 0) {
		return;
	}

	BC_VECTOR tmp = *num->n_vectors;
	for (size_t i = BC_VECTOR_SIZE / 2; i > 0; i /= 2) {
		BC_VECTOR lower = tmp % BC_POW_10_LUT[i];

		if (lower > 0) {
			tmp = lower;
		} else {
			tmp /= BC_POW_10_LUT[i];
			num->n_scale -= i;
		}
	}
}
