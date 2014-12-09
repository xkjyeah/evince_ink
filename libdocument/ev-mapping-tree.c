/* ev-mapping.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2009 Carlos Garcia Campos <carlosgc@gnome.org>
 *
 * Evince is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Evince is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "ev-mapping-tree.h"
#include "ev-document.h"

#include <math.h>
#include <stdlib.h>

/**
 * SECTION: ev-mapping-tree
 * @short_description: An implementation of a loose quadtree, mainly
 * to detect if a mouseclick exists near an item, e.g. an annotation.
 *
 * Use-case: To the tree, add individual segments of an ink annotation.
 * The extents are the points of the individual segments plus 
 * half the line width.
 *
 */
struct _EvMappingTree {
	guint          page;
	//GList         *list;
	GDestroyNotify data_destroy_func;
	volatile gint  ref_count;
    EvRectangle    extents;

    /* private indexing fields... */
    GHashTable      *cell_index;
    GList           *items;
    GArray          *depth_mask;
    GHashTable      *reverse_index;
};
struct ItemHitFunctionPair {
    EvMappingTreeHitFunction    hit_func;
    gpointer                    item;
    gpointer                    data; /* data to pass to hit_func */
    GList                       *link;
};
static EvPoint
ev_mapping_tree_normalize_coordinates(EvMappingTree *mapping_tree,
                                gdouble x, gdouble y);
static gboolean
ev_mapping_tree_in_extents(EvMappingTree *mapping_tree,
                gdouble x, gdouble y);
static void
destroy_pair(gpointer a, gpointer b);


#define MAX_DEPTH           29
#define SMALLEST_QT_UNIT    (1.0 / (1<<28))
#define EXPANSION           0.999

G_DEFINE_BOXED_TYPE (EvMappingTree, ev_mapping_tree, ev_mapping_tree_ref, ev_mapping_tree_unref)

/** 
 *  Returns the key for our depth and cell position
 *
 *  0 <= depth <= 29
 *
 *  0 <= x,y <= 2**29
 *
 * **/
static int64_t
make_cell_coordinates(int depth, int x, int y)
{
    int64_t key;

    key = (((int64_t)depth) & 0xFF) << 58;

    x &= 0x1FFFFFFF;
    y &= 0x1FFFFFFF; // 29 bits valid

    key |= (((int64_t)x) << 29) | y;

    return key;
}

static GList *
generate_valid_cells(EvMappingTree *tree, double x, double y)
{
    GList *list = NULL;
    int cx, cy, i;


    /* every point can be the target of up to 4 cells, but we have
     * 9 conditions to test*/
    for (i=0; i<MAX_DEPTH; i++) {
        
        /* no known elements here */
        if (!g_array_index(tree->depth_mask, gboolean, i)) {
            continue;
        }

        gboolean ov_upper = FALSE,
            ov_lower= FALSE,
            ov_left= FALSE,
            ov_right = FALSE;

        int cell_size = 1 << i;
        int maxX, maxY;

        cx = ((int)x) / cell_size;
        cy = ((int)y) / cell_size;

        maxX = (1 << (MAX_DEPTH - 1)) / cell_size;
        maxY = (1 << (MAX_DEPTH - 1)) / cell_size;

        /* Compute the possibly overlapping cells */
        double rx = x - cx * cell_size;
        double ry = y - cy * cell_size;

        if ( cx > 0 && rx / (cell_size * 0.5) <= EXPANSION ) {
            ov_left = TRUE;
        }
        else if ( cx < maxX - 1 && (cell_size - rx) / (cell_size * 0.5) <= EXPANSION ) {
            ov_right = TRUE;
        }

        if ( cy > 0 && ry / (cell_size * 0.5) <= EXPANSION ) {
            ov_lower = TRUE;
        }
        else if ( cy < maxY && (cell_size - ry) / (cell_size * 0.5) <= EXPANSION ) {
            ov_upper = TRUE;
        }

        /* Add the cell and the overlapping cells to the list */
        int64_t coords;
       
#define TEST_AND_ADD_COORDS(X,Y) \
        coords = make_cell_coordinates(i, X, Y); \
        if (g_hash_table_contains(tree->cell_index, &coords)) { \
            list = g_list_prepend(list, g_hash_table_lookup(tree->cell_index, &coords)); \
        }

        TEST_AND_ADD_COORDS(cx, cy);
        if (ov_left) {
            TEST_AND_ADD_COORDS(cx - 1, cy)

            if (ov_upper) {
                TEST_AND_ADD_COORDS(cx - 1, cy + 1)
            }
            else if (ov_lower) {
                TEST_AND_ADD_COORDS(cx - 1, cy - 1)
            }
        }
        else if (ov_right) {
            TEST_AND_ADD_COORDS(cx + 1, cy)

            if (ov_upper) {
                TEST_AND_ADD_COORDS(cx + 1, cy + 1)
            }
            else if (ov_lower) {
                TEST_AND_ADD_COORDS(cx + 1, cy - 1)
            }
        }

        if (ov_upper) {
            TEST_AND_ADD_COORDS(cx, cy + 1)
        }
        else if (ov_lower) {
            TEST_AND_ADD_COORDS(cx, cy - 1)
        }

    }
#undef TEST_AND_ADD_COORDS

    return list;
}

