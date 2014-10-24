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
 * @file        polygon_raster.c
 * @ingroup     core
 */

/* //////////////////////////////////////////////////////////////////////////////////////
 * trace
 */
#define TB_TRACE_MODULE_NAME            "polygon_raster"
#define TB_TRACE_MODULE_DEBUG           (0)

/* //////////////////////////////////////////////////////////////////////////////////////
 * includes
 */
#include "polygon_raster.h"

/* //////////////////////////////////////////////////////////////////////////////////////
 * macros
 */

// the polygon edges grow
#ifdef __gb_small__
#   define GB_POLYGON_RASTER_EDGES_GROW     (1024)
#else
#   define GB_POLYGON_RASTER_EDGES_GROW     (2048)
#endif

/* //////////////////////////////////////////////////////////////////////////////////////
 * types
 */

// the polygon raster edge type
typedef struct __gb_polygon_raster_edge_t
{
    /* the winding for rule
     *
     *   . <= -1
     *     .
     *       . 
     *         .
     *            .  
     *              .
     *            => 1
     *
     * 1:  top => bottom
     * -1: bottom => top
     */
    tb_int8_t       winding     : 2;

    // the index of next edge at the edge pool 
    tb_uint16_t     next;

    // the bottom y-coordinate
    tb_int16_t      y_bottom;

    // the x-coordinate of the active edge
    tb_fixed_t      x;

    // the slope of the edge: dx / dy 
    tb_fixed_t      slope;

}gb_polygon_raster_edge_t, *gb_polygon_raster_edge_ref_t;

/* the polygon raster type
 *
 * 1. make the edge table    
 *     (y)
 *      0 ----------------> . 
 *      1                 .   .
 *      2               .       . e2
 *      3          e1 .           .
 *      4 ------------------------> . 
 *      5         .               .
 *      6       .               .
 *      7 --> .               . e3
 *      8       .           .
 *      9      e4 .       .
 *      10          .   .
 *      11            .
 *
 * edge_table[0]: e1 e2
 * edge_table[4]: e3
 * edge_table[7]: e4
 *
 * 2. scan the edge table  
 *     (y)
 *      0                   . 
 *      1                 . - .
 *      2               . ----- . e2
 *      3          e1 . --------- .
 *      4           .               . 
 *      5         .               .
 *      6       .               .
 *      7     .               . e3
 *      8       .           .
 *      9      e4 .       .
 *      10          .   .
 *      11            .
 *
 * active_edges: e1 e2
 *
 * 3. scan the edge table  
 *     (y)
 *      0                   . 
 *      1                 .   .
 *      2               .       . e2
 *      3          e1 .           .
 *      4           . ------------- . 
 *      5         . ------------- .
 *      6       . ------------- .
 *      7     .               . e3
 *      8       .           .
 *      9      e4 .       .
 *      10          .   .
 *      11            .
 *
 * active_edges: e1 e3
 *
 * 4. scan the edge table  
 *     (y)
 *      0                   . 
 *      1                 .   .
 *      2               .       . e2
 *      3          e1 .           .
 *      4           .               . 
 *      5         .               .
 *      6       .               .
 *      7     . ------------- . e3
 *      8       . --------- .
 *      9      e4 . ----- .
 *      10          . - .
 *      11            .
 *
 * active_edges: e4 e3
 *
 * active_edges: be sorted by x in ascending
 *
 */
typedef struct __gb_polygon_raster_impl_t
{
    // the edge pool, tail: 0, index: > 0
    gb_polygon_raster_edge_ref_t    edge_pool;

    // the edge pool maxn
    tb_size_t                       edge_pool_maxn;
    
    // the edge table
    tb_uint16_t*                    edge_table;

    // the edge table base for the y-coordinate
    tb_long_t                       edge_table_base;

    // the edge table maxn
    tb_size_t                       edge_table_maxn;

    // the active edges
    tb_uint16_t                     active_edges;

    // the top of the polygon bounds
    tb_long_t                       top;

    // the bottom of the polygon bounds
    tb_long_t                       bottom;

}gb_polygon_raster_impl_t;

/* //////////////////////////////////////////////////////////////////////////////////////
 * private implementation
 */
