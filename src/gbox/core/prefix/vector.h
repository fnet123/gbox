/*!The Graphic Box Library
 * 
 * GBox is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * GBox is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with GBox; 
 * If not, see <a href="http://www.gnu.org/licenses/"> http://www.gnu.org/licenses/</a>
 * 
 * Copyright (C) 2014 - 2015, ruki All rights reserved.
 *
 * @author      ruki
 * @file        vector.h
 * @ingroup     core
 *
 */
#ifndef GB_CORE_PREFIX_VECTOR_H
#define GB_CORE_PREFIX_VECTOR_H

/* //////////////////////////////////////////////////////////////////////////////////////
 * includes
 */
#include "type.h"

/* //////////////////////////////////////////////////////////////////////////////////////
 * macros
 */

// vectors is equal?
#define gb_vector_eq(a, b)       ((a)->x == (b)->x && (a)->y == (b)->y)

/* //////////////////////////////////////////////////////////////////////////////////////
 * extern
 */
__tb_extern_c_enter__

/* //////////////////////////////////////////////////////////////////////////////////////
 * interfaces
 */

/*! make vector
 *
 * @param vector        the vector
 * @param x             the x
 * @param y             the y
 */
tb_void_t               gb_vector_make(gb_vector_ref_t vector, gb_float_t x, gb_float_t y);

/*! make vector with the integer value
 *
 * @param vector        the vector
 * @param x             the x
 * @param y             the y
 */
tb_void_t               gb_vector_imake(gb_vector_ref_t vector, tb_long_t x, tb_long_t y);

/*! make vector from the given point
 *
 * @param vector        the vector
 * @param point         the point 
 */
tb_void_t               gb_vector_make_from_point(gb_vector_ref_t vector, gb_point_ref_t point);

/*! make vector from the given two points
 *
 * @param vector        the vector
 * @param before        the before point 
 * @param after         the after point 
 */
tb_void_t               gb_vector_make_from_two_points(gb_vector_ref_t vector, gb_point_ref_t before, gb_point_ref_t after);

/*! make the unit vector
 *
 * @param vector        the vector
 * @param x             the x
 * @param y             the y
 *
 * @return              tb_true or tb_false
 */
tb_bool_t               gb_vector_make_unit(gb_vector_ref_t vector, gb_float_t x, gb_float_t y);

/*! make unit vector with the integer value
 *
 * @param vector        the vector
 * @param x             the x
 * @param y             the y
 *
 * @return              tb_true or tb_false
 */
tb_bool_t               gb_vector_imake_unit(gb_vector_ref_t vector, tb_long_t x, tb_long_t y);

/*! nagate the vector
 * 
 * @param vector        the vector
 */
tb_void_t               gb_vector_negate(gb_vector_ref_t vector);

/*! nagate the vector to the given vector
 * 
 * @param vector        the vector
 * @param negated       the negated vector
 */
tb_void_t               gb_vector_negate2(gb_vector_ref_t vector, gb_vector_ref_t negated);

/*! rotate 90 degrees 
 * 
 * @param vector        the vector
 * @param direction     the direction
 */
tb_void_t               gb_vector_rotate(gb_vector_ref_t vector, tb_size_t direction);

/*! rotate 90 degrees to the given vector
 * 
 * @param vector        the vector
 * @param rotated       the rotated vector
 * @param direction     the direction
 */
tb_void_t               gb_vector_rotate2(gb_vector_ref_t vector, gb_vector_ref_t rotated, tb_size_t direction);

/*! scale the given value
 * 
 * @param vector        the vector
 * @param scale         the scale
 */
tb_void_t               gb_vector_scale(gb_vector_ref_t vector, gb_float_t scale);

/*! scale the given value to the given vector
 * 
 * @param vector        the vector
 * @param scaled        the scaled vector
 * @param scale         the scale
 */
tb_void_t               gb_vector_scale2(gb_vector_ref_t vector, gb_vector_ref_t scaled, gb_float_t scale);

/*! the vector length
 * 
 * @param vector        the vector
 *
 * @return              the vector length
 */
gb_float_t              gb_vector_length(gb_vector_ref_t vector);

/*! set the vector length
 * 
 * @param vector        the vector
 * @param length        the vector length
 *
 * @return              tb_true or tb_false
 */
tb_bool_t               gb_vector_length_set(gb_vector_ref_t vector, gb_float_t length);

/*! can normalize the vector?
 * 
 * @param vector        the vector
 * 
 * @return              tb_true or tb_false
 */
tb_bool_t               gb_vector_can_normalize(gb_vector_ref_t vector);

/*! normalize the vector
 * 
 * @param vector        the vector
 * 
 * @return              tb_true or tb_false
 */
tb_bool_t               gb_vector_normalize(gb_vector_ref_t vector);

/*! normalize to the given vector
 * 
 * @param vector        the vector
 * @param normalized    the normalized vector
 * 
 * @return              tb_true or tb_false
 */
tb_bool_t               gb_vector_normalize2(gb_vector_ref_t vector, gb_vector_ref_t normalized);

/*! compute the dot product for the two vectors
 *
 * dot = |vector| * |other| * cos(a)
 *
 * @param vector        the vector
 * @param other         the other vector
 *
 * @return              the dot product
 */
gb_float_t              gb_vector_dot(gb_vector_ref_t vector, gb_vector_ref_t other);

/*! compute the cross product for the two vectors
 * 
 * cross = |vector| * |other| * sin(a)
 *
 * @param vector        the vector
 * @param other         the other vector
 *
 * @return              the cross product
 */
gb_float_t              gb_vector_cross(gb_vector_ref_t vector, gb_vector_ref_t other);

/*! the other vector is in the clockwise of the vector
 *
 * <pre>
 * . . . . . . . . . vector
 * .
 * .
 * .
 * .
 * other
 * </pre>
 * 
 * @param vector        the vector
 * @param other         the other vector
 * 
 * @return              tb_true or tb_false
 */
tb_bool_t               gb_vector_is_clockwise(gb_vector_ref_t vector, gb_vector_ref_t other);

/*! be equal to the other vector?
 *
 * @param vector        the vector
 * @param other         the other vector
 * 
 * @return              tb_true or tb_false
 */
tb_bool_t               gb_vector_near_eq(gb_vector_ref_t vector, gb_vector_ref_t other);

/* //////////////////////////////////////////////////////////////////////////////////////
 * extern
 */
__tb_extern_c_leave__

#endif
