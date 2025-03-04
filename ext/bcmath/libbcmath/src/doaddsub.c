/* doaddsub.c: bcmath library file. */
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


/* Perform addition: N1 is added to N2 and the value is
   returned.  The signs of N1 and N2 are ignored.
   SCALE_MIN is to set the minimum scale of the result. */

bc_num _bc_do_add(bc_num n1, bc_num n2)
{
	bc_num sum;
	size_t min_int_vsize  = MIN(n1->n_int_vsize, n2->n_int_vsize);
	size_t min_frac_vsize = MIN(n1->n_frac_vsize, n2->n_frac_vsize);
	size_t overlapping_size = min_int_vsize + min_frac_vsize;
	bool carry = 0;
	size_t count;

	/* Prepare sum. */
	sum = bc_new_num_nonzeroed(MAX(n1->n_len, n2->n_len) + 1, MAX(n1->n_scale, n2->n_scale));

	/* Start with the fraction part.  Initialize the pointers. */
	BC_VECTOR *n1ptr = n1->n_vectors;
	BC_VECTOR *n2ptr = n2->n_vectors;
	BC_VECTOR *sumptr = sum->n_vectors;

	/* Add the fraction part.  First copy the longer fraction.*/
	if (n1->n_frac_vsize != min_frac_vsize) {
		/* n1 has the longer frac_vsize */
		for (count = n1->n_frac_vsize - min_frac_vsize; count > 0; count--) {
			*sumptr++ = *n1ptr++;
		}
	} else {
		/* n2 has the longer frac_vsize */
		for (count = n2->n_frac_vsize - min_frac_vsize; count > 0; count--) {
			*sumptr++ = *n2ptr++;
		}
	}

	/* Now add the remaining fraction part and equal size integer parts. */
	count = 0;
	for (; overlapping_size > 0; overlapping_size--) {
		*sumptr = *n1ptr++ + *n2ptr++ + carry;
		if (*sumptr >= BC_VECTOR_BOUNDARY_NUM) {
			*sumptr -= BC_VECTOR_BOUNDARY_NUM;
			carry = 1;
		} else {
			carry = 0;
		}
		sumptr++;
	}

	/* Now add carry the longer integer part. */
	if (n1->n_int_vsize != n2->n_int_vsize) {
		if (n2->n_int_vsize > n1->n_int_vsize) {
			n1ptr = n2ptr;
		}
		for (count = sum->n_int_vsize - 1 - min_int_vsize; count > 0; count--) {
			*sumptr = *n1ptr++ + carry;
			if (*sumptr >= BC_VECTOR_BOUNDARY_NUM) {
				*sumptr -= BC_VECTOR_BOUNDARY_NUM;
				carry = 1;
			} else {
				carry = 0;
			}
			sumptr++;
		}
	}

	/* Set final carry. */
	if (sumptr == BC_VECTORS_UPPER_PTR(sum)) {
		*sumptr = carry;
	}

	/* Adjust sum and return. */
	_bc_rm_leading_zeros(sum);
	return sum;
}


/* Perform subtraction: N2 is subtracted from N1 and the value is
   returned.  The signs of N1 and N2 are ignored.  Also, N1 is
   assumed to be larger than N2.  SCALE_MIN is the minimum scale
   of the result. */
bc_num _bc_do_sub(bc_num n1, bc_num n2)
{
	bc_num diff;
	size_t min_int_vsize  = n2->n_int_vsize;
	size_t min_frac_vsize = MIN(n1->n_frac_vsize, n2->n_frac_vsize);
	size_t overlapping_size = min_int_vsize + min_frac_vsize;
	size_t borrow = 0;
	size_t count;

	/* Allocate temporary storage. */
	diff = bc_new_num_nonzeroed(MAX(n1->n_len, n2->n_len) + 1, MAX(n1->n_scale, n2->n_scale));

	/* Initialize the subtract. */
	BC_VECTOR *n1ptr = n1->n_vectors;
	BC_VECTOR *n2ptr = n2->n_vectors;
	BC_VECTOR *diffptr = diff->n_vectors;

	/* Take care of the longer frac_vsize number. */
	if (n1->n_frac_vsize != min_frac_vsize) {
		/* n1 has the longer frac_vsize */
		for (count = n1->n_frac_vsize - min_frac_vsize; count > 0; count--) {
			*diffptr++ = *n1ptr++;
		}
	} else {
		/* n2 has the longer frac_vsize */
		for (count = n2->n_frac_vsize - min_frac_vsize; count > 0; count--) {
			BC_VECTOR val = *n2ptr + borrow;
			if (val > 0) {
				*diffptr = BC_VECTOR_BOUNDARY_NUM - val;
				borrow = 1;
			} else {
				*diffptr = 0;
				borrow = 0;
			}
			diffptr++;
			n2ptr++;
		}
	}

	/* Now do the equal frac_vsize and integer parts. */
	count = 0;
	for (; overlapping_size > 0; overlapping_size--) {
		BC_VECTOR val = *n2ptr + borrow;
		if (val > *n1ptr) {
			*diffptr = BC_VECTOR_BOUNDARY_NUM + *n1ptr - val;
			borrow = 1;
		} else {
			*diffptr = *n1ptr - val;
			borrow = 0;
		}
		diffptr++;
		n1ptr++;
		n2ptr++;
	}

	/* If n1 has more digits than n2, we now do that subtract. */
	for (count = diff->n_int_vsize - min_int_vsize; count > 0; count--) {
		if (borrow > *n1ptr) {
			*diffptr = *n1ptr + BC_VECTOR_BOUNDARY_NUM - borrow;
			borrow = 1;
		} else {
			*diffptr = *n1ptr - borrow;
			borrow = 0;
		}
		diffptr++;
		n1ptr++;
	}

	/* Clean up and return. */
	_bc_rm_leading_zeros(diff);
	return diff;
}