static gb_polygon_raster_edge_ref_t gb_polygon_raster_edge_init(gb_polygon_raster_impl_t* impl, tb_uint16_t index)
{
    // check
    tb_assert_abort(impl && index <= TB_MAXU16);

    // init the edge pool
    if (!impl->edge_pool) impl->edge_pool = tb_nalloc_type(GB_POLYGON_RASTER_EDGES_GROW, gb_polygon_raster_edge_t);
    tb_assert_and_check_return_val(impl->edge_pool, tb_null);

    // grow the edge pool
    if (index >= impl->edge_pool_maxn)
    {
        impl->edge_pool_maxn = index + GB_POLYGON_RASTER_EDGES_GROW;
        impl->edge_pool = tb_ralloc_type(impl->edge_pool, impl->edge_pool_maxn, gb_polygon_raster_edge_t);
        tb_assert_and_check_return_val(impl->edge_pool, tb_null);
    }

    // make a new edge from the edge pool
    return &impl->edge_pool[index];
}
static tb_bool_t gb_polygon_raster_edge_table_init(gb_polygon_raster_impl_t* impl, tb_long_t table_base, tb_size_t table_size)
{
    // check
    tb_assert_abort(impl && table_size);

    // init the edge table
    if (!impl->edge_table)
    {
        impl->edge_table_maxn = table_size;
        impl->edge_table = tb_nalloc_type(impl->edge_table_maxn, tb_uint16_t);
    }
    else if (table_size > impl->edge_table_maxn)
    {
        impl->edge_table_maxn = table_size;
        impl->edge_table = tb_ralloc_type(impl->edge_table, impl->edge_table_maxn, tb_uint16_t);
    }
    tb_assert_and_check_return_val(impl->edge_table && table_size <= TB_MAXU16, tb_false);

    // clear the edge table
    tb_memset(impl->edge_table, 0, table_size * sizeof(tb_uint16_t));

    // init the edge table base
    impl->edge_table_base = table_base;

    // ok
    return tb_true;
}
static tb_bool_t gb_polygon_raster_edge_table_make(gb_polygon_raster_impl_t* impl, gb_polygon_ref_t polygon, gb_rect_ref_t bounds)
{
    // check
    tb_assert_abort(impl && polygon && polygon->points && polygon->counts && bounds);

    // empty polygon?
    tb_check_return_val(gb_nz(bounds->w) && gb_nz(bounds->h), tb_false);

    // init the edge table
    if (!gb_polygon_raster_edge_table_init(impl, gb_round(bounds->y), gb_round(bounds->h) + 1)) return tb_false;
 
    // make the edge table
    gb_point_t          pb;
    gb_point_t          pe;
    tb_bool_t           first       = tb_true;
    tb_long_t           top         = 0;
    tb_long_t           bottom      = 0;
    tb_uint16_t         index       = 0;
    tb_uint16_t         edge_index  = 0;
    tb_long_t           table_index = 0;
    gb_point_ref_t      points      = polygon->points;
    tb_uint16_t*        counts      = polygon->counts;
    tb_uint16_t         count       = *counts++;
    tb_uint16_t*        edge_table  = impl->edge_table;
    while (index < count)
    {
        // the point
        pe = *points++;

        // exists edge?
        if (index)
        {
            // get the integer y-coordinates
            tb_long_t iyb = gb_round(pb.y);
            tb_long_t iye = gb_round(pe.y);

            // not horizaontal edge?
            if (iyb != iye)
            {
                // get the fixed-point coordinates
                tb_fixed6_t xb = gb_float_to_fixed6(pb.x);
                tb_fixed6_t yb = gb_float_to_fixed6(pb.y);
                tb_fixed6_t xe = gb_float_to_fixed6(pe.x);
                tb_fixed6_t ye = gb_float_to_fixed6(pe.y);

                // compute the delta coordinates
                tb_fixed6_t dx = xe - xb;
                tb_fixed6_t dy = ye - yb;

                // update the edge index
                edge_index++;

                // make a new edge from the edge pool
                gb_polygon_raster_edge_ref_t edge = gb_polygon_raster_edge_init(impl, edge_index);

                // init the winding
                edge->winding = 1;

                // sort the points of the edge by the y-coordinate
                if (yb > ye)
                {
                    // reverse the edge points
                    tb_swap(tb_fixed6_t, xb, xe);
                    tb_swap(tb_fixed6_t, yb, ye);
                    tb_swap(tb_long_t, iyb, iye);

                    // reverse the winding
                    edge->winding = -1;
                }

                // compute the accurate bounds of the y-coordinate
                if (first)
                {
                    top     = iyb;
                    bottom  = iye;
                    first   = tb_false;
                }
                else
                {
                    if (iyb < top)    top = iyb;
                    if (iye > bottom) bottom = iye;
                }

                // check
                tb_assert_abort(iyb < iye);

                // compute the slope 
                edge->slope = tb_fixed6_div(dx, dy);

                /* compute the more accurate start x-coordinate
                 *
                 * xb + (iyb - yb + 0.5) * dx / dy
                 * => xb + ((0.5 - yb) % 1) * dx / dy
                 */
                edge->x = tb_fixed6_to_fixed(xb) + ((edge->slope * ((TB_FIXED6_HALF - yb) & 63)) >> 6);

                // init bottom y-coordinate
                edge->y_bottom = iye - 1;

                // the table index
                table_index = iyb - impl->edge_table_base;
                tb_assert_abort(table_index >= 0 && table_index < impl->edge_table_maxn);
                
                /* insert edge to the head of the edge table
                 *
                 * table[index]: => edge => edge => .. => 0
                 *              |
                 *            insert
                 */
                edge->next = edge_table[table_index];
                edge_table[table_index] = edge_index;
            }
        }

        // save the previous point
        pb = pe;
        
        // next point
        index++;

        // next polygon
        if (index == count) 
        {
            // next
            count = *counts++;
            index = 0;
        }
    }

    // update top and bottom of the polygon
    impl->top     = top;
    impl->bottom  = bottom;

    // ok
    return tb_true;
}
static tb_void_t gb_polygon_raster_scan_line_convex(gb_polygon_raster_impl_t* impl, tb_long_t y, gb_polygon_raster_func_t func, tb_cpointer_t priv)
{
    // check
    tb_assert_abort(impl && impl->edge_pool && func);

    // the left-hand edge index
    tb_uint16_t index_lsh = impl->active_edges; 
    tb_check_return(index_lsh);

    // the left-hand edge
    gb_polygon_raster_edge_ref_t edge_lsh = impl->edge_pool + index_lsh; 

    // the right-hand edge index
    tb_uint16_t index_rsh = edge_lsh->next; 
    tb_check_return(index_rsh);

    // the right-hand edge
    gb_polygon_raster_edge_ref_t edge_rsh = impl->edge_pool + index_rsh; 

    // check
    tb_assert_abort(edge_lsh->x <= edge_rsh->x);

    // trace
    tb_trace_d("y: %ld, %{fixed} => %{fixed}", y, edge_lsh->x, edge_rsh->x);

    // init the end y-coordinate for the only one line
    tb_long_t ye = y + 1;

    /* scan rect region? may be faster
     *
     * |    | 
     * |    |
     * |    |
     */
    if (tb_fixed_abs(edge_lsh->slope) <= TB_FIXED_NEAR0 && tb_fixed_abs(edge_rsh->slope) <= TB_FIXED_NEAR0)        
    {
        // get the min and max edge for the y-bottom
        gb_polygon_raster_edge_ref_t    edge_min    = edge_lsh; 
        gb_polygon_raster_edge_ref_t    edge_max    = edge_rsh; 
        tb_uint16_t                     index_max   = index_rsh;
        if (edge_min->y_bottom > edge_max->y_bottom)
        {
            edge_min    = edge_rsh; 
            edge_max    = edge_lsh; 
            index_max   = index_lsh;
        }

        // compute the ye
        ye = edge_min->y_bottom + 1;

        // clear the active edges, only two edges
        impl->active_edges = 0;

        // re-insert the max edge to the edge table using the new top-y coordinate
        if (ye < edge_max->y_bottom)
        {
            // check
            tb_assert_abort(ye >= impl->edge_table_base && ye - impl->edge_table_base < impl->edge_table_maxn);

            /* re-insert to the edge table using the new top-y coordinate
             *
             * table[index]: => edge => edge => .. => 0
             *              |
             *            insert
             */
            edge_max->next = impl->edge_table[ye - impl->edge_table_base];
            impl->edge_table[ye - impl->edge_table_base] = index_max;
        }
    }

    // done it
    func(tb_fixed_round(edge_lsh->x), tb_fixed_round(edge_rsh->x), y, ye, priv);
}
static tb_void_t gb_polygon_raster_scan_line_concave(gb_polygon_raster_impl_t* impl, tb_long_t y, tb_size_t rule, gb_polygon_raster_func_t func, tb_cpointer_t priv)
{
    // check
    tb_assert_abort(impl && impl->edge_pool && func);

    // done
    tb_long_t                       done = 0;
    tb_long_t                       winding = 0; 
    tb_uint16_t                     index_lsh = impl->active_edges; 
    tb_uint16_t                     index_rsh = 0; 
    gb_polygon_raster_edge_ref_t    edge_lsh = tb_null; 
    gb_polygon_raster_edge_ref_t    edge_rsh = tb_null; 
    gb_polygon_raster_edge_ref_t    edge_cache_lsh = tb_null; 
    gb_polygon_raster_edge_ref_t    edge_cache_rsh = tb_null; 
    gb_polygon_raster_edge_ref_t    edge_pool   = impl->edge_pool;
    while (index_lsh) 
    { 
        // the left-hand edge
        edge_lsh = edge_pool + index_lsh; 

        /* compute the winding
         *   
         *    /\
         *    |            |
         *    |-1          | +1
         *    |            |
         *    |            |
         *                \/
         */
        winding += edge_lsh->winding; 

        // the right-hand edge index
        index_rsh = edge_lsh->next; 
        tb_check_break(index_rsh);

        // the right-hand edge
        edge_rsh = edge_pool + index_rsh; 

        // check
        tb_assert_abort(edge_lsh->x <= edge_rsh->x);

        // compute the rule
        switch (rule)
        {
        case GB_POLYGON_RASTER_RULE_ODD:
            {
                /* the odd rule 
                 *
                 *    ------------------                 ------------------ 
                 *  /|\                 |               ||||||||||||||||||||
                 *   |     --------     |               ||||||||||||||||||||
                 *   |   /|\       |    |               ||||||        ||||||
                 * 0 | -1 |   0    | -1 | 0     =>      ||||||        ||||||
                 *   |    |       \|/   |               ||||||        ||||||
                 *   |     --------     |               ||||||||||||||||||||
                 *   |                 \|/              ||||||||||||||||||||
                 *    ------------------                 ------------------ 
                 */
                done = winding & 1;
            }
            break;
        case GB_POLYGON_RASTER_RULE_NONZERO:
            {
                /* the non-zero rule 
                 *
                 *    ------------------                 ------------------
                 *  /|\                 |               ||||||||||||||||||||
                 *   |     --------     |               ||||||||||||||||||||
                 *   |   /|\       |    |               ||||||||||||||||||||
                 * 0 | -1 |   -2   | -1 | 0             ||||||||||||||||||||
                 *   |    |       \|/   |               ||||||||||||||||||||
                 *   |     --------     |               ||||||||||||||||||||
                 *   |                 \|/              ||||||||||||||||||||
                 *    ------------------                 ------------------
                 */
                done = winding;
            }
            break;
        default:
            {
                // clear it
                done = 0;

                // trace
                tb_trace_e("unknown rule: %lu", rule);
            }
            break;
        }

        // trace
        tb_trace_d("y: %ld, winding: %ld, %{fixed} => %{fixed}", y, winding, edge_lsh->x, edge_rsh->x);

#if 0
        // done it for winding?
        if (done) func(tb_fixed_round(edge_lsh->x), tb_fixed_round(edge_rsh->x), y, y + 1, priv);
#else
        // cache the conjoint edges and done them together
        if (done)
        {
            // no edge cache?
            if (!edge_cache_lsh && !edge_cache_rsh) 
            {
                // init edge cache
                edge_cache_lsh = edge_lsh;
                edge_cache_rsh = edge_rsh;
            }
            // is conjoint? merge it
            else if (edge_cache_rsh && tb_fixed_round(edge_cache_rsh->x) == tb_fixed_round(edge_lsh->x))
            {
                // merge the edges to the edge cache
                edge_cache_rsh = edge_rsh;
            }
            else
            {
                // check
                tb_assert_abort(edge_cache_lsh && edge_cache_rsh);

                // done edge cache
                func(tb_fixed_round(edge_cache_lsh->x), tb_fixed_round(edge_cache_rsh->x), y, y + 1, priv);

                // update edge cache
                edge_cache_lsh = edge_lsh;
                edge_cache_rsh = edge_rsh;
            }
        }
#endif

        // the next left-hand edge index
        index_lsh = index_rsh; 
    }

    // done the left edge cache
    if (edge_cache_lsh && edge_cache_rsh) func(tb_fixed_round(edge_cache_lsh->x), tb_fixed_round(edge_cache_rsh->x), y, y + 1, priv);
}
static tb_void_t gb_polygon_raster_scan_next(gb_polygon_raster_impl_t* impl, tb_long_t y, tb_size_t* porder)
{
    // check
    tb_assert_abort(impl && impl->edge_pool && impl->edge_table && y <= impl->bottom);

    // done
    tb_size_t                       first = 1;
    tb_size_t                       order = 1;
    tb_fixed_t                      prev_x = 0;
    tb_uint16_t                     index_prev = 0;
    tb_uint16_t                     index = impl->active_edges;
    gb_polygon_raster_edge_ref_t    edge = tb_null; 
    gb_polygon_raster_edge_ref_t    edge_prev = tb_null; 
    gb_polygon_raster_edge_ref_t    edge_pool = impl->edge_pool;
    tb_uint16_t                     active_edges = impl->active_edges;
    while (index)
    {
        // the edge
        edge = edge_pool + index;

        /* remove edge from the active edges if (y >= edge->y_bottom)
         *            
         *             .
         *           .  .
         *         .     .
         *       .        .  <- y_bottom: end and no next y for this edge, so remove it
         *     .           . <- the start y of the next edge
         *       .        .
         *          .   .   
         *            .      <- bottom
         */
        if (edge->y_bottom < y + 1)
        {
            // the next edge index
            index = edge->next;

            // remove this edge from head
            if (!index_prev) active_edges = index;
            else 
            {
                // the previous edge 
                edge_prev = edge_pool + index_prev;

                // remove this edge from the body
                edge_prev->next = index;
            }

            // continue 
            continue;
        }

        // update the x-coordinate
        edge->x += edge->slope;

        // is order?
        if (porder)
        {
            if (first) first = 0;
            else if (order && edge->x < prev_x) order = 0;
        }

        // update the previous x coordinate
        prev_x = edge->x;

        // update the previous edge index
        index_prev = index;

        // update the edge index
        index = edge->next;
    }

    // save order
    if (porder) *porder = order; 

    // update the active edges 
    impl->active_edges = active_edges;
}
static tb_void_t gb_polygon_raster_active_append(gb_polygon_raster_impl_t* impl, tb_uint16_t index)
{
    // check
    tb_assert_abort(impl && impl->edge_pool);

    // done
    tb_uint16_t                     next = 0;
    gb_polygon_raster_edge_ref_t    edge = tb_null;
    gb_polygon_raster_edge_ref_t    edge_pool = impl->edge_pool;
    tb_uint16_t                     active_edges = impl->active_edges;
    while (index)
    {
        // the edge
        edge = edge_pool + index;

        // save the next edge index
        next = edge->next;

        // insert the edge to the head of the active edges
        edge->next = active_edges;
        active_edges = index;

        // the next edge index
        index = next;
    }

    // update the active edges 
    impl->active_edges = active_edges;
}
static tb_void_t gb_polygon_raster_active_sorted_insert(gb_polygon_raster_impl_t* impl, tb_uint16_t edge_index)
{
    // check
    tb_assert_abort(impl && impl->edge_pool && edge_index);

    // the edge pool
    gb_polygon_raster_edge_ref_t edge_pool = impl->edge_pool;

    // the edge
    gb_polygon_raster_edge_ref_t edge = edge_pool + edge_index;

    // insert edge to the active edges by x in ascending
    edge->next = 0;
    if (!impl->active_edges) impl->active_edges = edge_index;
    else 
    {
        // find an inserted position
        gb_polygon_raster_edge_ref_t    edge_prev       = tb_null;
        gb_polygon_raster_edge_ref_t    edge_active     = tb_null;
        tb_uint16_t                     index_active    = impl->active_edges;
        while (index_active)
        {
            // the active edge
            edge_active = edge_pool + index_active;

            // check
            tb_assert_abort(edge_index != index_active);

            /* is this?
             *
             * x: 1 2 3     5 6
             *               |
             *             4 or 5
             */
            if (edge->x <= edge_active->x) 
            {
                /* same vertex?
                 *
                 *
                 * x: 1 2 3     5 6
                 *               |   .
                 *               5    .
                 *             .       .
                 *           .          .
                 *         .          active_edge
                 *       .
                 *     edge
                 *
                 * x: 1 2 3   5         6
                 *                 .    |
                 *                  .   5
                 *                   .    .
                 *                    .     .
                 *          active_edge       .
                 *                              . 
                 *                                .  
                 *                                  .
                 *                                   edge
                 *
                 *  x: 1 2 3   5         6
                 *                 .    |
                 *                .     5
                 *              .    .
                 *            .     .
                 *  active_edge    .
                 *                . 
                 *               .  
                 *              .
                 *             edge
                 *
                 *
                 * x: 1 2 3     5 6
                 *               |   .
                 *               5      .
                 *                 .       .
                 *                   .       active_edge 
                 *                     .           
                 *                       .
                 *                         .
                 *                           .
                 *                             .
                 *                               .
                 *                                 .
                 *                                 edge
                 */
                if (edge->x == edge_active->x)
                {
                    /* the edge is at the left-hand of the active edge?
                     * 
                     * x: 1 2 3     5 6    <- active_edges
                     *               |   .
                     *               5    .
                     *             .       .
                     *           .          .
                     *         .        active_edge
                     *       .
                     *     edge
                     *
                     * if (edge->dx / edge->dy < active->dx / active->dy)?
                     */
                    if (edge->slope < edge_active->slope) break;
                }
                else break;
            }
            
            // the previous active edge
            edge_prev = edge_active;

            // the next active edge index
            index_active = edge_prev->next;
        }

        // insert edge to the active edges: edge_prev -> edge -> edge_active
        if (!edge_prev)
        {
            // insert to the head
            edge->next          = impl->active_edges;
            impl->active_edges  = edge_index;
        }
        else
        {
            // insert to the body
            edge->next      = index_active;
            edge_prev->next = edge_index;
        }
    }
}
static tb_void_t gb_polygon_raster_active_sorted_append(gb_polygon_raster_impl_t* impl, tb_uint16_t edge_index)
{
    // check
    tb_assert_abort(impl && impl->edge_pool);

    // done
    tb_uint16_t                     index_next = 0;
    gb_polygon_raster_edge_ref_t    edge = tb_null;
    gb_polygon_raster_edge_ref_t    edge_pool = impl->edge_pool;
    while (edge_index)
    {
        // the edge
        edge = edge_pool + edge_index;

        // save the next edge index
        index_next = edge->next;

        // insert the edge to the active edges
        gb_polygon_raster_active_sorted_insert(impl, edge_index);

        // the next edge index
        edge_index = index_next;
    }
}
static tb_void_t gb_polygon_raster_active_sort(gb_polygon_raster_impl_t* impl)
{
    // check
    tb_assert_abort(impl && impl->edge_pool);

    // done
    tb_uint16_t                     index_lsh   = impl->active_edges;
    tb_uint16_t                     index_rsh   = 0;
    gb_polygon_raster_edge_ref_t    edge_lsh    = tb_null;
    gb_polygon_raster_edge_ref_t    edge_rsh    = tb_null;
    gb_polygon_raster_edge_t        edge_tmp;
    gb_polygon_raster_edge_ref_t    edge_pool   = impl->edge_pool;
    while (index_lsh)
    {
        // the left-hand edge
        edge_lsh = edge_pool + index_lsh;

        // the right-hand edge index
        index_rsh = edge_lsh->next;
        while (index_rsh)
        {
            // the right-hand edge
            edge_rsh = edge_pool + index_rsh;

            // need sort? swap them
            if (edge_lsh->x > edge_rsh->x)
            {
                // save the left-hand edge
                edge_tmp = *edge_lsh;

                // swap the left-hand edge
                *edge_lsh = *edge_rsh;

                // restore the next index
                edge_lsh->next = edge_tmp.next;
                edge_tmp.next = edge_rsh->next;

                // swap the right-hand edge
                *edge_rsh = edge_tmp;
            }
        
            // the next right-hand edge index
            index_rsh = edge_rsh->next;
        }

        // the next left-hand edge index
        index_lsh = edge_lsh->next;
    }
}
static tb_void_t gb_polygon_raster_done_convex(gb_polygon_raster_impl_t* impl, gb_polygon_ref_t polygon, gb_rect_ref_t bounds, gb_polygon_raster_func_t func, tb_cpointer_t priv)
{
    // check
    tb_assert_abort(impl && func);

    // init the active edges
    impl->active_edges = 0;

    // make the edge table
    if (!gb_polygon_raster_edge_table_make(impl, polygon, bounds)) return ;

    // done scan
    tb_long_t       y;
    tb_long_t       top         = impl->top; 
    tb_long_t       bottom      = impl->bottom; 
    tb_long_t       base        = impl->edge_table_base; 
    tb_uint16_t*    edge_table  = impl->edge_table;
    for (y = top; y < bottom; y++)
    {
        // append edges to the sorted active edges by x in ascending
        gb_polygon_raster_active_sorted_append(impl, edge_table[y - base]); 

        // scan line from the active edges
        gb_polygon_raster_scan_line_convex(impl, y, func, priv); 

        // end?
        tb_check_break(y < bottom - 1);

        // scan the next line from the active edges
        gb_polygon_raster_scan_next(impl, y, tb_null); 
    }
}
static tb_void_t gb_polygon_raster_done_concave(gb_polygon_raster_impl_t* impl, gb_polygon_ref_t polygon, gb_rect_ref_t bounds, tb_size_t rule, gb_polygon_raster_func_t func, tb_cpointer_t priv)
{
    // check
    tb_assert_abort(impl && func);

    // init the active edges
    impl->active_edges = 0;

    // make the edge table
    if (!gb_polygon_raster_edge_table_make(impl, polygon, bounds)) return ;

    // done scan
    tb_long_t       y;
    tb_size_t       order       = 1; 
    tb_long_t       top         = impl->top; 
    tb_long_t       bottom      = impl->bottom; 
    tb_long_t       base        = impl->edge_table_base; 
    tb_uint16_t*    edge_table  = impl->edge_table;
    for (y = top; y < bottom; y++)
    {
        // order? append edges to the sorted active edges by x in ascending
        if (order) gb_polygon_raster_active_sorted_append(impl, edge_table[y - base]); 
        else
        {
            // append edges to the active edges from the edge table
            gb_polygon_raster_active_append(impl, edge_table[y - base]); 

            // sort by x in ascending at the active edges
            gb_polygon_raster_active_sort(impl); 
        }

        // scan line from the active edges
        gb_polygon_raster_scan_line_concave(impl, y, rule, func, priv); 

        // end?
        tb_check_break(y < bottom - 1);

        // scan the next line from the active edges
        gb_polygon_raster_scan_next(impl, y, &order); 
    }
}

