/*  date.c
 *
 *  Copyright (C) 2003 Choe Hwanjin(krisna@kldp.org)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <time.h>
#include <gdk/gdkkeysyms.h>

#include <libxfce4util/i18n.h>
#include <libxfce4util/debug.h>

#include <panel/global.h>
#include <panel/controls.h>
#include <panel/icons.h>
#include <panel/plugins.h>

typedef struct {
    GtkWidget *eventbox;
    GtkWidget *date_label;
    GtkWidget *time_label;
    gchar *date_font;
    gchar *time_font;
    gchar *date_format;
    gchar *time_format;
    guint timeout_id;
    gint orientation;
    gboolean week_start_monday;

    GtkWidget *date_font_selector;
    GtkWidget *date_format_entry;
    GtkWidget *time_font_selector;
    GtkWidget *time_format_entry;
    GtkWidget *cal;
} DatetimePlugin;

static gboolean
datetime_update(gpointer data)
{
    GTimeVal timeval;
    gchar buf[256];
    gchar *utf8str;
    int len;
    struct tm *current;
    DatetimePlugin *datetime;

    if (data == NULL)
	return FALSE;

    datetime = (DatetimePlugin*)data;

    if (!GTK_IS_LABEL(datetime->date_label) &&
	!GTK_IS_LABEL(datetime->time_label))
	return FALSE;

    g_get_current_time(&timeval);
    current = localtime((time_t *)&timeval.tv_sec);
    if (GTK_IS_LABEL(datetime->date_label)) {
	len = strftime(buf, sizeof(buf) - 1, datetime->date_format, current);
	if (len != 0) {
	    buf[len] = '\0';  /* make sure nul terminated string */
	    utf8str = g_locale_to_utf8(buf, len, NULL, NULL, NULL);
	    if (utf8str != NULL) {
		gtk_label_set_text(GTK_LABEL(datetime->date_label), utf8str);
		g_free(utf8str);
	    }
	} else 
	    gtk_label_set_text(GTK_LABEL(datetime->date_label), "Error");
    }

    if (GTK_IS_LABEL(datetime->time_label)) {
	len = strftime(buf, sizeof(buf) - 1, datetime->time_format, current);
	if (len != 0) {
	    buf[len] = '\0';  /* make sure nul terminated string */
	    utf8str = g_locale_to_utf8(buf, len, NULL, NULL, NULL);
	    if (utf8str != NULL) {
		gtk_label_set_text(GTK_LABEL(datetime->time_label), utf8str);
		g_free(utf8str);
	    }
	} else 
	    gtk_label_set_text(GTK_LABEL(datetime->time_label), "Error");
    }

    return TRUE;
}

static GtkWidget *
pop_calendar_window(GtkWidget *parent, int orientation,
		    gboolean week_start_monday)
{
    GtkWidget *window;
    GtkWidget *cal;
    gint parent_x, parent_y, parent_w, parent_h;
    gint root_w, root_h;
    gint width, height, x, y;
    GtkCalendarDisplayOptions display_options;
    GtkRequisition requisition;
    GtkAllocation allocation;

    window = gtk_window_new(GTK_WINDOW_POPUP);

    cal = gtk_calendar_new();
    display_options = GTK_CALENDAR_SHOW_HEADING | GTK_CALENDAR_SHOW_DAY_NAMES;
    if (week_start_monday)
	display_options |= GTK_CALENDAR_WEEK_START_MONDAY;
    gtk_calendar_display_options(GTK_CALENDAR (cal), display_options);
    gtk_container_add(GTK_CONTAINER(window), cal);

    gdk_window_get_origin(GDK_WINDOW(parent->window), &parent_x, &parent_y);
    gdk_drawable_get_size(GDK_DRAWABLE(parent->window), &parent_w, &parent_h);

    root_w = gdk_screen_width();
    root_h = gdk_screen_height();

    gtk_widget_realize(GTK_WIDGET(window));

    gtk_widget_size_request(GTK_WIDGET(cal), &requisition);

    allocation.x = requisition.width;
    allocation.y = requisition.height;
    gtk_widget_size_allocate(GTK_WIDGET(cal), &allocation);

    gtk_widget_size_request(GTK_WIDGET(cal), &requisition);
    width = requisition.width;
    height = requisition.height;

    /*
    g_print("parent: %dx%d +%d+%d\n", parent_w, parent_h, parent_x, parent_y);
    g_print("root: %dx%d\n", root_w, root_h);
    g_print("calendar: %dx%d\n", width, height);
    */

    if (orientation == GTK_ORIENTATION_VERTICAL) {
        if (parent_x < root_w / 2) {
            if (parent_y < root_h / 2) {
                /* upper left */
                x = parent_x + parent_w;
                y = parent_y;
            } else {
                /* lower left */
                x = parent_x + parent_w;
                y = parent_y + parent_h - height;
            }
        } else {
            if (parent_y < root_h / 2) {
                /* upper right */
                x = parent_x - width;
                y = parent_y;
            } else {
                /* lower right */
                x = parent_x - width;
                y = parent_y + parent_h - height;
            }
        }
    } else {
        if (parent_x < root_w / 2) {
            if (parent_y < root_h / 2) {
                /* upper left */
                x = parent_x;
                y = parent_y + parent_h;
            } else {
                /* lower left */
                x = parent_x;
                y = parent_y - height;
            }
        } else {
            if (parent_y < root_h / 2) {
                /* upper right */
                x = parent_x + parent_w - width;
                y = parent_y + parent_h;
            } else {
                /* lower right */
                x = parent_x + parent_w - width;
                y = parent_y - height;
            }
        }
    }

    gtk_window_move(GTK_WINDOW(window), x, y);
    gtk_widget_show(cal);
    gtk_widget_show(window);

    return window;
}

