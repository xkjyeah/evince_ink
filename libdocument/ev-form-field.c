/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2007 Carlos Garcia Campos <carlosgc@gnome.org>
 *  Copyright (C) 2006 Julien Rebetez
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include "ev-form-field.h"

static void ev_form_field_init                 (EvFormField               *field);
static void ev_form_field_class_init           (EvFormFieldClass          *klass);
static void ev_form_field_text_init            (EvFormFieldText           *field_text);
static void ev_form_field_text_class_init      (EvFormFieldTextClass      *klass);
static void ev_form_field_button_init          (EvFormFieldButton         *field_button);
static void ev_form_field_button_class_init    (EvFormFieldButtonClass    *klass);
static void ev_form_field_choice_init          (EvFormFieldChoice         *field_choice);
static void ev_form_field_choice_class_init    (EvFormFieldChoiceClass    *klass);
static void ev_form_field_signature_init       (EvFormFieldSignature      *field_choice);
static void ev_form_field_signature_class_init (EvFormFieldSignatureClass *klass);

G_DEFINE_ABSTRACT_TYPE (EvFormField, ev_form_field, G_TYPE_OBJECT)
G_DEFINE_TYPE (EvFormFieldText, ev_form_field_text, EV_TYPE_FORM_FIELD)
G_DEFINE_TYPE (EvFormFieldButton, ev_form_field_button, EV_TYPE_FORM_FIELD)
G_DEFINE_TYPE (EvFormFieldChoice, ev_form_field_choice, EV_TYPE_FORM_FIELD)
G_DEFINE_TYPE (EvFormFieldSignature, ev_form_field_signature, EV_TYPE_FORM_FIELD)

static void
ev_form_field_init (EvFormField *field)
{
	field->page = -1;
	field->changed = FALSE;
	field->is_read_only = FALSE;
}

static void
ev_form_field_class_init (EvFormFieldClass *klass)
{
}

static void
ev_form_field_text_finalize (GObject *object)
{
	EvFormFieldText *field_text = EV_FORM_FIELD_TEXT (object);

	if (field_text->text) {
		g_free (field_text->text);
		field_text->text = NULL;
	}

	(* G_OBJECT_CLASS (ev_form_field_text_parent_class)->finalize) (object);
}

static void
ev_form_field_text_init (EvFormFieldText *field_text)
{
}

static void
ev_form_field_text_class_init (EvFormFieldTextClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = ev_form_field_text_finalize;
}

static void
ev_form_field_button_init (EvFormFieldButton *field_button)
{
}

static void
ev_form_field_button_class_init (EvFormFieldButtonClass *klass)
{
}

static void
ev_form_field_choice_finalize (GObject *object)
{
	EvFormFieldChoice *field_choice = EV_FORM_FIELD_CHOICE (object);

	if (field_choice->selected_items) {
		g_list_free (field_choice->selected_items);
		field_choice->selected_items = NULL;
	}

	if (field_choice->text) {
		g_free (field_choice->text);
		field_choice->text = NULL;
	}

	(* G_OBJECT_CLASS (ev_form_field_choice_parent_class)->finalize) (object);
}

static void
ev_form_field_choice_init (EvFormFieldChoice *field_choice)
{
}

static void
ev_form_field_choice_class_init (EvFormFieldChoiceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = ev_form_field_choice_finalize;
}

static void
ev_form_field_signature_init (EvFormFieldSignature *field_signature)
{
}

static void
ev_form_field_signature_class_init (EvFormFieldSignatureClass *klass)
{
}

EvFormField *
ev_form_field_text_new (gint                id,
			EvFormFieldTextType type)
{
	EvFormField *field;
	
	g_return_val_if_fail (id >= 0, NULL);
	g_return_val_if_fail (type >= EV_FORM_FIELD_TEXT_NORMAL &&
			      type <= EV_FORM_FIELD_TEXT_FILE_SELECT, NULL);

	field = EV_FORM_FIELD (g_object_new (EV_TYPE_FORM_FIELD_TEXT, NULL));
	field->id = id;
	EV_FORM_FIELD_TEXT (field)->type = type;

	return field;
}

EvFormField *
ev_form_field_button_new (gint                  id,
			  EvFormFieldButtonType type)
{
	EvFormField *field;

	g_return_val_if_fail (id >= 0, NULL);
	g_return_val_if_fail (type >= EV_FORM_FIELD_BUTTON_PUSH &&
			      type <= EV_FORM_FIELD_BUTTON_RADIO, NULL);

	field = EV_FORM_FIELD (g_object_new (EV_TYPE_FORM_FIELD_BUTTON, NULL));
	field->id = id;
	EV_FORM_FIELD_BUTTON (field)->type = type;

	return field;
}

EvFormField *
ev_form_field_choice_new (gint                  id,
			  EvFormFieldChoiceType type)
{
	EvFormField *field;

	g_return_val_if_fail (id >= 0, NULL);
	g_return_val_if_fail (type >= EV_FORM_FIELD_CHOICE_COMBO &&
			      type <= EV_FORM_FIELD_CHOICE_LIST, NULL);
	
	field = EV_FORM_FIELD (g_object_new (EV_TYPE_FORM_FIELD_CHOICE, NULL));
	field->id = id;
	EV_FORM_FIELD_CHOICE (field)->type = type;

	return field;
}

EvFormField *
ev_form_field_signature_new (gint id)
{
	EvFormField *field;

	g_return_val_if_fail (id >= 0, NULL);

	field = EV_FORM_FIELD (g_object_new (EV_TYPE_FORM_FIELD_SIGNATURE, NULL));
	field->id = id;

	return field;
}

/* EvFormFieldMapping */
static void
ev_form_field_mapping_free_foreach (EvFormFieldMapping *mapping)
{
	g_object_unref (mapping->field);
	g_free (mapping);
}

void
ev_form_field_mapping_free (GList *field_mapping)
{
	if (!field_mapping)
		return;

	g_list_foreach (field_mapping, (GFunc)ev_form_field_mapping_free_foreach, NULL);
	g_list_free (field_mapping);
}

EvFormField *
ev_form_field_mapping_find (GList   *field_mapping,
			    gdouble  x,
			    gdouble  y)
{
	GList *list;

	for (list = field_mapping; list; list = list->next) {
		EvFormFieldMapping *mapping = list->data;

		if ((x >= mapping->x1) &&
		    (y >= mapping->y1) &&
		    (x <= mapping->x2) &&
		    (y <= mapping->y2)) {
			return mapping->field;
		}
	}

	return NULL;
}

void
ev_form_field_mapping_get_area (GList       *field_mapping,
				EvFormField *field,
				EvRectangle *area)
{
	GList *list;

	for (list = field_mapping; list; list = list->next) {
		EvFormFieldMapping *mapping = list->data;

		if (mapping->field->id == field->id) {
			area->x1 = mapping->x1;
			area->y1 = mapping->y1;
			area->x2 = mapping->x2;
			area->y2 = mapping->y2;

			break;
		}
	}
}

EvFormField *
ev_form_field_mapping_find_by_id (GList *field_mapping,
				  gint   id)
{
	GList *list;
	
	for (list = field_mapping; list; list = list->next) {
		EvFormFieldMapping *mapping = list->data;

		if (id == mapping->field->id)
			return mapping->field;
	}
	
	return NULL;
}
