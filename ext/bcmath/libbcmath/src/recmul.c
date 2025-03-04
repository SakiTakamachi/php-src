/* recmul.c: bcmath library file. */
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
#include <stddef.h>
#include <assert.h>
#include <stdbool.h>
#include "private.h"
#include "zend_alloc.h"


/* Multiply utility routines */

static inline void bc_mul_carry_calc(BC_VECTOR *prod_vector, size_t prod_arr_size)
{
	for (size_t i = 0; i < prod_arr_size - 1; i++) {
		prod_vector[i + 1] += prod_vector[i] / BC_VECTOR_BOUNDARY_NUM;
		prod_vector[i] %= BC_VECTOR_BOUNDARY_NUM;
	}
}

/**
 * If the vector_size of n1 and n2 are both 1, the calculation will be performed at high speed.
 */
static inline void bc_fast_mul(bc_num n1, bc_num n2, bc_num prod)
{
	BC_VECTOR tmp = n1->n_vectors[0] * n2->n_vectors[0];
	prod->n_vectors[0] = tmp % BC_VECTOR_BOUNDARY_NUM;
	prod->n_vectors[1] = tmp / BC_VECTOR_BOUNDARY_NUM;
}

/**
 * Multiply and add these groups of numbers to perform multiplication fast.
 * How much to shift the digits when adding values can be calculated from the index of the array.
 */
static void bc_standard_mul(bc_num n1, size_t n1_vsize, bc_num n2, size_t n2_vsize, bc_num prod)
{
	size_t i;

	BC_VECTOR *v1ptr = n1->n_vectors;
	BC_VECTOR *v2ptr = n2->n_vectors;
	BC_VECTOR *vpptr = prod->n_vectors;

	size_t prod_vsize = prod->n_int_vsize + prod->n_frac_vsize;

	/* Multiplication and addition */
	size_t count = 0;
	for (i = 0; i < n1_vsize; i++) {
		/*
		 * This calculation adds the result multiple times to the array entries.
		 * When multiplying large numbers of digits, there is a possibility of
		 * overflow, so digit adjustment is performed beforehand.
		 */
		if (UNEXPECTED(count >= BC_VECTOR_NO_OVERFLOW_ADD_COUNT)) {
			bc_mul_carry_calc(vpptr, prod_vsize);
			count = 0;
		}
		count++;
		for (size_t j = 0; j < n2_vsize; j++) {
			vpptr[i + j] += v1ptr[i] * v2ptr[j];
		}
	}

	/*
	 * Move a value exceeding 4/8 digits by carrying to the next digit.
	 * However, the last digit does nothing.
	 */
	bc_mul_carry_calc(vpptr, prod_vsize);
}

/* The multiply routine. N2 times N1 is put int PROD with the scale of
   the result being MIN(N2 scale+N1 scale, MAX (SCALE, N2 scale, N1 scale)).
   */
bc_num bc_multiply(bc_num n1, bc_num n2, size_t scale)
{
	bc_num prod = bc_new_num_with_vsize(
		n1->n_int_vsize + n2->n_int_vsize,
		n1->n_len + n2->n_len,
		n1->n_frac_vsize + n2->n_frac_vsize,
		n1->n_scale + n2->n_scale
	);

	/* Initialize things. */
	size_t n1_vsize = n1->n_int_vsize + n1->n_frac_vsize;
	size_t n2_vsize = n2->n_int_vsize + n2->n_frac_vsize;
	size_t prod_scale = MIN(prod->n_scale, MAX(scale, MAX(n1->n_scale, n2->n_scale)));

	/* Do the multiply */
	if (n1_vsize == 1 && n2_vsize == 1) {
		bc_fast_mul(n1, n2, prod);
	} else {
		bc_standard_mul(n1, n1_vsize, n2, n2_vsize, prod);
	}

	/* Assign to prod and clean up the number. */
	prod->n_sign = (n1->n_sign == n2->n_sign ? PLUS : MINUS);
	_bc_rm_leading_zeros(prod);
	bc_set_new_scale(prod, prod_scale);
	if (bc_is_zero(prod)) {
		prod->n_sign = PLUS;
	}
	return prod;
}

bc_num bc_square(bc_num n1, size_t scale)
{
	bc_num prod = bc_new_num_with_vsize(
		n1->n_int_vsize + n1->n_int_vsize,
		n1->n_len + n1->n_len,
		n1->n_frac_vsize + n1->n_frac_vsize,
		n1->n_scale + n1->n_scale
	);

	size_t n1_vsize = n1->n_int_vsize + n1->n_frac_vsize;
	size_t prod_scale = MIN(prod->n_scale, MAX(scale, n1->n_scale));

	if (n1_vsize == 1) {
		bc_fast_mul(n1, n1, prod);
	} else {
		bc_standard_mul(n1, n1_vsize, n1, n1_vsize, prod);
	}

	prod->n_sign = PLUS;
	_bc_rm_leading_zeros(prod);
	bc_set_new_scale(prod, prod_scale);

	return prod;
}
