/* ev-annotation.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2009 Carlos Garcia Campos <carlosgc@gnome.org>
 * Copyright (C) 2007 Iñigo Martinez <inigomartinez@gmail.com>
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

#include "config.h"

#include <cairo.h>

#include "ev-annotation.h"
#include "ev-document-misc.h"
#include "ev-document-type-builtins.h"
#include "ev-mapping-tree.h"


struct _EvAnnotation {
	GObject          parent;

	EvAnnotationType type;
	EvPage          *page;

	gchar           *contents;
	gchar           *name;
	gchar           *modified;
	GdkRGBA          rgba;

    /* appearances */
    // cairo_surface_t *appearance;
    char            *appearance;
    EvRectangle      appearance_bounds;
};

struct _EvAnnotationClass {
	GObjectClass parent_class;
};

struct _EvAnnotationMarkupInterface {
	GTypeInterface base_iface;
};

struct _EvAnnotationText {
	EvAnnotation parent;

	gboolean             is_open : 1;
	EvAnnotationTextIcon icon;
};

struct _EvAnnotationTextClass {
	EvAnnotationClass parent_class;
};

struct _EvAnnotationAttachment {
	EvAnnotation parent;

	EvAttachment *attachment;
};

struct _EvAnnotationAttachmentClass {
	EvAnnotationClass parent_class;
};

struct _EvAnnotationTextMarkup {
	EvAnnotation parent;

        EvAnnotationTextMarkupType type;
};

struct _EvAnnotationTextMarkupClass {
	EvAnnotationClass parent_class;
};

/* This struct is proxy to the cairo_operator_t */
struct _EvAnnotationInk {
	EvAnnotation parent;

	/* data structures to store the line */
	EvAnnotationInkOperator operator;
	GArray *widths; /* array of doubles */
	double width;	/* int for single width -- incompatible with widths. Either must be set to 0 */
	GArray *paths;	/* array of paths */

    /* for determining hit */
    EvMappingTree *quadtree;
};
struct _EvAnnotationInkClass {
	EvAnnotationClass parent_class;
};

static void ev_annotation_markup_default_init           (EvAnnotationMarkupInterface *iface);
static void ev_annotation_text_markup_iface_init        (EvAnnotationMarkupInterface *iface);
static void ev_annotation_ink_markup_iface_init        (EvAnnotationMarkupInterface *iface);
static void ev_annotation_attachment_markup_iface_init  (EvAnnotationMarkupInterface *iface);
static void ev_annotation_text_markup_markup_iface_init (EvAnnotationMarkupInterface *iface);

/* EvAnnotation */
enum {
	PROP_ANNOT_0,
	PROP_ANNOT_PAGE,
	PROP_ANNOT_CONTENTS,
	PROP_ANNOT_NAME,
	PROP_ANNOT_MODIFIED,
	PROP_ANNOT_COLOR,
        PROP_ANNOT_RGBA
};

/* EvAnnotationMarkup */
enum {
	PROP_MARKUP_0,
	PROP_MARKUP_LABEL,
	PROP_MARKUP_OPACITY,
	PROP_MARKUP_HAS_POPUP,
	PROP_MARKUP_RECTANGLE,
	PROP_MARKUP_POPUP_IS_OPEN
};

/* EvAnnotationText */
enum {
	PROP_TEXT_ICON = PROP_MARKUP_POPUP_IS_OPEN + 1,
	PROP_TEXT_IS_OPEN
};

/* EvAnnotationAttachment */
enum {
	PROP_ATTACHMENT_ATTACHMENT = PROP_MARKUP_POPUP_IS_OPEN + 1
};

/* EvAnnotationTextMarkup */
enum {
        PROP_TEXT_MARKUP_TYPE = PROP_MARKUP_POPUP_IS_OPEN + 1
};

/* EvAnnotationInk */
enum {
        PROP_INK_PATHS = PROP_MARKUP_POPUP_IS_OPEN + 1,
	PROP_INK_WIDTH,
	PROP_INK_WIDTHS,
	PROP_INK_OPERATOR
};

G_DEFINE_ABSTRACT_TYPE (EvAnnotation, ev_annotation, G_TYPE_OBJECT)
G_DEFINE_INTERFACE (EvAnnotationMarkup, ev_annotation_markup, EV_TYPE_ANNOTATION)
G_DEFINE_TYPE_WITH_CODE (EvAnnotationText, ev_annotation_text, EV_TYPE_ANNOTATION,
	 {
		 G_IMPLEMENT_INTERFACE (EV_TYPE_ANNOTATION_MARKUP,
					ev_annotation_text_markup_iface_init);
	 });
G_DEFINE_TYPE_WITH_CODE (EvAnnotationAttachment, ev_annotation_attachment, EV_TYPE_ANNOTATION,
	 {
		 G_IMPLEMENT_INTERFACE (EV_TYPE_ANNOTATION_MARKUP,
					ev_annotation_attachment_markup_iface_init);
	 });
G_DEFINE_TYPE_WITH_CODE (EvAnnotationTextMarkup, ev_annotation_text_markup, EV_TYPE_ANNOTATION,
	 {
		 G_IMPLEMENT_INTERFACE (EV_TYPE_ANNOTATION_MARKUP,
					ev_annotation_text_markup_markup_iface_init);
	 });
G_DEFINE_TYPE_WITH_CODE (EvAnnotationInk, ev_annotation_ink, EV_TYPE_ANNOTATION,
    {
        G_IMPLEMENT_INTERFACE (EV_TYPE_ANNOTATION_MARKUP,
                    ev_annotation_ink_markup_iface_init);
    });

/* EvAnnotation */
static void
ev_annotation_finalize (GObject *object)
{
        EvAnnotation *annot = EV_ANNOTATION (object);

	if (annot->page) {
		g_object_unref (annot->page);
		annot->page = NULL;
	}

        if (annot->contents) {
                g_free (annot->contents);
                annot->contents = NULL;
        }

        if (annot->name) {
                g_free (annot->name);
                annot->name = NULL;
        }

        if (annot->modified) {
                g_free (annot->modified);
                annot->modified = NULL;
        }

        if (annot->appearance) {
//            cairo_surface_destroy(annot->appearance);
            g_free(annot->appearance);           
            annot->appearance = NULL;
        }

        G_OBJECT_CLASS (ev_annotation_parent_class)->finalize (object);
}

