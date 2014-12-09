//#include <ev-mapping-tree.h>
//#include "ev-document.h"
#include "evince-document.h"
#include "ev-mapping-tree.h"

#include <glib.h>
#include <stdio.h>


int main(void);

static void
do_nothing(gpointer p)
{
}

static gdouble squared_distance(gdouble x, gdouble y)
{
    return x*x + y*y;
}

static gboolean
is_on_line(gpointer a, gdouble x, gdouble y, gpointer data)
{
    EvRectangle *rect = (EvRectangle*) a;

    gdouble halfwidth = 0.1 * 0.5;
    gdouble halfwidthsq = halfwidth * halfwidth;

    gdouble sq_line_length = squared_distance(rect->x2 - rect->x1, rect->y2 - rect->y1);

    /**
     * if projection lies on line segment
     *      ensure that perpendicular distance is not more than half-width
     *
     * else
     *      ensure that distance to one of the edge points is at
     *      most half-width
     *      */
    {
        gdouble xa,ya, xb,yb;

        xa = x - rect->x1;
        xb = rect->x2 - rect->x1;

        ya = y - rect->y1;
        yb = rect->y2 - rect->y1;

        gdouble projection = xa * xb + ya * yb;

        if ( projection < sq_line_length )
        {
            gdouble perpendic_distance = squared_distance(xa, ya) - projection*projection/sq_line_length;
            if ( perpendic_distance <= halfwidthsq  ) {
                return TRUE;
            }
        }
    }
    {
        gdouble sqdist1, sqdist2;
        sqdist1 = squared_distance(x - rect->x1, y - rect->y1);
        sqdist2 = squared_distance(x - rect->x2, y - rect->y2);

        if (sqdist1 <= halfwidthsq || sqdist2 <= halfwidthsq) {
            return TRUE;
        }
    }
    return FALSE;
}

int main() {

    EvRectangle rect, l1, l2, l3;

    rect.x1 = -1;
    rect.x2 = 1;
    rect.y1 = -2;
    rect.y2 = 2;

    EvMappingTree *tree = ev_mapping_tree_new(1, rect, do_nothing);


    {
        l1.x1 = -0.5;
        l1.x2 = -0.000000001;
        l1.y1 = 1;
        l1.y2 = 1.05;
        int64_t coords = ev_mapping_tree_add(tree, &l1, l1, is_on_line, NULL);
        fprintf(stderr, "%lx\n", coords);
        g_assert(coords == (((int64_t)26 << 58) | (1 << 29) | (3)));
    }
    
    {
        l2.x1 = -0.73;
        l2.x2 = -0.000000001;
        l2.y1 = 1;
        l2.y2 = 1.05;
        int64_t coords = ev_mapping_tree_add(tree, &l2, l2, is_on_line, NULL);
        fprintf(stderr, "%lx\n", coords);
        g_assert(coords == (((int64_t)26 << 58) | (1 << 29) | (3)));
    }

    {
        g_assert(ev_mapping_tree_get(tree, -0.73, 1));
        g_assert(ev_mapping_tree_get(tree, -0.53, 1));
        g_assert(ev_mapping_tree_get(tree, -0.53, 1.05));
        g_assert(!ev_mapping_tree_get(tree, -0.53, 1.2));
    }

    return 0;
}

