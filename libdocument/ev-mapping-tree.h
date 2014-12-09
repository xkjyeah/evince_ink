/* ev-mapping.h
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

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#ifndef EV_MAPPING_TREE_H
#define EV_MAPPING_TREE_H

#include "ev-document.h"

G_BEGIN_DECLS

typedef struct _EvMappingTree EvMappingTree;

#define        EV_TYPE_MAPPING_TREE        (ev_mapping_tree_get_type())
GType          ev_mapping_tree_get_type    (void) G_GNUC_CONST;

EvMappingTree *ev_mapping_tree_new         (guint          page,
                        EvRectangle extents,
					    GDestroyNotify data_destroy_func);
EvMappingTree *ev_mapping_tree_ref         (EvMappingTree *mapping_tree);
void           ev_mapping_tree_unref       (EvMappingTree *mapping_tree);

guint          ev_mapping_tree_get_page    (EvMappingTree *mapping_tree);
//GTree         *ev_mapping_tree_get_list    (EvMappingTree *mapping_tree);
void           ev_mapping_tree_remove      (EvMappingTree *mapping_tree,
					    gpointer item);
//EvMapping     *ev_mapping_tree_find        (EvMappingTree *mapping_tree,
//					    gconstpointer  data);
//EvMapping     *ev_mapping_tree_find_custom (EvMappingTree *mapping_tree,
//					    gconstpointer  data,
//					    GCompareFunc   func);
gpointer      ev_mapping_tree_get         (EvMappingTree *mapping_tree,
					    gdouble        x,
					    gdouble        y);
//gpointer       ev_mapping_tree_get_data    (EvMappingTree *mapping_tree,
//					    gdouble        x,
//					    gdouble        y);
EvMapping     *ev_mapping_tree_nth         (EvMappingTree *mapping_tree,
                                            guint          n);
guint          ev_mapping_tree_length      (EvMappingTree *mapping_tree);

typedef gboolean (*EvMappingTreeHitFunction)(gpointer item, gdouble x, gdouble y, gpointer data);
int64_t
ev_mapping_tree_add(EvMappingTree *tree,
                    gpointer item,
                    EvRectangle extents,
                    EvMappingTreeHitFunction hit_func,
                    gpointer data);
G_END_DECLS

#endif /* EV_MAPPING_TREE_H */