/**
 * */
static EvPoint
ev_mapping_tree_normalize_coordinates(EvMappingTree *mapping_tree,
                                gdouble x, gdouble y)
{
    EvPoint rv;

    rv.x = (x - mapping_tree->extents.x1) /
                (mapping_tree->extents.x2 - mapping_tree->extents.x1) /
                SMALLEST_QT_UNIT;
    rv.y = (y - mapping_tree->extents.y1) /
                (mapping_tree->extents.y2 - mapping_tree->extents.y1) /
                SMALLEST_QT_UNIT;

    return rv;
}

/**
 * */
static gboolean
ev_mapping_tree_in_extents(EvMappingTree *mapping_tree,
                gdouble x, gdouble y)
{
    return mapping_tree->extents.x1 <= x &&
            x <= mapping_tree->extents.x2 &&
            mapping_tree->extents.y1 <= y &&
            y <= mapping_tree->extents.y2;
}

/**
 * ev_mapping_tree_find:
 * @mapping_tree: an #EvMappingTree
 * @data: mapping data to find
 *
 * Returns: (transfer none): an #EvMapping
 */
//gpointer
//ev_mapping_tree_find (EvMappingTree *mapping_tree,
//		      gconstpointer  data)
//{
//    // TODO: do we need this here?
//}

/**
 * ev_mapping_tree_nth:
 * @mapping_tree: an #EvMappingTree
 * @n: the position to retrieve
 *
 * Returns: (transfer none): the #Evmapping at position @n in @mapping_tree
 */
EvMapping *
ev_mapping_tree_nth (EvMappingTree *mapping_tree,
                     guint          n)
{
        g_return_val_if_fail (mapping_tree != NULL, NULL);

        return (EvMapping *)g_list_nth_data (mapping_tree->items, n);
}

/**
 * ev_mapping_tree_get:
 * @mapping_tree: an #EvMappingTree
 * @x: X coordinate
 * @y: Y coordinate
 *
 * Returns: (transfer none): the #EvMapping in the list at coordinates (x, y)
 *
 * Since: 3.12
 */
gpointer 
ev_mapping_tree_get (EvMappingTree *mapping_tree,
		     gdouble        x,
		     gdouble        y)
{
    g_return_val_if_fail (mapping_tree != NULL, NULL);

    g_return_val_if_fail (ev_mapping_tree_in_extents(mapping_tree,x,y), NULL);


    EvPoint normalized = ev_mapping_tree_normalize_coordinates(mapping_tree,
                            x,y);

    /* for each depth,
     *
     * find the possibly valid cells
     *
     * look for item in the cell
     *
     *
     * */
    GList *list = generate_valid_cells(mapping_tree, normalized.x, normalized.y);

    for ( ; list != NULL; list = list->next ) {
        // each "cell" is also a list
        GList *cell = list->data;

        for ( ; cell != NULL; cell = cell->next ) {
            struct ItemHitFunctionPair *pair = cell->data;

            if (pair->hit_func(pair->item, x, y, pair->data)) {
                return pair->item;
            }
        }
    }

	return NULL;
}