static gboolean
on_button_press_event_cb(GtkWidget *widget,
			 GdkEventButton *event, gpointer data)
{
    if (event->button == 1) {
    	DatetimePlugin *datetime;

	if (data == NULL)
	    return FALSE;

	datetime = (DatetimePlugin*)data;
	if (datetime->cal != NULL) {
	    gtk_widget_destroy(datetime->cal);
	    datetime->cal = NULL;
	} else {
	    datetime->cal = pop_calendar_window(datetime->eventbox,
						datetime->orientation,
						datetime->week_start_monday);
	}
	return TRUE;
    }
    return FALSE;
}

static gboolean
on_key_press_event_cb(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    if (event->keyval == GDK_Escape) {
    	DatetimePlugin *datetime = (DatetimePlugin*)data;

	if (datetime != NULL && datetime->cal) {
	    gtk_widget_destroy (GTK_WIDGET(datetime->cal));
	    datetime->cal = NULL;
	}
	return TRUE;
    }   
    return FALSE;
}

static void
datetime_apply_format(DatetimePlugin *datetime,
		      const char *date_format,
		      const char *time_format)
{
    if (datetime == NULL)
	return;

    if (date_format != NULL) {
	g_free(datetime->date_format);
	datetime->date_format = g_strcompress(date_format);
    }

    if (time_format != NULL) {
	g_free(datetime->time_format);
	datetime->time_format = g_strcompress(time_format);
    }

    if (datetime->timeout_id)
	g_source_remove(datetime->timeout_id);

    if (strstr(datetime->date_format, "%S") != NULL ||
	strstr(datetime->date_format, "%s") != NULL ||
	strstr(datetime->date_format, "%r") != NULL ||
	strstr(datetime->date_format, "%T") != NULL ||
        strstr(datetime->time_format, "%S") != NULL ||
	strstr(datetime->time_format, "%s") != NULL ||
	strstr(datetime->time_format, "%r") != NULL ||
	strstr(datetime->time_format, "%T") != NULL)
	datetime->timeout_id = g_timeout_add(1000, datetime_update, datetime);
    else
	datetime->timeout_id = g_timeout_add(10000, datetime_update, datetime);
}

static DatetimePlugin *
datetime_new (void)
{
    GtkWidget *vbox;
    GtkWidget *align;
    DatetimePlugin *datetime = g_new (DatetimePlugin, 1);

    datetime->eventbox = gtk_event_box_new();
    gtk_widget_add_events(datetime->eventbox, GDK_KEY_PRESS_MASK);
    g_signal_connect(G_OBJECT(datetime->eventbox), "button-press-event",
	    	     G_CALLBACK(on_button_press_event_cb), datetime);
    g_signal_connect(G_OBJECT(datetime->eventbox), "key-press-event",
	    	     G_CALLBACK(on_key_press_event_cb), datetime);

    align = gtk_alignment_new(0.5, 0.5, 0, 0);
    gtk_container_add(GTK_CONTAINER(datetime->eventbox), align);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), border_width);
    gtk_container_add(GTK_CONTAINER(align), vbox);

    /* time */
    datetime->time_label = gtk_label_new("");
    gtk_label_set_justify(GTK_LABEL(datetime->time_label), GTK_JUSTIFY_CENTER);
    datetime->time_font = g_strdup("Bitstream Vera Sans 11");
    datetime->time_format = g_strdup(_("%H:%M"));
    gtk_box_pack_start(GTK_BOX(vbox), datetime->time_label, FALSE, FALSE, 0);

    /* date */
    datetime->date_label = gtk_label_new("");
    gtk_label_set_justify(GTK_LABEL(datetime->date_label), GTK_JUSTIFY_CENTER);
    datetime->date_font = g_strdup("Bitstream Vera Sans 11");
    datetime->date_format = g_strdup(_("%Y-%m-%d"));
    gtk_box_pack_start(GTK_BOX(vbox), datetime->date_label, FALSE, FALSE, 0);

    datetime_update(datetime);
    gtk_widget_show_all(datetime->eventbox);

    datetime->cal = NULL;
    datetime->orientation = GTK_ORIENTATION_HORIZONTAL;
    datetime->week_start_monday = FALSE;

    datetime_apply_format(datetime, NULL, NULL);

    return datetime;
}