/* //////////////////////////////////////////////////////////////////////////////////////
 * implementation
 */
gb_polygon_raster_ref_t gb_polygon_raster_init()
{
    // init it
    return (gb_polygon_raster_ref_t)tb_malloc0_type(gb_polygon_raster_impl_t);
}
tb_void_t gb_polygon_raster_exit(gb_polygon_raster_ref_t raster)
{
    // check
    gb_polygon_raster_impl_t* impl = (gb_polygon_raster_impl_t*)raster;
    tb_assert_and_check_return(impl);

    // exit the edge pool
    if (impl->edge_pool) tb_free(impl->edge_pool);
    impl->edge_pool = tb_null;

    // exit the edge table
    if (impl->edge_table) tb_free(impl->edge_table);
    impl->edge_table = tb_null;

    // exit it
    tb_free(impl);
}
tb_void_t gb_polygon_raster_done(gb_polygon_raster_ref_t raster, gb_polygon_ref_t polygon, gb_rect_ref_t bounds, tb_size_t rule, gb_polygon_raster_func_t func, tb_cpointer_t priv)
{
    // check
    gb_polygon_raster_impl_t* impl = (gb_polygon_raster_impl_t*)raster;
    tb_assert_abort(impl && polygon && func);

    // is convex polygon for each contour?
    if (polygon->convex)
    {
        // done
        tb_size_t       index               = 0;
        gb_point_ref_t  points              = polygon->points;
        tb_uint16_t*    counts              = polygon->counts;
        tb_uint16_t     contour_counts[2]   = {0, 0};
        gb_polygon_t    contour             = {tb_null, contour_counts, tb_true};
        while ((contour_counts[0] = *counts++))
        {
            // init the polygon for this contour
            contour.points = points + index;

            // done raster for the convex contour, will be faster
            gb_polygon_raster_done_convex(impl, &contour, bounds, func, priv);

            // update the contour index
            index += contour_counts[0];
        }
    }
    else
    {
        // done raster for the concave polygon
        gb_polygon_raster_done_concave(impl, polygon, bounds, rule, func, priv);
    }
}