/**
 * ev_mapping_tree_get_data:
 * @mapping_tree: an #EvMappingTree
 * @x: X coordinate
 * @y: Y coordinate
 *
 * Returns: (transfer none): the data of a mapping in the list at coordinates (x, y)
 */
//gpointer
//ev_mapping_tree_get_data (EvMappingTree *mapping_tree,
//			  gdouble        x,
//			  gdouble        y)
//{
//	EvMapping *mapping;
//
//	mapping = ev_mapping_tree_get (mapping_tree, x, y);
//	if (mapping)
//		return mapping->data;
//
//	return NULL;
//}

static gint
compare_item(gconstpointer a, gconstpointer bs)
{
    struct ItemHitFunctionPair *b = (struct ItemHitFunctionPair*) bs;   
    return (a < b->item)?-1:
           (a > b->item)?1:0;
}

/**
 * ev_mapping_tree_remove:
 * @mapping_tree: an #EvMappingTree
 * @mapping: #EvMapping to remove
 *
 * Since: 3.14
 */
void
ev_mapping_tree_remove (EvMappingTree *mapping_tree,
			gpointer item)
{
    // Reverse index and cell
    int64_t coords = *(int64_t*)g_hash_table_lookup(mapping_tree->reverse_index, item);
    GList *cell = g_hash_table_lookup(mapping_tree->cell_index, &coords);
    GList *llink = g_list_find_custom(cell, item, compare_item);
    struct ItemHitFunctionPair *pair = llink->data;

    // remove from reverse index
    g_hash_table_remove(mapping_tree->reverse_index, item);
    // *coords automatically freed
    
    // remove from items
    mapping_tree->items = g_list_remove_link(mapping_tree->items, pair->link);

    // remove from cell
    g_hash_table_replace(mapping_tree->cell_index,
                        &coords,
                        g_list_remove_link(cell, llink));
    g_list_free(llink);

    // destroy the pair item
    destroy_pair(pair, mapping_tree);
    
    // We leave the depth_mask as it is (it is only an approximation optimization)
}

guint
ev_mapping_tree_get_page (EvMappingTree *mapping_tree)
{
	return mapping_tree->page;
}

guint
ev_mapping_tree_length (EvMappingTree *mapping_tree)
{
        g_return_val_if_fail (mapping_tree != NULL, 0);

        return g_list_length (mapping_tree->items);
}

/**
 * ev_mapping_tree_new:
 * @page: page index for this mapping
 * @list: (element-type EvMapping): a #GTree of data for the page
 * @data_destroy_func: function to free a list element
 *
 * Returns: an #EvMappingTree
 */
EvMappingTree *
ev_mapping_tree_new (guint          page,
             EvRectangle extents,
		     GDestroyNotify data_destroy_func)
{
	EvMappingTree *mapping_tree;

	g_return_val_if_fail (data_destroy_func != NULL, NULL);

	mapping_tree = g_slice_new (EvMappingTree);
	mapping_tree->page = page;
	mapping_tree->data_destroy_func = data_destroy_func;
	mapping_tree->ref_count = 1;
    mapping_tree->extents = extents;


    /* Build the tree data structure */
    mapping_tree->cell_index = g_hash_table_new_full(g_int64_hash, g_int64_equal,
                    NULL, NULL);
    mapping_tree->reverse_index = g_hash_table_new_full(g_direct_hash, g_direct_equal,
                    NULL, g_free);
    mapping_tree->items = NULL;
    mapping_tree->depth_mask = g_array_sized_new(0, 0, sizeof(gboolean), MAX_DEPTH);


    gboolean f = FALSE;
    int i;
    for (i=0; i<MAX_DEPTH; i++) {
        g_array_append_val(mapping_tree->depth_mask, f);
    }

	return mapping_tree;
}

/**
 * 
 *  Do the following things:
 *
 *  - Measure the extents, find the correct cell
 *  - Update the depth mask
 *
 * */
