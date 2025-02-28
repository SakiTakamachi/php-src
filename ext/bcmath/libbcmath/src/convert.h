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
   | Authors: Niels Dossche <nielsdos@php.net>                            |
   +----------------------------------------------------------------------+
*/

#include "bcmath.h"
#include "private.h"

#ifndef BCMATH_CONVERT_H
#define BCMATH_CONVERT_H

void bc_convert_int_str_to_vector(BC_VECTOR *vectors, const char *source, size_t vector_size, size_t protruded_len);
void bc_convert_frac_str_to_vector(BC_VECTOR *vectors, const char *source, size_t vector_size, size_t protruded_len);
void bc_write_bcd_representation(uint32_t value, char *str);

#endif