static void
datetime_free(Control *control)
{
    DatetimePlugin *datetime;

    g_return_if_fail (control != NULL);

    datetime = control->data;
    g_return_if_fail (datetime != NULL);

    if (datetime->timeout_id)
	g_source_remove(datetime->timeout_id);

    g_free(datetime);
}

static void
datetime_apply_font(DatetimePlugin *datetime,
		    const gchar *date_font_name,
		    const gchar *time_font_name)
{
    PangoFontDescription *font;

    if (date_font_name != NULL) {
	g_free(datetime->date_font);
	datetime->date_font = g_strdup(date_font_name);
	font = pango_font_description_from_string(date_font_name);
	gtk_widget_modify_font(datetime->date_label, font);
    }

    if (time_font_name != NULL) {
	g_free(datetime->time_font);
	datetime->time_font = g_strdup(time_font_name);
	font = pango_font_description_from_string(time_font_name);
	gtk_widget_modify_font(datetime->time_label, font);
    }
}

extern xmlDocPtr xmlconfig;

static void
datetime_read_config(Control *control, xmlNodePtr node)
{
    DatetimePlugin *datetime;
    xmlChar *value;

    g_return_if_fail (control != NULL);
    g_return_if_fail (node != NULL);

    datetime = (DatetimePlugin*)control->data;

    node = node->children;
    if (node == NULL)
	return;

    while (node != NULL) {
	if (xmlStrEqual(node->name, (const xmlChar *)"Date")) {
	    xmlNodePtr tmp = node->children;
	    while (tmp != NULL) {
		if (xmlStrEqual(tmp->name, (const xmlChar *)"Font")) {
		    value = xmlNodeListGetString(xmlconfig,
						 tmp->children, 1);
		    if (value != NULL) {
			datetime_apply_font(datetime, value, NULL);
			xmlFree(value);
		    }
		} else if (xmlStrEqual(tmp->name, (const xmlChar *)"Format")) {
		    value = xmlNodeListGetString(xmlconfig, tmp->children, 1);
		    if (value != NULL) {
			datetime_apply_format(datetime, value, NULL);
			xmlFree(value);
		    }
		}
		tmp = tmp->next;
	    }
	} else if (xmlStrEqual(node->name, (const xmlChar *)"Time")) {
	    xmlNodePtr tmp = node->children;
	    while (tmp != NULL) {
		if (xmlStrEqual(tmp->name, (const xmlChar *)"Font")) {
		    value = xmlNodeListGetString(xmlconfig, tmp->children, 1);
		    if (value != NULL) {
			datetime_apply_font(datetime, NULL, value);
			xmlFree(value);
		    }
		} else if (xmlStrEqual(tmp->name, (const xmlChar *)"Format")) {
		    value = xmlNodeListGetString(xmlconfig, tmp->children, 1);
		    if (value != NULL) {
			datetime_apply_format(datetime, NULL, value);
			xmlFree(value);
		    }
		}
		tmp = tmp->next;
	    }
	} else if (xmlStrEqual(node->name, (const xmlChar *)"Calendar")) {
	    value = xmlGetProp(node, (const xmlChar *)"WeekStartsMonday");
	    if (g_ascii_strcasecmp("true", value) == 0)
		datetime->week_start_monday = TRUE;
	    else
		datetime->week_start_monday = FALSE;
	}
	node = node->next;
    }

    datetime_update(datetime);
}