int64_t
ev_mapping_tree_add(EvMappingTree *tree,
                    gpointer item,
                    EvRectangle extents,
                    EvMappingTreeHitFunction hit_func,
                    gpointer data)
{
    struct ItemHitFunctionPair *pair;

    // compute the minimum possible size
    EvPoint n1, n2;
    n1 = ev_mapping_tree_normalize_coordinates(tree, extents.x1, extents.y1);
    n2 = ev_mapping_tree_normalize_coordinates(tree, extents.x2, extents.y2);

    // first guess of cell size
    double span = MAX( fabs(n1.x - n2.x), fabs(n1.y - n2.y) );
    int depth = (int)MAX(0, (ceil( log(span) / log(2) )) - 1);
    int cell_size = 1 << depth;
    int i;

    //fprintf(stderr, "%lf %lf %i %i\n", span, log(span), cell_size, depth);

    for (i=0; i<3; i++) {
        /* with expansion factor = 0.999 we should
         * ever only visit up to 2 cells, usually only 1 */
        g_assert(i < 2);

        double margin = EXPANSION * cell_size * 0.5;
        int cx1, cy1, cx2, cy2;
        double rx1, ry1, rx2, ry2;

        cx1 = (int)( (n1.x + margin) / cell_size );
        cy1 = (int)( (n1.y + margin) / cell_size );
        cx2 = (int)( (n2.x + margin) / cell_size );
        cy2 = (int)( (n2.y + margin) / cell_size );

        rx1 = n1.x - cell_size * cx1;
        ry1 = n1.y - cell_size * cy1;
        rx2 = n2.x - cell_size * cx2;
        ry2 = n2.y - cell_size * cy2;

        if (((cx1 == cx2) || // same cell
            ( abs(cx1 - cx2) == 1 && MAX(rx1, rx2) < 2*margin ) ) // diff cell but
            // within margin
            &&
            ((cy1 == cy2) || 
            ( abs(cy1 - cy2) == 1 && MAX(ry1, ry2) < 2*margin ) )) {

            // correct cell!
            int cx = MIN(cx1, cx2);
            int cy = MIN(cy1, cy2);
            int64_t *coords = g_malloc(sizeof(int64_t));
            
            *coords = make_cell_coordinates(depth, cx, cy);

            // create the item
            pair = g_malloc(sizeof(struct ItemHitFunctionPair));
            pair->hit_func = hit_func;
            pair->item = item;
            pair->data = data;

            // append the item
            tree->items = g_list_prepend(tree->items, pair);
            pair->link = tree->items;
            GList *old_list = g_hash_table_lookup(tree->cell_index, coords);
            GList *new_list = g_list_prepend(old_list, pair);
            g_hash_table_replace(tree->cell_index, coords, new_list);
            g_hash_table_replace(tree->reverse_index, pair->item, coords);
            g_array_index(tree->depth_mask, gboolean, depth) = TRUE;
            return *coords;
        }

        cell_size *= 2;
        depth += 1;
    }

    return -1;
}

EvMappingTree *
ev_mapping_tree_ref (EvMappingTree *mapping_tree)
{
	g_return_val_if_fail (mapping_tree != NULL, NULL);
	g_return_val_if_fail (mapping_tree->ref_count > 0, mapping_tree);

	g_atomic_int_add (&mapping_tree->ref_count, 1);

	return mapping_tree;
}

static void
destroy_pair(gpointer a, gpointer b)
{
    struct ItemHitFunctionPair *pair = (struct ItemHitFunctionPair*) a;
    EvMappingTree *tree = (EvMappingTree*)b;
    tree->data_destroy_func(pair->item);
    g_free(pair);
}

static void
destroy_cell(gpointer key, gpointer value, gpointer data)
{
    g_list_free(value);
}

void
ev_mapping_tree_unref (EvMappingTree *mapping_tree)
{
	g_return_if_fail (mapping_tree != NULL);
	g_return_if_fail (mapping_tree->ref_count > 0);

	if (g_atomic_int_add (&mapping_tree->ref_count, -1) - 1 == 0) {
        /* Free the quadtree items */
        g_hash_table_foreach(mapping_tree->cell_index, destroy_cell, NULL);
        g_hash_table_destroy(mapping_tree->cell_index);
        g_hash_table_destroy(mapping_tree->reverse_index);
        g_list_foreach(mapping_tree->items, destroy_pair, mapping_tree);
        g_list_free(mapping_tree->items);
        g_array_free(mapping_tree->depth_mask, TRUE);
        
		g_slice_free (EvMappingTree, mapping_tree);
	}
}