static void
ev_annotation_init (EvAnnotation *annot)
{
	annot->type = EV_ANNOTATION_TYPE_UNKNOWN;
}

static void
ev_annotation_set_property (GObject      *object,
			    guint         prop_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
	EvAnnotation *annot = EV_ANNOTATION (object);

	switch (prop_id) {
	case PROP_ANNOT_PAGE:
		annot->page = g_value_dup_object (value);
		break;
	case PROP_ANNOT_CONTENTS:
		ev_annotation_set_contents (annot, g_value_get_string (value));
		break;
	case PROP_ANNOT_NAME:
		ev_annotation_set_name (annot, g_value_get_string (value));
		break;
	case PROP_ANNOT_MODIFIED:
		ev_annotation_set_modified (annot, g_value_get_string (value));
		break;
	case PROP_ANNOT_COLOR:
		ev_annotation_set_color (annot, g_value_get_pointer (value));
		break;
        case PROP_ANNOT_RGBA:
                ev_annotation_set_rgba (annot, g_value_get_boxed (value));
                break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
ev_annotation_get_property (GObject    *object,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
	EvAnnotation *annot = EV_ANNOTATION (object);

	switch (prop_id) {
	case PROP_ANNOT_CONTENTS:
		g_value_set_string (value, ev_annotation_get_contents (annot));
		break;
	case PROP_ANNOT_NAME:
		g_value_set_string (value, ev_annotation_get_name (annot));
		break;
	case PROP_ANNOT_MODIFIED:
		g_value_set_string (value, ev_annotation_get_modified (annot));
		break;
	case PROP_ANNOT_COLOR: {
                GdkColor color;

                ev_annotation_get_color (annot, &color);
		g_value_set_pointer (value, &color);
		break;
        }
        case PROP_ANNOT_RGBA:
                g_value_set_boxed (value, &annot->rgba);
                break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
ev_annotation_class_init (EvAnnotationClass *klass)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

	g_object_class->finalize = ev_annotation_finalize;
	g_object_class->set_property = ev_annotation_set_property;
	g_object_class->get_property = ev_annotation_get_property;

	g_object_class_install_property (g_object_class,
					 PROP_ANNOT_PAGE,
					 g_param_spec_object ("page",
							      "Page",
							      "The page wehere the annotation is",
							      EV_TYPE_PAGE,
							      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_ANNOT_CONTENTS,
					 g_param_spec_string ("contents",
							      "Contents",
							      "The annotation contents",
							      NULL,
							      G_PARAM_READWRITE |
                                                              G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_ANNOT_NAME,
					 g_param_spec_string ("name",
							      "Name",
							      "The annotation unique name",
							      NULL,
							      G_PARAM_READWRITE |
                                                              G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_ANNOT_MODIFIED,
					 g_param_spec_string ("modified",
							      "Modified",
							      "Last modified date as string",
							      NULL,
							      G_PARAM_READWRITE |
                                                              G_PARAM_STATIC_STRINGS));
        /**
         * EvAnnotation:color:
         *
         * The colour of the annotation as a #GdkColor.
         *
         * Deprecated: 3.6: Use #EvAnnotation:rgba instead.
         */
	g_object_class_install_property (g_object_class,
					 PROP_ANNOT_COLOR,
					 g_param_spec_pointer ("color",
							       "Color",
							       "The annotation color",
							       G_PARAM_READWRITE |
                                                               G_PARAM_STATIC_STRINGS));

        /**
         * EvAnnotation:rgba:
         *
         * The colour of the annotation as a #GdkRGBA.
         *
         * Since: 3.6
         */
        g_object_class_install_property (g_object_class,
                                         PROP_ANNOT_COLOR,
                                         g_param_spec_boxed ("rgba", NULL, NULL,
                                                             GDK_TYPE_RGBA,
                                                             G_PARAM_READWRITE |
                                                             G_PARAM_STATIC_STRINGS));
}

EvAnnotationType
ev_annotation_get_annotation_type (EvAnnotation *annot)
{
	g_return_val_if_fail (EV_IS_ANNOTATION (annot), 0);

	return annot->type;
}

/**
 * ev_annotation_get_page:
 * @annot: an #EvAnnotation
 *
 * Get the page where @annot appears.
 *
 * Returns: (transfer none): the #EvPage where @annot appears
 */
EvPage *
ev_annotation_get_page (EvAnnotation *annot)
{
	g_return_val_if_fail (EV_IS_ANNOTATION (annot), NULL);

	return annot->page;
}

/**
 * ev_annotation_get_page_index:
 * @annot: an #EvAnnotation
 *
 * Get the index of the page where @annot appears. Note that the index
 * is 0 based.
 *
 * Returns: the page index.
 */
guint
ev_annotation_get_page_index (EvAnnotation *annot)
{
	g_return_val_if_fail (EV_IS_ANNOTATION (annot), 0);

	return annot->page->index;
}

/**
 * ev_annotation_equal:
 * @annot: an #EvAnnotation
 * @other: another #EvAnnotation
 *
 * Compare @annot and @other.
 *
 * Returns: %TRUE if @annot is equal to @other, %FALSE otherwise
 */
gboolean
ev_annotation_equal (EvAnnotation *annot,
		     EvAnnotation *other)
{
	g_return_val_if_fail (EV_IS_ANNOTATION (annot), FALSE);
	g_return_val_if_fail (EV_IS_ANNOTATION (other), FALSE);

	return (annot == other || g_strcmp0 (annot->name, other->name) == 0);
}

/**
 * ev_annotation_get_contents:
 * @annot: an #EvAnnotation
 *
 * Get the contents of @annot. The contents of
 * @annot is the text that is displayed in the annotation, or an
 * alternate description of the annotation's content for non-text annotations
 *
 * Returns: a string with the contents of the annotation or
 * %NULL if @annot has no contents.
 */
const gchar *
ev_annotation_get_contents (EvAnnotation *annot)
{
	g_return_val_if_fail (EV_IS_ANNOTATION (annot), NULL);

	return annot->contents;
}

/**
 * ev_annotation_set_contents:
 * @annot: an #EvAnnotation
 *
 * Set the contents of @annot. You can monitor
 * changes in the annotation's  contents by connecting to
 * notify::contents signal of @annot.
 *
 * Returns: %TRUE if the contents have been changed, %FALSE otherwise.
 */
gboolean
ev_annotation_set_contents (EvAnnotation *annot,
			    const gchar  *contents)
{
	g_return_val_if_fail (EV_IS_ANNOTATION (annot), FALSE);

	if (g_strcmp0 (annot->contents, contents) == 0)
		return FALSE;

	if (annot->contents)
		g_free (annot->contents);
	annot->contents = contents ? g_strdup (contents) : NULL;

	g_object_notify (G_OBJECT (annot), "contents");

	return TRUE;
}

/**
 * ev_annotation_get_name:
 * @annot: an #EvAnnotation
 *
 * Get the name of @annot. The name of the annotation is a string
 * that uniquely indenftifies @annot amongs all the annotations
 * in the same page.
 *
 * Returns: the string with the annotation's name.
 */
const gchar *
ev_annotation_get_name (EvAnnotation *annot)
{
	g_return_val_if_fail (EV_IS_ANNOTATION (annot), NULL);

	return annot->name;
}

/**
 * ev_annotation_set_name:
 * @annot: an #EvAnnotation
 *
 * Set the name of @annot.
 * You can monitor changes of the annotation name by connecting
 * to the notify::name signal on @annot.
 *
 * Returns: %TRUE when the name has been changed, %FALSE otherwise.
 */
gboolean
ev_annotation_set_name (EvAnnotation *annot,
			const gchar  *name)
{
	g_return_val_if_fail (EV_IS_ANNOTATION (annot), FALSE);

	if (g_strcmp0 (annot->name, name) == 0)
		return FALSE;

	if (annot->name)
		g_free (annot->name);
	annot->name = name ? g_strdup (name) : NULL;

	g_object_notify (G_OBJECT (annot), "name");

	return TRUE;
}

/**
 * ev_annotation_get_modified:
 * @annot: an #EvAnnotation
 *
 * Get the last modification date of @annot.
 *
 * Returns: A string containing the last modification date.
 */
const gchar *
ev_annotation_get_modified (EvAnnotation *annot)
{
	g_return_val_if_fail (EV_IS_ANNOTATION (annot), NULL);

	return annot->modified;
}

/**
 * ev_annotation_set_modified:
 * @annot: an #EvAnnotation
 * @modified: string with the last modification date.
 *
 * Set the last modification date of @annot to @modified. To
 * set the last modification date using a #GTime, use
 * ev_annotation_set_modified_from_time() instead. You can monitor
 * changes to the last modification date by connecting to the
 * notify::modified signal on @annot.
 *
 * Returns: %TRUE if the last modification date has been updated, %FALSE otherwise.
 */
gboolean
ev_annotation_set_modified (EvAnnotation *annot,
			    const gchar  *modified)
{
	g_return_val_if_fail (EV_IS_ANNOTATION (annot), FALSE);

	if (g_strcmp0 (annot->modified, modified) == 0)
		return FALSE;

	if (annot->modified)
		g_free (annot->modified);
	annot->modified = modified ? g_strdup (modified) : NULL;

	g_object_notify (G_OBJECT (annot), "modified");

	return TRUE;
}

/**
 * ev_annotation_set_modified_from_time:
 * @annot: an #EvAnnotation
 * @utime: a #GTime
 *
 * Set the last modification date of @annot to @utime.  You can
 * monitor changes to the last modification date by connectin to the
 * notify::modified sinal on @annot.
 * For the time-format used, see ev_document_misc_format_date().
 *
 * Returns: %TRUE if the last modified date has been updated, %FALSE otherwise.
 */
gboolean
ev_annotation_set_modified_from_time (EvAnnotation *annot,
				      GTime         utime)
{
	gchar *modified;

	g_return_val_if_fail (EV_IS_ANNOTATION (annot), FALSE);

	modified = ev_document_misc_format_date (utime);

	if (g_strcmp0 (annot->modified, modified) == 0) {
		g_free (modified);
		return FALSE;
	}

	if (annot->modified)
		g_free (annot->modified);
	annot->modified = modified;

	g_object_notify (G_OBJECT (annot), "modified");

	return TRUE;
}

/**
 * ev_annotation_get_color:
 * @annot: an #EvAnnotation
 * @color: (out): a #GdkColor to be filled with the Annotation color.
 *
 * Get the color of @annot.
 *
 * Deprecated: 3.6: Use ev_annotation_get_rgba() instead.
 */
void
ev_annotation_get_color (EvAnnotation *annot,
			 GdkColor     *color)
{
        GdkRGBA rgba;

	g_return_if_fail (EV_IS_ANNOTATION (annot));
        g_return_if_fail (color != NULL);

        ev_annotation_get_rgba (annot, &rgba);

        color->pixel = 0;
        color->red = CLAMP (rgba.red * 65535. + 0.5, 0, 65535);
        color->green = CLAMP (rgba.green * 65535. + 0.5, 0, 65535);
        color->blue = CLAMP (rgba.blue * 65535. + 0.5, 0, 65535);
}

/**
 * ev_annotation_set_color:
 * @annot: an #Evannotation
 * @color: a #GdkColor
 *
 * Set the color of the annotation to @color. You can monitor
 * changes to the annotation's color by connecting to
 * notify::color signal on @annot.
 *
 * Returns: %TRUE  when the color has been changed, %FALSE otherwise.
 *
 * Deprecated: 3.6: Use ev_annotation_set_rgba() instead.
 */
gboolean
ev_annotation_set_color (EvAnnotation   *annot,
			 const GdkColor *color)
{
        GdkColor annot_color;
        GdkRGBA rgba;

	g_return_val_if_fail (EV_IS_ANNOTATION (annot), FALSE);

        ev_annotation_get_color (annot, &annot_color);
        if (color == NULL || gdk_color_equal (color, &annot_color))
                return FALSE;

        rgba.red = color->red / 65535.;
        rgba.green = color->green / 65535.;
        rgba.blue = color->blue / 65535.;
        rgba.alpha = 1.;

        ev_annotation_set_rgba (annot, &rgba);

	return TRUE;
}

/**
 * ev_annotation_get_rgba:
 * @annot: an #EvAnnotation
 * @rgba: (out): a #GdkRGBA to be filled with the annotation color
 *
 * Gets the color of @annot.
 *
 * Since: 3.6
 */
void
ev_annotation_get_rgba (EvAnnotation *annot,
                        GdkRGBA      *rgba)
{
        g_return_if_fail (EV_IS_ANNOTATION (annot));
        g_return_if_fail (rgba != NULL);

        *rgba = annot->rgba;
}

/**
 * ev_annotation_set_rgba:
 * @annot: an #Evannotation
 * @rgba: a #GdkRGBA
 *
 * Set the color of the annotation to @rgba.
 *
 * Returns: %TRUE if the color has been changed, %FALSE otherwise
 *
 * Since: 3.6
 */
gboolean
ev_annotation_set_rgba (EvAnnotation  *annot,
                        const GdkRGBA *rgba)
{
        g_return_val_if_fail (EV_IS_ANNOTATION (annot), FALSE);
        g_return_val_if_fail (rgba != NULL, FALSE);

        if (gdk_rgba_equal (rgba, &annot->rgba))
                return FALSE;

        annot->rgba = *rgba;
        g_object_notify (G_OBJECT (annot), "rgba");
	g_object_notify (G_OBJECT (annot), "color");

        return TRUE;
}

void
ev_annotation_get_appearance (EvAnnotation           *annot,
//                              cairo_surface_t        **src,
                              const char            **appearance,
                              EvRectangle            *bounds)
{
    *appearance = annot->appearance;
    *bounds = annot->appearance_bounds;
}
void
ev_annotation_set_appearance (EvAnnotation           *annot,
                              const char             *appearance,
                              EvRectangle            *bounds)
{
    if (annot->appearance) {
        g_free(annot->appearance);
    }

    GString *str = g_string_new(appearance);

    annot->appearance = str->str;
    annot->appearance_bounds = *bounds;

    g_string_free(str, FALSE);
    //cairo_surface_reference(src);
}

/* EvAnnotationMarkup */
typedef struct {
	gchar   *label;
	gdouble  opacity;
	gboolean has_popup;
	gboolean popup_is_open;
	EvRectangle rectangle;
} EvAnnotationMarkupProps;

static void
ev_annotation_markup_default_init (EvAnnotationMarkupInterface *iface)
{
	static gboolean initialized = FALSE;

	if (!initialized) {
		g_object_interface_install_property (iface,
						     g_param_spec_string ("label",
									  "Label",
									  "Label of the markup annotation",
									  NULL,
									  G_PARAM_READWRITE |
                                                                          G_PARAM_STATIC_STRINGS));
		g_object_interface_install_property (iface,
						     g_param_spec_double ("opacity",
									  "Opacity",
									  "Opacity of the markup annotation",
									  0,
									  G_MAXDOUBLE,
									  1.,
									  G_PARAM_READWRITE |
                                                                          G_PARAM_STATIC_STRINGS));
		g_object_interface_install_property (iface,
						     g_param_spec_boolean ("has-popup",
									   "Has popup",
									   "Whether the markup annotation has "
									   "a popup window associated",
									   TRUE,
									   G_PARAM_READWRITE |
                                                                           G_PARAM_STATIC_STRINGS));
		g_object_interface_install_property (iface,
						     g_param_spec_boxed ("rectangle",
									 "Rectangle",
									 "The Rectangle of the popup associated "
									 "to the markup annotation",
									 EV_TYPE_RECTANGLE,
									 G_PARAM_READWRITE |
                                                                         G_PARAM_STATIC_STRINGS));
		g_object_interface_install_property (iface,
						     g_param_spec_boolean ("popup-is-open",
									   "PopupIsOpen",
									   "Whether the popup associated to "
									   "the markup annotation is open",
									   FALSE,
									   G_PARAM_READWRITE |
                                                                           G_PARAM_STATIC_STRINGS));
		initialized = TRUE;
	}
}

static void
ev_annotation_markup_props_free (EvAnnotationMarkupProps *props)
{
	g_free (props->label);
	g_slice_free (EvAnnotationMarkupProps, props);
}

static EvAnnotationMarkupProps *
ev_annotation_markup_get_properties (EvAnnotationMarkup *markup)
{
	EvAnnotationMarkupProps *props;
	static GQuark props_key = 0;

	if (!props_key)
		props_key = g_quark_from_static_string ("ev-annotation-markup-props");

	props = g_object_get_qdata (G_OBJECT (markup), props_key);
	if (!props) {
		props = g_slice_new0 (EvAnnotationMarkupProps);
		g_object_set_qdata_full (G_OBJECT (markup),
					 props_key, props,
					 (GDestroyNotify) ev_annotation_markup_props_free);
	}

	return props;
}

static void
ev_annotation_markup_set_property (GObject      *object,
				   guint         prop_id,
				   const GValue *value,
				   GParamSpec   *pspec)
{
	EvAnnotationMarkup *markup = EV_ANNOTATION_MARKUP (object);

	switch (prop_id) {
	case PROP_MARKUP_LABEL:
		ev_annotation_markup_set_label (markup, g_value_get_string (value));
		break;
	case PROP_MARKUP_OPACITY:
		ev_annotation_markup_set_opacity (markup, g_value_get_double (value));
		break;
	case PROP_MARKUP_HAS_POPUP:
		ev_annotation_markup_set_has_popup (markup, g_value_get_boolean (value));
		break;
	case PROP_MARKUP_RECTANGLE:
		ev_annotation_markup_set_rectangle (markup, g_value_get_boxed (value));
		break;
	case PROP_MARKUP_POPUP_IS_OPEN:
		ev_annotation_markup_set_popup_is_open (markup, g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
ev_annotation_markup_get_property (GObject    *object,
				   guint       prop_id,
				   GValue     *value,
				   GParamSpec *pspec)
{
	EvAnnotationMarkupProps *props;

	props = ev_annotation_markup_get_properties (EV_ANNOTATION_MARKUP (object));

	switch (prop_id) {
	case PROP_MARKUP_LABEL:
		g_value_set_string (value, props->label);
		break;
	case PROP_MARKUP_OPACITY:
		g_value_set_double (value, props->opacity);
		break;
	case PROP_MARKUP_HAS_POPUP:
		g_value_set_boolean (value, props->has_popup);
		break;
	case PROP_MARKUP_RECTANGLE:
		g_value_set_boxed (value, &props->rectangle);
		break;
	case PROP_MARKUP_POPUP_IS_OPEN:
		g_value_set_boolean (value, props->popup_is_open);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
ev_annotation_markup_class_install_properties (GObjectClass *klass)
{
	klass->set_property = ev_annotation_markup_set_property;
	klass->get_property = ev_annotation_markup_get_property;

	g_object_class_override_property (klass, PROP_MARKUP_LABEL, "label");
	g_object_class_override_property (klass, PROP_MARKUP_OPACITY, "opacity");
	g_object_class_override_property (klass, PROP_MARKUP_HAS_POPUP, "has-popup");
	g_object_class_override_property (klass, PROP_MARKUP_RECTANGLE, "rectangle");
	g_object_class_override_property (klass, PROP_MARKUP_POPUP_IS_OPEN, "popup-is-open");
}

const gchar *
ev_annotation_markup_get_label (EvAnnotationMarkup *markup)
{
	EvAnnotationMarkupProps *props;

	g_return_val_if_fail (EV_IS_ANNOTATION_MARKUP (markup), NULL);

	props = ev_annotation_markup_get_properties (markup);
	return props->label;
}

gboolean
ev_annotation_markup_set_label (EvAnnotationMarkup *markup,
				const gchar        *label)
{
	EvAnnotationMarkupProps *props;

	g_return_val_if_fail (EV_IS_ANNOTATION_MARKUP (markup), FALSE);
	g_return_val_if_fail (label != NULL, FALSE);

	props = ev_annotation_markup_get_properties (markup);
	if (g_strcmp0 (props->label, label) == 0)
		return FALSE;

	if (props->label)
		g_free (props->label);
	props->label = g_strdup (label);

	g_object_notify (G_OBJECT (markup), "label");

	return TRUE;
}

gdouble
ev_annotation_markup_get_opacity (EvAnnotationMarkup *markup)
{
	EvAnnotationMarkupProps *props;

	g_return_val_if_fail (EV_IS_ANNOTATION_MARKUP (markup), 1.0);

	props = ev_annotation_markup_get_properties (markup);
	return props->opacity;
}

gboolean
ev_annotation_markup_set_opacity (EvAnnotationMarkup *markup,
				  gdouble             opacity)
{
	EvAnnotationMarkupProps *props;

	g_return_val_if_fail (EV_IS_ANNOTATION_MARKUP (markup), FALSE);

	props = ev_annotation_markup_get_properties (markup);
	if (props->opacity == opacity)
		return FALSE;

	props->opacity = opacity;

	g_object_notify (G_OBJECT (markup), "opacity");

	return TRUE;
}

gboolean
ev_annotation_markup_has_popup (EvAnnotationMarkup *markup)
{
	EvAnnotationMarkupProps *props;

	g_return_val_if_fail (EV_IS_ANNOTATION_MARKUP (markup), FALSE);

	props = ev_annotation_markup_get_properties (markup);
	return props->has_popup;
}

gboolean
ev_annotation_markup_set_has_popup (EvAnnotationMarkup *markup,
				    gboolean            has_popup)
{
	EvAnnotationMarkupProps *props;

	g_return_val_if_fail (EV_IS_ANNOTATION_MARKUP (markup), FALSE);

	props = ev_annotation_markup_get_properties (markup);
	if (props->has_popup == has_popup)
		return FALSE;

	props->has_popup = has_popup;

	g_object_notify (G_OBJECT (markup), "has-popup");

	return TRUE;
}

void
ev_annotation_markup_get_rectangle (EvAnnotationMarkup *markup,
				    EvRectangle        *ev_rect)
{
	EvAnnotationMarkupProps *props;

	g_return_if_fail (EV_IS_ANNOTATION_MARKUP (markup));
	g_return_if_fail (ev_rect != NULL);

	props = ev_annotation_markup_get_properties (markup);
	*ev_rect = props->rectangle;
}

gboolean
ev_annotation_markup_set_rectangle (EvAnnotationMarkup *markup,
				    const EvRectangle  *ev_rect)
{
	EvAnnotationMarkupProps *props;

	g_return_val_if_fail (EV_IS_ANNOTATION_MARKUP (markup), FALSE);
	g_return_val_if_fail (ev_rect != NULL, FALSE);

	props = ev_annotation_markup_get_properties (markup);
	if (props->rectangle.x1 == ev_rect->x1 &&
	    props->rectangle.y1 == ev_rect->y1 &&
	    props->rectangle.x2 == ev_rect->x2 &&
	    props->rectangle.y2 == ev_rect->y2)
		return FALSE;

	props->rectangle = *ev_rect;

	g_object_notify (G_OBJECT (markup), "rectangle");

	return TRUE;
}

gboolean
ev_annotation_markup_get_popup_is_open (EvAnnotationMarkup *markup)
{
	EvAnnotationMarkupProps *props;

	g_return_val_if_fail (EV_IS_ANNOTATION_MARKUP (markup), FALSE);

	props = ev_annotation_markup_get_properties (markup);
	return props->popup_is_open;
}

gboolean
ev_annotation_markup_set_popup_is_open (EvAnnotationMarkup *markup,
					gboolean            is_open)
{
	EvAnnotationMarkupProps *props;

	g_return_val_if_fail (EV_IS_ANNOTATION_MARKUP (markup), FALSE);

	props = ev_annotation_markup_get_properties (markup);
	if (props->popup_is_open == is_open)
		return FALSE;

	props->popup_is_open = is_open;

	g_object_notify (G_OBJECT (markup), "popup_is_open");

	return TRUE;
}

/* EvAnnotationText */
static void
ev_annotation_text_init (EvAnnotationText *annot)
{
	EV_ANNOTATION (annot)->type = EV_ANNOTATION_TYPE_TEXT;
}

static void
ev_annotation_text_set_property (GObject      *object,
				 guint         prop_id,
				 const GValue *value,
				 GParamSpec   *pspec)
{
	EvAnnotationText *annot = EV_ANNOTATION_TEXT (object);

	if (prop_id < PROP_ATTACHMENT_ATTACHMENT) {
		ev_annotation_markup_set_property (object, prop_id, value, pspec);
		return;
	}

	switch (prop_id) {
	case PROP_TEXT_ICON:
		ev_annotation_text_set_icon (annot, g_value_get_enum (value));
		break;
	case PROP_TEXT_IS_OPEN:
		ev_annotation_text_set_is_open (annot, g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
ev_annotation_text_get_property (GObject    *object,
				 guint       prop_id,
				 GValue     *value,
				 GParamSpec *pspec)
{
	EvAnnotationText *annot = EV_ANNOTATION_TEXT (object);

	if (prop_id < PROP_ATTACHMENT_ATTACHMENT) {
		ev_annotation_markup_get_property (object, prop_id, value, pspec);
		return;
	}

	switch (prop_id) {
	case PROP_TEXT_ICON:
		g_value_set_enum (value, annot->icon);
		break;
	case PROP_TEXT_IS_OPEN:
		g_value_set_boolean (value, annot->is_open);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
ev_annotation_text_class_init (EvAnnotationTextClass *klass)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

	ev_annotation_markup_class_install_properties (g_object_class);

	g_object_class->set_property = ev_annotation_text_set_property;
	g_object_class->get_property = ev_annotation_text_get_property;

	g_object_class_install_property (g_object_class,
					 PROP_TEXT_ICON,
					 g_param_spec_enum ("icon",
							    "Icon",
							    "The icon fo the text annotation",
							    EV_TYPE_ANNOTATION_TEXT_ICON,
							    EV_ANNOTATION_TEXT_ICON_NOTE,
							    G_PARAM_READWRITE |
                                                            G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_TEXT_IS_OPEN,
					 g_param_spec_boolean ("is-open",
							       "IsOpen",
							       "Whether text annot is initially open",
							       FALSE,
							       G_PARAM_READWRITE |
                                                               G_PARAM_STATIC_STRINGS));
}

static void
ev_annotation_text_markup_iface_init (EvAnnotationMarkupInterface *iface)
{
}

EvAnnotation *
ev_annotation_text_new (EvPage *page)
{
	return EV_ANNOTATION (g_object_new (EV_TYPE_ANNOTATION_TEXT,
					    "page", page,
					    NULL));
}

EvAnnotationTextIcon
ev_annotation_text_get_icon (EvAnnotationText *text)
{
	g_return_val_if_fail (EV_IS_ANNOTATION_TEXT (text), 0);

	return text->icon;
}

gboolean
ev_annotation_text_set_icon (EvAnnotationText    *text,
			     EvAnnotationTextIcon icon)
{
	g_return_val_if_fail (EV_IS_ANNOTATION_TEXT (text), FALSE);

	if (text->icon == icon)
		return FALSE;

	text->icon = icon;

	g_object_notify (G_OBJECT (text), "icon");

	return TRUE;
}

gboolean
ev_annotation_text_get_is_open (EvAnnotationText *text)
{
	g_return_val_if_fail (EV_IS_ANNOTATION_TEXT (text), FALSE);

	return text->is_open;
}

gboolean
ev_annotation_text_set_is_open (EvAnnotationText *text,
				gboolean          is_open)
{
	g_return_val_if_fail (EV_IS_ANNOTATION_TEXT (text), FALSE);

	if (text->is_open == is_open)
		return FALSE;

	text->is_open = is_open;

	g_object_notify (G_OBJECT (text), "is_open");

	return TRUE;
}

/* EvAnnotationAttachment */
static void
ev_annotation_attachment_finalize (GObject *object)
{
	EvAnnotationAttachment *annot = EV_ANNOTATION_ATTACHMENT (object);

	if (annot->attachment) {
		g_object_unref (annot->attachment);
		annot->attachment = NULL;
	}

	G_OBJECT_CLASS (ev_annotation_attachment_parent_class)->finalize (object);
}

static void
ev_annotation_attachment_init (EvAnnotationAttachment *annot)
{
	EV_ANNOTATION (annot)->type = EV_ANNOTATION_TYPE_ATTACHMENT;
}

static void
ev_annotation_attachment_set_property (GObject      *object,
				       guint         prop_id,
				       const GValue *value,
				       GParamSpec   *pspec)
{
	EvAnnotationAttachment *annot = EV_ANNOTATION_ATTACHMENT (object);

	if (prop_id < PROP_ATTACHMENT_ATTACHMENT) {
		ev_annotation_markup_set_property (object, prop_id, value, pspec);
		return;
	}

	switch (prop_id) {
	case PROP_ATTACHMENT_ATTACHMENT:
		ev_annotation_attachment_set_attachment (annot, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
ev_annotation_attachment_get_property (GObject    *object,
				       guint       prop_id,
				       GValue     *value,
				       GParamSpec *pspec)
{
	EvAnnotationAttachment *annot = EV_ANNOTATION_ATTACHMENT (object);

	if (prop_id < PROP_ATTACHMENT_ATTACHMENT) {
		ev_annotation_markup_get_property (object, prop_id, value, pspec);
		return;
	}

	switch (prop_id) {
	case PROP_ATTACHMENT_ATTACHMENT:
		g_value_set_object (value, annot->attachment);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
ev_annotation_attachment_class_init (EvAnnotationAttachmentClass *klass)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

	ev_annotation_markup_class_install_properties (g_object_class);

	g_object_class->set_property = ev_annotation_attachment_set_property;
	g_object_class->get_property = ev_annotation_attachment_get_property;
	g_object_class->finalize = ev_annotation_attachment_finalize;

	g_object_class_install_property (g_object_class,
					 PROP_ATTACHMENT_ATTACHMENT,
					 g_param_spec_object ("attachment",
							      "Attachment",
							      "The attachment of the annotation",
							      EV_TYPE_ATTACHMENT,
							      G_PARAM_CONSTRUCT |
							      G_PARAM_READWRITE |
                                                              G_PARAM_STATIC_STRINGS));
}

static void
ev_annotation_attachment_markup_iface_init (EvAnnotationMarkupInterface *iface)
{
}

EvAnnotation *
ev_annotation_attachment_new (EvPage       *page,
			      EvAttachment *attachment)
{
	g_return_val_if_fail (EV_IS_ATTACHMENT (attachment), NULL);

	return EV_ANNOTATION (g_object_new (EV_TYPE_ANNOTATION_ATTACHMENT,
					    "page", page,
					    "attachment", attachment,
					    NULL));
}

/**
 * ev_annotation_attachment_get_attachment:
 * @annot: an #EvAnnotationAttachment
 *
 * Returns: (transfer none): an #EvAttachment
 */
EvAttachment *
ev_annotation_attachment_get_attachment (EvAnnotationAttachment *annot)
{
	g_return_val_if_fail (EV_IS_ANNOTATION_ATTACHMENT (annot), NULL);

	return annot->attachment;
}

gboolean
ev_annotation_attachment_set_attachment (EvAnnotationAttachment *annot,
					 EvAttachment           *attachment)
{
	g_return_val_if_fail (EV_IS_ANNOTATION_ATTACHMENT (annot), FALSE);

	if (annot->attachment == attachment)
		return FALSE;

	if (annot->attachment)
		g_object_unref (annot->attachment);
	annot->attachment = attachment ? g_object_ref (attachment) : NULL;

	g_object_notify (G_OBJECT (annot), "attachment");

	return TRUE;
}

/* EvAnnotationTextMarkup */
static void
ev_annotation_text_markup_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
	EvAnnotationTextMarkup *annot = EV_ANNOTATION_TEXT_MARKUP (object);

	if (prop_id < PROP_TEXT_MARKUP_TYPE) {
		ev_annotation_markup_get_property (object, prop_id, value, pspec);
		return;
	}

	switch (prop_id) {
	case PROP_TEXT_MARKUP_TYPE:
		g_value_set_enum (value, annot->type);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
ev_annotation_text_markup_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
	EvAnnotationTextMarkup *annot = EV_ANNOTATION_TEXT_MARKUP (object);

	if (prop_id < PROP_TEXT_MARKUP_TYPE) {
		ev_annotation_markup_set_property (object, prop_id, value, pspec);
		return;
	}

	switch (prop_id) {
	case PROP_TEXT_MARKUP_TYPE:
                annot->type = g_value_get_enum (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
ev_annotation_text_markup_init (EvAnnotationTextMarkup *annot)
{
        EV_ANNOTATION (annot)->type = EV_ANNOTATION_TYPE_TEXT_MARKUP;
}

static void
ev_annotation_text_markup_class_init (EvAnnotationTextMarkupClass *class)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (class);

	ev_annotation_markup_class_install_properties (g_object_class);

        g_object_class->get_property = ev_annotation_text_markup_get_property;
        g_object_class->set_property = ev_annotation_text_markup_set_property;

        g_object_class_install_property (g_object_class,
					 PROP_TEXT_MARKUP_TYPE,
					 g_param_spec_enum ("type",
							    "Type",
							    "The text markup annotation type",
							    EV_TYPE_ANNOTATION_TEXT_MARKUP_TYPE,
							    EV_ANNOTATION_TEXT_MARKUP_HIGHLIGHT,
							    G_PARAM_READWRITE |
                                                            G_PARAM_CONSTRUCT |
                                                            G_PARAM_STATIC_STRINGS));
}

static void
ev_annotation_text_markup_markup_iface_init (EvAnnotationMarkupInterface *iface)
{
}

EvAnnotation *
ev_annotation_text_markup_highlight_new (EvPage *page)
{
        EvAnnotation *annot = EV_ANNOTATION (g_object_new (EV_TYPE_ANNOTATION_TEXT_MARKUP,
                                                           "page", page,
                                                           "type", EV_ANNOTATION_TEXT_MARKUP_HIGHLIGHT,
                                                           NULL));
        return annot;
}

EvAnnotation *
ev_annotation_text_markup_strike_out_new (EvPage *page)
{
        EvAnnotation *annot = EV_ANNOTATION (g_object_new (EV_TYPE_ANNOTATION_TEXT_MARKUP,
                                                           "page", page,
                                                           "type", EV_ANNOTATION_TEXT_MARKUP_STRIKE_OUT,
                                                           NULL));
        return annot;
}

EvAnnotation *
ev_annotation_text_markup_underline_new (EvPage *page)
{
        EvAnnotation *annot = EV_ANNOTATION (g_object_new (EV_TYPE_ANNOTATION_TEXT_MARKUP,
                                                           "page", page,
                                                           "type", EV_ANNOTATION_TEXT_MARKUP_UNDERLINE,
                                                           NULL));
        return annot;
}

EvAnnotation *
ev_annotation_ink_new (EvPage *page)
{
        EvAnnotation *annot = EV_ANNOTATION (g_object_new (EV_TYPE_ANNOTATION_INK,
                                                           "page", page,
                                                           NULL));
	return annot;
}

EvAnnotationTextMarkupType
ev_annotation_text_markup_get_markup_type (EvAnnotationTextMarkup *annot)
{
        g_return_val_if_fail (EV_IS_ANNOTATION_TEXT_MARKUP (annot), EV_ANNOTATION_TEXT_MARKUP_HIGHLIGHT);

        return annot->type;
}

gboolean
ev_annotation_text_markup_set_markup_type (EvAnnotationTextMarkup    *annot,
                                           EvAnnotationTextMarkupType markup_type)
{
        g_return_val_if_fail (EV_IS_ANNOTATION_TEXT_MARKUP (annot), FALSE);

        if (annot->type == markup_type)
                return FALSE;

        annot->type = markup_type;
        g_object_notify (G_OBJECT (annot), "type");

        return TRUE;
}

/* EvAnnotationInk */
static void
ev_annotation_ink_init (EvAnnotationInk *annot)
{
	EV_ANNOTATION (annot)->type = EV_ANNOTATION_TYPE_INK;
    annot->widths = NULL;
    annot->paths = NULL;
    annot->quadtree = NULL;
}

static void
ev_annotation_ink_finalize (GObject *object)
{
	EvAnnotationInk *annot = EV_ANNOTATION_INK (object);
    int i;

	if (annot->widths) {
		g_array_unref (annot->widths);
		annot->widths = NULL;
	}
	if (annot->paths) {
        for (i=0; i<annot->paths->len; i++) {
            g_array_unref(g_array_index(annot->paths, GArray*, i));
        }
        g_array_unref(annot->paths);
		annot->paths = NULL;
	}

	G_OBJECT_CLASS (ev_annotation_ink_parent_class)->finalize (object);
}


static void
ev_annotation_ink_set_property (GObject      *object,
				 guint         prop_id,
				 const GValue *value,
				 GParamSpec   *pspec)
{
	EvAnnotationInk *annot = EV_ANNOTATION_INK (object);

	if (prop_id < PROP_INK_PATHS) {
		ev_annotation_markup_set_property (object, prop_id, value, pspec);
		return;
	}

	switch (prop_id) {
	case PROP_INK_PATHS:
		ev_annotation_ink_set_paths (annot, g_value_get_pointer(value));
		break;
	case PROP_INK_WIDTH: {
		double width = g_value_get_double(value);
		ev_annotation_ink_set_width (annot, width);
		}
		break;
	case PROP_INK_WIDTHS: {
		GArray *widths = g_value_get_pointer(value);
		ev_annotation_ink_set_widths (annot, widths);
		}
		break;
	case PROP_INK_OPERATOR:
		ev_annotation_ink_set_operator (annot, g_value_get_enum(value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}


void
ev_annotation_ink_set_widths (	EvAnnotationInk *annot,
				GArray *widths )
{
	annot->width = 0;
	if (annot->widths) {
		g_array_unref(annot->widths);
	}
	annot->widths = widths;
	g_array_ref(widths);
}


gboolean
ev_annotation_ink_get_width (	EvAnnotationInk *annot,
                                double *width )
{
    if (width) {
        *width = annot->width;
    }
    return TRUE;
}


void
ev_annotation_ink_set_width (	EvAnnotationInk *annot,
				double width )
{
	annot->width = width;
	if (annot->widths) {
		g_array_unref(annot->widths);
		annot->widths = NULL;
	}
}


void
ev_annotation_ink_set_operator (EvAnnotationInk *annot,
				int op)
{
	annot->operator = (EvAnnotationInkOperator)op;
}

static gdouble squared_distance(gdouble x, gdouble y)
{
    return x*x + y*y;
}

static gboolean
is_on_line(gpointer a, gdouble x, gdouble y, gpointer data)
{
    EvAnnotationInk *annot = EV_ANNOTATION_INK(data);
    EvRectangle *rect = (EvRectangle*) a;

    gdouble halfwidth = annot->width * 0.5;
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
    if (sq_line_length != 0) {
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

gboolean
ev_annotation_ink_get_paths(	EvAnnotationInk *annot,
				GArray **paths)
{
    *paths = annot->paths;   
    return TRUE;
}

void
ev_annotation_ink_set_paths(	EvAnnotationInk *annot,
				GArray *paths)
{
	int i;
    
    // Copy the path into the array 
    if (annot->paths) {
        for (i=0; i<annot->paths->len; i++) {
            g_array_unref(g_array_index(annot->paths, GArray*, i));
        }
        g_array_unref(annot->paths);
    }

	annot->paths = paths;

	for (i=0; i<paths->len; i++) {
		g_array_ref(g_array_index(paths, GArray*, i));
	}
	g_array_ref(paths);

    // Find the extents of the annotation
    gboolean first_time = TRUE;
    gdouble min_x = 0, max_x = 1, min_y = 1, max_y = 1;
    gdouble half_width = annot->width / 2;
	for (i=0; i<paths->len; i++) {
        GArray *path = g_array_index(paths, GArray*, i);
        int j;
        for (j=0; j<path->len; j+=2) {
            if (first_time) {
                max_x = min_x = g_array_index(path, gdouble, j);
                max_y = min_y = g_array_index(path, gdouble, j + 1);
                first_time = FALSE;
            }
            
            min_x = MIN(min_x, g_array_index(path, gdouble, j));
            min_y = MIN(min_y, g_array_index(path, gdouble, j + 1));
            max_x = MAX(max_x, g_array_index(path, gdouble, j));
            max_y = MAX(max_y, g_array_index(path, gdouble, j + 1));
        }
	}


    // Copy the path into the quadtree data structure
    if (annot->quadtree) {
        ev_mapping_tree_unref(annot->quadtree);
    }
    EvRectangle extents;
    extents.x1 = min_x - half_width;
    extents.y1 = min_y - half_width;
    extents.x2 = max_x + half_width;
    extents.y2 = max_y + half_width;
    annot->quadtree = ev_mapping_tree_new(0, extents, g_free);

	for (i=0; i<paths->len; i++) {
        GArray *path = g_array_index(annot->paths, GArray*, i);
        int j;
        gdouble xp, yp;
        for (j=0; j<path->len; j += 2) {
            gdouble x, y;
            x = g_array_index(path, gdouble, j);
            y = g_array_index(path, gdouble, j + 1);

            if (j > 0) {
                EvRectangle rect;
                EvRectangle *line = g_malloc(sizeof(EvRectangle));

                rect.x1 = MIN(x, xp) - half_width;
                rect.x2 = MAX(x, xp) + half_width;
                rect.y1 = MIN(y, yp) - half_width;
                rect.y2 = MAX(y, yp) + half_width;

                line->x1 = xp;
                line->x2 = x;
                line->y1 = yp;
                line->y2 = y;

                ev_mapping_tree_add(annot->quadtree,
                            line, rect,
                            is_on_line, annot);
            }

            xp = x; yp = y;
        }
	}
}

gboolean
ev_annotation_ink_is_hit (EvAnnotationInk *annot, gdouble x, gdouble y)
{
    gpointer hit_item = ev_mapping_tree_get(annot->quadtree, x, y);

    return (hit_item != 0);
}

static void
ev_annotation_ink_get_property (GObject    *object,
				 guint       prop_id,
				 GValue     *value,
				 GParamSpec *pspec)
{
	EvAnnotationInk *annot = EV_ANNOTATION_INK (object);

	if (prop_id < PROP_ATTACHMENT_ATTACHMENT) {
		ev_annotation_markup_get_property (object, prop_id, value, pspec);
		return;
	}

	switch (prop_id) {
	case PROP_INK_PATHS:
		g_value_set_pointer (value, annot->paths);
		break;
	case PROP_INK_WIDTH:
		g_value_set_double (value, annot->width);
		break;
	case PROP_INK_WIDTHS:
		g_value_set_pointer (value, annot->widths);
		break;
	case PROP_INK_OPERATOR:
		g_value_set_enum (value, annot->operator);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
ev_annotation_ink_markup_iface_init (EvAnnotationMarkupInterface *iface)
{
}

static void
ev_annotation_ink_class_init (EvAnnotationInkClass *klass)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

	ev_annotation_markup_class_install_properties (g_object_class);

	g_object_class->finalize = ev_annotation_ink_finalize;
	g_object_class->set_property = ev_annotation_ink_set_property;
	g_object_class->get_property = ev_annotation_ink_get_property;

	// TODO:
	g_object_class_install_property (g_object_class,
					 PROP_INK_WIDTHS,
					 g_param_spec_enum ("operator",
							    "Operator",
							    "Blending mode",
							    EV_TYPE_ANNOTATION_INK_OPERATOR,
							    EV_ANNOTATION_INK_OPERATOR_OVER,
							    G_PARAM_READWRITE |
                                                            G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_INK_WIDTHS,
					 g_param_spec_pointer ("widths",
							       "Widths",
							       "The width of each segment",
							       G_PARAM_READWRITE |
                                                               G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_INK_WIDTH,
					 g_param_spec_double ("width",
							       "Width",
							       "The width of the line (for constant width)",
							       0, 1e10,
                                   1,
							       G_PARAM_READWRITE |
                                                               G_PARAM_STATIC_STRINGS));
}