static void
datetime_write_config(Control *control, xmlNodePtr parent)
{
    DatetimePlugin *datetime;
    gchar *format;
    xmlNodePtr node;

    g_return_if_fail (control != NULL);
    g_return_if_fail (parent != NULL);
   
    datetime = (DatetimePlugin*)control->data;
    g_return_if_fail (datetime != NULL);

    node = xmlNewTextChild(parent, NULL, (const xmlChar *)"Date", NULL);
    xmlNewTextChild(node, NULL, (const xmlChar *)"Font", datetime->date_font);
    format = g_strescape(datetime->date_format, NULL);
    xmlNewTextChild(node, NULL, (const xmlChar *)"Format", format);
    g_free(format);

    node = xmlNewTextChild(parent, NULL, (const xmlChar *)"Time", NULL);
    xmlNewTextChild(node, NULL, (const xmlChar *)"Font", datetime->time_font);
    format = g_strescape(datetime->time_format, NULL);
    xmlNewTextChild(node, NULL, (const xmlChar *)"Format", format);
    g_free(format);

    node = xmlNewTextChild(parent, NULL, (const xmlChar *)"Calendar", NULL);
    if (datetime->week_start_monday)
	xmlSetProp(node, "WeekStartsMonday", "true");
    else
	xmlSetProp(node, "WeekStartsMonday", "false");
}

static void
datetime_attach_callback(Control *control, const char *signal,
		     GCallback callback, gpointer data)
{
}

static void
datetime_date_font_selection_cb(GtkWidget *widget, gpointer data)
{
    DatetimePlugin *datetime;
    GtkWidget *dialog;
    gint result;

    g_return_if_fail (data != NULL);

    datetime = (DatetimePlugin*)data;

    dialog = gtk_font_selection_dialog_new(_("Select font"));
    gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(dialog),
	   	 			    datetime->date_font);
    gtk_font_selection_dialog_set_preview_text(GTK_FONT_SELECTION_DIALOG(dialog),
			gtk_label_get_text(GTK_LABEL(datetime->date_label)));

    result = gtk_dialog_run(GTK_DIALOG(dialog));
    if (result == GTK_RESPONSE_OK || result == GTK_RESPONSE_ACCEPT) {
	gchar *font_name;
	font_name = gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(dialog));
	if (font_name != NULL) {
	    gtk_button_set_label(GTK_BUTTON(widget), font_name);
	    datetime_apply_font(datetime, font_name, NULL);
	}
    }
    gtk_widget_destroy(dialog);
}

static void
datetime_time_font_selection_cb(GtkWidget *widget, gpointer data)
{
    DatetimePlugin *datetime;
    GtkWidget *dialog;
    gint result;

    g_return_if_fail (data != NULL);

    datetime = (DatetimePlugin*)data;

    dialog = gtk_font_selection_dialog_new(_("Select font"));
    gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(dialog),
	   	 			    datetime->time_font);
    gtk_font_selection_dialog_set_preview_text(GTK_FONT_SELECTION_DIALOG(dialog),
			gtk_label_get_text(GTK_LABEL(datetime->time_label)));

    result = gtk_dialog_run(GTK_DIALOG(dialog));
    if (result == GTK_RESPONSE_OK || result == GTK_RESPONSE_ACCEPT) {
	gchar *font_name;
	font_name = gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(dialog));
	if (font_name != NULL) {
	    gtk_button_set_label(GTK_BUTTON(widget), font_name);
	    datetime_apply_font(datetime, NULL, font_name);
	}
    }
    gtk_widget_destroy(dialog);
}

static void
date_entry_activate_cb (GtkWidget *widget, DatetimePlugin *datetime)
{
    const gchar *format;
    format = gtk_entry_get_text(GTK_ENTRY(widget));
    if (format != NULL)
	datetime_apply_format(datetime, format, NULL);
    datetime_update(datetime);
}

static void
time_entry_activate_cb (GtkWidget *widget, DatetimePlugin *datetime)
{
    const gchar *format;
    format = gtk_entry_get_text(GTK_ENTRY(widget));
    if (format != NULL)
	datetime_apply_format(datetime, NULL, format);
    datetime_update(datetime);
}

static void
week_day_button_toggle_cb (GtkWidget *widget, gpointer data)
{
    DatetimePlugin *datetime = (DatetimePlugin*)data;

    datetime->week_start_monday = 
	gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

static void
datetime_create_options(Control *control, GtkContainer *container, GtkWidget *done)
{
    DatetimePlugin *datetime;
    GtkWidget *main_vbox;
    GtkWidget *frame;
    GtkWidget *label;
    GtkWidget *entry;
    GtkWidget *button;
    GtkWidget *vbox;
    GtkWidget *hbox;
    GtkSizeGroup *sg;
    gchar *format;

    g_return_if_fail (control != NULL);
    g_return_if_fail (container != NULL);
    g_return_if_fail (done != NULL);

    datetime = (DatetimePlugin*)control->data;
    g_return_if_fail (datetime != NULL);

    main_vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(container), main_vbox);

    gtk_widget_show_all(main_vbox);

    sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

    /* time */
    frame = xfce_framebox_new(_("Time"), TRUE);
    gtk_box_pack_start(GTK_BOX(main_vbox), frame, TRUE, TRUE, 0);

    vbox = gtk_vbox_new(FALSE, 0);
    xfce_framebox_add(XFCE_FRAMEBOX(frame), vbox);

    hbox = gtk_hbox_new(FALSE, border_width);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    label = gtk_label_new(_("Font:"));
    gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
    gtk_size_group_add_widget(sg, label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    button = gtk_button_new_with_label(datetime->time_font);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(button), "clicked",
	    	     G_CALLBACK(datetime_time_font_selection_cb), datetime);
    datetime->time_font_selector = button;

    hbox = gtk_hbox_new(FALSE, border_width);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    label = gtk_label_new(_("Format:"));
    gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
    gtk_size_group_add_widget(sg, label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    entry = gtk_entry_new();
    format = g_strescape(datetime->time_format, NULL);
    gtk_entry_set_text(GTK_ENTRY(entry), format);
    gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
    g_free(format);
    g_signal_connect (G_OBJECT(entry), "activate",
		      G_CALLBACK (time_entry_activate_cb), datetime);
    datetime->date_format_entry = entry;

    /* date */
    frame = xfce_framebox_new(_("Date"), TRUE);
    gtk_box_pack_start(GTK_BOX(main_vbox), frame, TRUE, TRUE, 0);

    vbox = gtk_vbox_new(FALSE, 0);
    xfce_framebox_add(XFCE_FRAMEBOX(frame), vbox);

    hbox = gtk_hbox_new(FALSE, border_width);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    label = gtk_label_new(_("Font:"));
    gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
    gtk_size_group_add_widget(sg, label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    button = gtk_button_new_with_label(datetime->date_font);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(button), "clicked",
	    	     G_CALLBACK(datetime_date_font_selection_cb), datetime);
    datetime->date_font_selector = button;

    hbox = gtk_hbox_new(FALSE, border_width);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    label = gtk_label_new(_("Format:"));
    gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
    gtk_size_group_add_widget(sg, label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    entry = gtk_entry_new();
    format = g_strescape(datetime->date_format, NULL);
    gtk_entry_set_text(GTK_ENTRY(entry), format);
    gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
    g_free(format);
    g_signal_connect (G_OBJECT(entry), "activate",
		      G_CALLBACK (date_entry_activate_cb), datetime);
    datetime->date_format_entry = entry;

    /* first day of week */
    frame = xfce_framebox_new(_("Calendar"), TRUE);
    gtk_box_pack_start(GTK_BOX(main_vbox), frame, TRUE, TRUE, 0);

    button = gtk_check_button_new_with_label(_("Week day starts Monday"));
    xfce_framebox_add(XFCE_FRAMEBOX(frame), button);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
				 datetime->week_start_monday);
    g_signal_connect(G_OBJECT(button), "toggled",
	    	     G_CALLBACK(week_day_button_toggle_cb), datetime);

    gtk_widget_show_all(main_vbox);
}

static void
datetime_set_size(Control *control, int size)
{
    g_return_if_fail (control != NULL);

    gtk_widget_set_size_request (control->base, -1, -1);
}

static void
datetime_set_orientation(Control *control, int orientation)
{
    DatetimePlugin *datetime = control->data;

    datetime->orientation = orientation;
}

/*  Date panel control
 *  -------------------
*/
static gboolean
create_datetime_control (Control * control)
{
    DatetimePlugin *datetime = datetime_new();

    gtk_container_add (GTK_CONTAINER (control->base), datetime->eventbox);

    control->data = (gpointer) datetime;
    control->with_popup = FALSE;

    gtk_widget_set_size_request (control->base, -1, -1);

    return TRUE;
}

G_MODULE_EXPORT void
xfce_control_class_init (ControlClass * cc)
{
    cc->name = "datetime";
    cc->caption = "Date and Time";

    cc->create_control = (CreateControlFunc) create_datetime_control;

    cc->free = datetime_free;
    cc->read_config = datetime_read_config;
    cc->write_config = datetime_write_config;
    cc->attach_callback = datetime_attach_callback;

    cc->create_options = datetime_create_options;

    cc->set_orientation = datetime_set_orientation;

    cc->set_size = datetime_set_size;
}

/* macro defined in plugins.h */
XFCE_PLUGIN_CHECK_INIT

// vim: set ts=8 sw=4 :
