/*  $Id$
 *
 *  Copyright (C) 2003 Choe Hwanjin(krisna@kldp.org)
 *  Copyright (c) 2006 Remco den Breeje <remco@sx.mine.nu>
 *  Copyright (c) 2008 Diego Ongaro <ongardie@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Library General Public License as published
 *  by the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* local includes */
#include <time.h>
#include <string.h>

/* xfce includes */
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4panel/xfce-panel-plugin.h>
#include <libxfce4panel/xfce-panel-convenience.h>

#include "datetime.h"
#include "datetime-dialog.h"

#define DATETIME_MAX_STRLEN 256

/* check for new Xfce 4.10 panel features */
#ifdef LIBXFCE4PANEL_CHECK_VERSION
#if LIBXFCE4PANEL_CHECK_VERSION (4,9,0)
#define HAS_PANEL_49
#endif
#endif

#ifdef HAS_PANEL_49
static void datetime_set_mode(XfcePanelPlugin *plugin, XfcePanelPluginMode mode, t_datetime *datetime);
#else
static void datetime_set_orientation(XfcePanelPlugin *plugin, GtkOrientation orientation, t_datetime *datetime);
{
#endif

/**
 *  Convert a GTimeVal to milliseconds.
 *  Fractions of a millisecond are truncated.
 *  With a 32 bit word size and some values of t.tv_sec,
 *  multiplication by 1000 could overflow a glong,
 *  so the value is cast to a gint64.
 */
static inline gint64 datetime_gtimeval_to_ms(const GTimeVal t)
{
  return ((gint64) t.tv_sec * 1000) + ((gint64) t.tv_usec / 1000);
}

/*
 * Compute the wake interval,
 * which is the time remaining from the current time
 * to the next larger integral multiple of the update interval.
 * Setting a timer to this value schedules the next update
 * to occur on the next second or minute
 * when the given update interval is 1000 or 60000 milliseconds, respectively.
 */
static inline guint datetime_wake_interval(const GTimeVal current_time,
                                           const guint update_interval_ms)
{
  return update_interval_ms - (datetime_gtimeval_to_ms(current_time) %
                               update_interval_ms);
}

/*
 * Get date/time string
 */
gchar * datetime_do_utf8strftime(const char *format, const struct tm *tm)
{
  int len;
  gchar buf[DATETIME_MAX_STRLEN];
  gchar *utf8str = NULL;

  /* get formatted date/time */
  len = strftime(buf, sizeof(buf)-1, format, tm);
  if (len == 0)
    return g_strdup(_("Invalid format"));

  buf[len] = '\0';  /* make sure nul terminated string */
  utf8str = g_locale_to_utf8(buf, -1, NULL, NULL, NULL);
  if(utf8str == NULL)
    return g_strdup(_("Error"));

  return utf8str;
}

/**
 *  Check whether date/time format shows seconds
 */
static gboolean datetime_format_has_seconds(const gchar *format)
{
  static struct tm time_struct = {
    .tm_sec   = 0,
    .tm_min   = 0,
    .tm_hour  = 0,
    .tm_mday  = 1,
    .tm_mon   = 0,
    .tm_year  = 70, /* use 1970 so strftime() can convert '%s' */
    .tm_wday  = 0,
    .tm_yday  = 0,
    .tm_isdst = 0
  };
  int len1;
  int len2;
  gchar buf1[DATETIME_MAX_STRLEN];
  gchar buf2[DATETIME_MAX_STRLEN];

  if (format == NULL)
    return FALSE;

  time_struct.tm_sec = 1;
  len1 = strftime(buf1, sizeof(buf1)-1, format, &time_struct);
  if (len1 == 0)
    return FALSE;
  buf1[len1] = '\0';

  time_struct.tm_sec = 2;
  len2 = strftime(buf2, sizeof(buf2)-1, format, &time_struct);
  if (len2 == 0)
    return FALSE;
  buf2[len2] = '\0';

  return len1 != len2 || strcmp(buf1, buf2) != 0;
}

/*
 * set date and time labels
 */
gboolean datetime_update(t_datetime *datetime)
{
  GTimeVal timeval;
  gchar *utf8str;
  struct tm *current;
  guint wake_interval;  /* milliseconds to next update */

  DBG("wake");

  /* stop timer */
  if (datetime->timeout_id)
  {
    g_source_remove(datetime->timeout_id);
  }

  g_get_current_time(&timeval);
  current = localtime((time_t *)&timeval.tv_sec);

  if (datetime->layout != LAYOUT_TIME &&
      datetime->date_format != NULL && GTK_IS_LABEL(datetime->date_label))
  {
    utf8str = datetime_do_utf8strftime(datetime->date_format, current);
    gtk_label_set_text(GTK_LABEL(datetime->date_label), utf8str);
    g_free(utf8str);
  }

  if (datetime->layout != LAYOUT_DATE &&
      datetime->time_format != NULL && GTK_IS_LABEL(datetime->time_label))
  {
    utf8str = datetime_do_utf8strftime(datetime->time_format, current);
    gtk_label_set_text(GTK_LABEL(datetime->time_label), utf8str);
    g_free(utf8str);
  }

  /* Compute the time to the next update and start the timer. */
  wake_interval = datetime_wake_interval(timeval, datetime->update_interval);
  datetime->timeout_id = g_timeout_add(wake_interval, (GSourceFunc) datetime_update, datetime);

  return TRUE;
}

static gboolean datetime_tooltip_timer(t_datetime *datetime)
{
  DBG("wake");

  /* flag to datetime_query_tooltip that there is no longer an active timeout */
  datetime->tooltip_timeout_id = 0;

  /*
   * Run datetime_query_tooltip if the mouse is still there.
   * If it is run, it'll register *this* function to be called after another
   * timeout. If not, *this* function won't run again.
   */
  gtk_widget_trigger_tooltip_query(GTK_WIDGET(datetime->button));

  /* we don't want to automatically run again */
  return FALSE;
}

static gboolean datetime_query_tooltip(GtkWidget *widget,
                                       gint x, gint y,
                                       gboolean keyboard_mode,
                                       GtkTooltip *tooltip,
                                       t_datetime *datetime)
{
  GTimeVal timeval;
  struct tm *current;
  gchar *utf8str;
  gchar *format = NULL;
  guint wake_interval;  /* milliseconds to next update */

  switch(datetime->layout)
  {
    case LAYOUT_TIME:
      format = datetime->date_format;
      break;
    case LAYOUT_DATE:
      format = datetime->time_format;
      break;
    default:
      break;
  }

  if (format == NULL)
    return FALSE;

  g_get_current_time(&timeval);
  current = localtime((time_t *)&timeval.tv_sec);

  utf8str = datetime_do_utf8strftime(format, current);
  gtk_tooltip_set_text(tooltip, utf8str);
  g_free(utf8str);

  /* if there is no active timeout to update the tooltip, register one */
  if (!datetime->tooltip_timeout_id)
  {
    /*
     * I think we can afford to inefficiently poll every
     * second while the user keeps the mouse here.
     */
    wake_interval = datetime_wake_interval(timeval, 1000);
    datetime->tooltip_timeout_id = g_timeout_add(wake_interval,
      (GSourceFunc) datetime_tooltip_timer, datetime);
  }

  return TRUE;
}

static void on_calendar_realized(GtkWidget *widget, t_datetime *datetime)
{
  gint x, y;
  GtkWidget *parent;

  parent = g_object_get_data(G_OBJECT(widget), "calendar-parent");
  xfce_panel_plugin_position_widget(datetime->plugin, widget, parent, &x, &y);
  gtk_window_move(GTK_WINDOW(widget), x, y);
}

static gboolean close_calendar_window(t_datetime *datetime)
{
  gtk_widget_destroy(datetime->cal);
  datetime->cal = NULL;

  xfce_panel_plugin_block_autohide (XFCE_PANEL_PLUGIN (datetime->plugin), FALSE);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(datetime->button), FALSE);

  return TRUE;
}

/*
 * call the gtk calendar
 */
static GtkWidget * pop_calendar_window(t_datetime *datetime, int orientation)
{
  GtkWidget *window;
  GtkWidget *frame;
  GtkWidget *cal;
  GtkWidget *parent = datetime->button;
  GdkScreen *screen;
  GtkCalendarDisplayOptions display_options;
  int num;

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
  gtk_window_set_skip_taskbar_hint(GTK_WINDOW(window), TRUE);
  gtk_window_set_skip_pager_hint(GTK_WINDOW(window), TRUE);
  gtk_window_stick(GTK_WINDOW(window));
  g_object_set_data(G_OBJECT(window), "calendar-parent", parent);

  /* set screen number */
  screen = gtk_widget_get_screen(parent);
  num = gdk_screen_get_monitor_at_window(screen, parent->window);
  gtk_window_set_screen(GTK_WINDOW(window), screen);

  frame = gtk_frame_new(NULL);
  gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_OUT);
  gtk_container_add (GTK_CONTAINER(window), frame);

  cal = gtk_calendar_new();
  display_options = GTK_CALENDAR_SHOW_HEADING |
    GTK_CALENDAR_SHOW_WEEK_NUMBERS |
    GTK_CALENDAR_SHOW_DAY_NAMES;
  gtk_calendar_display_options(GTK_CALENDAR (cal), display_options);
  gtk_container_add (GTK_CONTAINER(frame), cal);

  g_signal_connect_after(G_OBJECT(window), "realize",
      G_CALLBACK(on_calendar_realized),
      datetime);
  g_signal_connect_swapped(G_OBJECT(window), "delete-event",
      G_CALLBACK(close_calendar_window),
      datetime);
  g_signal_connect_swapped(G_OBJECT(window), "focus-out-event",
      G_CALLBACK(close_calendar_window),
      datetime);
  gtk_widget_show_all(window);

  xfce_panel_plugin_block_autohide (XFCE_PANEL_PLUGIN (datetime->plugin), TRUE);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(datetime->button), TRUE);

  return window;
}

/*
 * when the dialog is clicked, show the calendar
 */
static gboolean datetime_clicked(GtkWidget *widget,
    GdkEventButton *event,
    t_datetime *datetime)
{
  gint orientation;

  if (event->button != 1 || event->state & GDK_CONTROL_MASK)
    return FALSE;

  if (datetime == NULL)
    return FALSE;

  if (datetime->cal != NULL)
  {
    close_calendar_window(datetime);
  }
  else
  {
    /* get orientation before drawing the calendar */
    orientation = xfce_panel_plugin_get_orientation(datetime->plugin);

    /* draw calendar */
    datetime->cal = pop_calendar_window(datetime,
                                        orientation);
  }
  return TRUE;
}

static void datetime_update_date_font(t_datetime *datetime)
{
  PangoFontDescription *font;
  font = pango_font_description_from_string(datetime->date_font);

  if (G_LIKELY (font))
  {
    gtk_widget_modify_font(datetime->date_label, font);
    pango_font_description_free (font);
  }
}

static void datetime_update_time_font(t_datetime *datetime)
{
  PangoFontDescription *font;
  font = pango_font_description_from_string(datetime->time_font);

  if (G_LIKELY (font))
  {
    gtk_widget_modify_font(datetime->time_label, font);
    pango_font_description_free (font);
  }
}

static void datetime_set_update_interval(t_datetime *datetime)
{
  /* a custom date format could specify seconds */
  gboolean date_has_seconds = datetime_format_has_seconds(datetime->date_format);
  gboolean time_has_seconds = datetime_format_has_seconds(datetime->time_format);
  gboolean has_seconds;

  /* set update interval for the date/time displayed in the panel */
  switch(datetime->layout)
  {
    case LAYOUT_DATE:
      has_seconds = date_has_seconds;
      break;
    case LAYOUT_TIME:
      has_seconds = time_has_seconds;
      break;
    default:
      has_seconds = date_has_seconds || time_has_seconds;
      break;
  }

  /* 1000 ms in 1 second */
  datetime->update_interval = 1000 * (has_seconds ? 1 : 60);
}

/*
 * set layout after doing some checks
 */
void datetime_apply_layout(t_datetime *datetime, t_layout layout)
{
  if(layout < LAYOUT_COUNT)
  {
    datetime->layout = layout;
  }

  /* hide labels based on layout-selection */
  gtk_widget_show(GTK_WIDGET(datetime->time_label));
  gtk_widget_show(GTK_WIDGET(datetime->date_label));
  switch(datetime->layout)
  {
    case LAYOUT_DATE:
      gtk_widget_hide(GTK_WIDGET(datetime->time_label));
      break;
    case LAYOUT_TIME:
      gtk_widget_hide(GTK_WIDGET(datetime->date_label));
      break;
    default:
      break;
  }

  /* update tooltip handler */
  if (datetime->tooltip_handler_id)
  {
    g_signal_handler_disconnect(datetime->button,
                                datetime->tooltip_handler_id);
    datetime->tooltip_handler_id = 0;
  }
  switch(datetime->layout)
  {
    case LAYOUT_DATE:
    case LAYOUT_TIME:
      gtk_widget_set_has_tooltip(GTK_WIDGET(datetime->button), TRUE);
      datetime->tooltip_handler_id = g_signal_connect(datetime->button,
                             "query-tooltip",
                             G_CALLBACK(datetime_query_tooltip), datetime);
      break;

    default:
      gtk_widget_set_has_tooltip(GTK_WIDGET(datetime->button), FALSE);
  }

  /* set labels to occupy the same size or not */
  gtk_box_set_homogeneous(GTK_BOX(datetime->box), !datetime->side_by_side);

  /* set order based on layout-selection */
  switch(datetime->layout)
  {
    case LAYOUT_TIME_DATE:
      gtk_box_reorder_child(GTK_BOX(datetime->box), datetime->time_label, 0);
      gtk_box_reorder_child(GTK_BOX(datetime->box), datetime->date_label, 1);
      break;

    default:
      gtk_box_reorder_child(GTK_BOX(datetime->box), datetime->time_label, 1);
      gtk_box_reorder_child(GTK_BOX(datetime->box), datetime->date_label, 0);
  }

  datetime_set_update_interval(datetime);
}

/*
 * set the date and time font type
 */
void datetime_apply_font(t_datetime *datetime,
    const gchar *date_font_name,
    const gchar *time_font_name)
{
  if (date_font_name != NULL)
  {
    g_free(datetime->date_font);
    datetime->date_font = g_strdup(date_font_name);
    datetime_update_date_font(datetime);
  }

  if (time_font_name != NULL)
  {
    g_free(datetime->time_font);
    datetime->time_font = g_strdup(time_font_name);
    datetime_update_time_font(datetime);
  }
}

/*
 * set the date and time format
 */
void datetime_apply_format(t_datetime *datetime,
    const gchar *date_format,
    const gchar *time_format)
{
  if (datetime == NULL)
    return;

  if (date_format != NULL)
  {
    g_free(datetime->date_format);
    datetime->date_format = g_strdup(date_format);
  }

  if (time_format != NULL)
  {
    g_free(datetime->time_format);
    datetime->time_format = g_strdup(time_format);
  }

  datetime_set_update_interval(datetime);
}

/*
 * set the date and time foreground color
 */
void datetime_apply_color(t_datetime *datetime,
    const gchar *date_color,
    const gchar *time_color)
{
  GdkColor color;

  if (date_color != NULL)
  {
    g_free(datetime->date_color);
    datetime->date_color = g_strdup(date_color);

    gdk_color_parse (datetime->date_color, &color);
    gtk_widget_modify_fg (datetime->date_label, GTK_STATE_NORMAL, &color);
  }

  if (time_color != NULL)
  {
    g_free(datetime->time_color);
    datetime->time_color = g_strdup(time_color);

    gdk_color_parse (datetime->time_color, &color);
    gtk_widget_modify_fg (datetime->time_label, GTK_STATE_NORMAL, &color);
  }
}

/*
 * set the layout according to side-by-side value
 */
void datetime_apply_side_by_side(t_datetime *datetime,
    const gboolean side_by_side,
    const gboolean update_layout)
{
  GtkOrientation orientation;

  datetime->side_by_side = side_by_side;

  /* update orientation */
  orientation = xfce_panel_plugin_get_orientation(datetime->plugin);

#ifdef HAS_PANEL_49
  datetime_set_mode(datetime->plugin, orientation, datetime);
#else
  datetime_set_orientation(datetime->plugin, orientation, datetime);
#endif

  if(update_layout)
    datetime_apply_layout(datetime, datetime->layout);
}

/*
 * Function only called by the signal handler.
 */
static int datetime_set_size(XfcePanelPlugin *plugin,
    gint size,
    t_datetime *datetime)
{
  /* return true to please the signal handler ;) */
  return TRUE;
}

/*
 * Read the settings from the config file
 */
static void datetime_read_rc_file(XfcePanelPlugin *plugin, t_datetime *dt)
{
  gchar *file;
  XfceRc *rc = NULL;
  t_layout layout;
  const gchar *date_font, *time_font, *date_format, *time_format, *date_color, *time_color;
  const gboolean *side_by_side;

  /* load defaults */
  layout = LAYOUT_DATE_TIME;
  date_font = "Bitstream Vera Sans 8";
  time_font = "Bitstream Vera Sans 8";
  date_format = "%Y-%m-%d";
  time_format = "%H:%M";
  date_color = "#000000000000";
  time_color = "#000000000000";
  side_by_side = FALSE;

  /* open file */
  if((file = xfce_panel_plugin_lookup_rc_file(plugin)) != NULL)
  {
    rc = xfce_rc_simple_open(file, TRUE);
    g_free(file);

    if(rc != NULL)
    {
      layout       = xfce_rc_read_int_entry(rc, "layout", layout);
      date_font    = xfce_rc_read_entry(rc, "date_font", date_font);
      time_font    = xfce_rc_read_entry(rc, "time_font", time_font);
      date_format  = xfce_rc_read_entry(rc, "date_format", date_format);
      time_format  = xfce_rc_read_entry(rc, "time_format", time_format);
      date_color   = xfce_rc_read_entry(rc, "date_color", date_color);
      time_color   = xfce_rc_read_entry(rc, "time_color", time_color);
      side_by_side = xfce_rc_read_bool_entry(rc, "side_by_side", side_by_side);
    }
  }

  date_font   = g_strdup(date_font);
  time_font   = g_strdup(time_font);
  date_format = g_strdup(date_format);
  time_format = g_strdup(time_format);
  date_color  = g_strdup(date_color);
  time_color  = g_strdup(time_color);

  if(rc != NULL)
    xfce_rc_close(rc);

  /* set values in dt struct */
  datetime_apply_side_by_side(dt, side_by_side, FALSE);
  datetime_apply_layout(dt, layout);
  datetime_apply_font(dt, date_font, time_font);
  datetime_apply_format(dt, date_format, time_format);
  datetime_apply_color(dt, date_color, time_color);
}

/*
 * write the settings to the config file
 */
void datetime_write_rc_file(XfcePanelPlugin *plugin, t_datetime *dt)
{
  char *file;
  XfceRc *rc;

  if(!(file = xfce_panel_plugin_save_location(plugin, TRUE)))
    return;

  rc = xfce_rc_simple_open(file, FALSE);
  g_free(file);

  if(rc != NULL)
  {
    xfce_rc_write_int_entry(rc, "layout", dt->layout);
    xfce_rc_write_entry(rc, "date_font", dt->date_font);
    xfce_rc_write_entry(rc, "time_font", dt->time_font);
    xfce_rc_write_entry(rc, "date_format", dt->date_format);
    xfce_rc_write_entry(rc, "time_format", dt->time_format);
    xfce_rc_write_entry(rc, "date_color", dt->date_color);
    xfce_rc_write_entry(rc, "time_color", dt->time_color);
    xfce_rc_write_bool_entry(rc, "side_by_side", dt->side_by_side);

    xfce_rc_close(rc);
  }

}

/*
 * change widgets orientation when the panel orientation changes
 */
#ifdef HAS_PANEL_49
static void datetime_set_mode(XfcePanelPlugin *plugin, XfcePanelPluginMode mode, t_datetime *datetime)
{
  GtkOrientation panel_orientation = xfce_panel_plugin_get_orientation (plugin);
  GtkOrientation orientation = (mode == XFCE_PANEL_PLUGIN_MODE_VERTICAL) ?
    GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL;
#else
static void datetime_set_orientation(XfcePanelPlugin *plugin, GtkOrientation orientation, t_datetime *datetime)
{
#endif

  gboolean side_by_side = TRUE;

  if (orientation == GTK_ORIENTATION_VERTICAL)
  {
    xfce_hvbox_set_orientation(XFCE_HVBOX(datetime->box), GTK_ORIENTATION_HORIZONTAL);
    gtk_label_set_angle(GTK_LABEL(datetime->time_label), -90);
    gtk_label_set_angle(GTK_LABEL(datetime->date_label), -90);
    gtk_box_reorder_child(GTK_BOX(datetime->box), datetime->time_label, 0);
    gtk_box_reorder_child(GTK_BOX(datetime->box), datetime->date_label, 1);
  }
  else
  {
    xfce_hvbox_set_orientation(XFCE_HVBOX(datetime->box), datetime->side_by_side ?
      GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL);
    gtk_label_set_angle(GTK_LABEL(datetime->time_label), 0);
    gtk_label_set_angle(GTK_LABEL(datetime->date_label), 0);
    gtk_box_reorder_child(GTK_BOX(datetime->box), datetime->date_label, 0);
    gtk_box_reorder_child(GTK_BOX(datetime->box), datetime->time_label, 1);
  }
}

/*
 * create the gtk-part of the datetime plugin
 */
static void datetime_create_widget(t_datetime * datetime)
{
  GtkOrientation orientation;
  orientation = xfce_panel_plugin_get_orientation(datetime->plugin);

  /* create button */
  datetime->button = xfce_create_panel_toggle_button();
  gtk_widget_show(datetime->button);

  /* create a box which can be easily adapted to the panel orientation */
  datetime->box = xfce_hvbox_new(GTK_ORIENTATION_VERTICAL, TRUE, 0);

  gtk_widget_show(datetime->box);
  gtk_container_add(GTK_CONTAINER(datetime->button), datetime->box);

  /* create time and date lines */
  datetime->time_label = gtk_label_new("");
  datetime->date_label = gtk_label_new("");
  gtk_label_set_justify(GTK_LABEL(datetime->time_label), GTK_JUSTIFY_CENTER);
  gtk_label_set_justify(GTK_LABEL(datetime->date_label), GTK_JUSTIFY_CENTER);

  /* add time and date lines to the box */
  gtk_box_pack_start(GTK_BOX(datetime->box),
      datetime->time_label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(datetime->box),
      datetime->date_label, FALSE, FALSE, 0);

  /* connect widget signals to functions */
  g_signal_connect(datetime->button, "button-press-event",
      G_CALLBACK(datetime_clicked), datetime);

  /* set orientation according to the panel orientation */
#ifdef HAS_PANEL_49
  datetime_set_mode(datetime->plugin, orientation, datetime);
#else
  datetime_set_orientation(datetime->plugin, orientation, datetime);
#endif
}

/*
 * create datetime plugin
 */
static t_datetime * datetime_new(XfcePanelPlugin *plugin)
{
  t_datetime * datetime;

  DBG("Starting datetime panel plugin");

  /* alloc and clear mem */
  datetime = panel_slice_new0 (t_datetime);

  /* store plugin reference */
  datetime->plugin = plugin;

  /* call widget-create function */
  datetime_create_widget(datetime);

  /* load settings (default values if non-av) */
  datetime_read_rc_file(plugin, datetime);

  /* set date and time labels */
  datetime_update(datetime);

  return datetime;
}

/*
 * frees the datetime struct
 */
static void datetime_free(XfcePanelPlugin *plugin, t_datetime *datetime)
{
  /* stop timeouts */
  if (datetime->timeout_id != 0)
    g_source_remove(datetime->timeout_id);
#if USE_GTK_TOOLTIP_API
  if (datetime->tooltip_timeout_id != 0)
    g_source_remove(datetime->tooltip_timeout_id);
#endif

  /* destroy widget */
  gtk_widget_destroy(datetime->button);

  /* cleanup */
  g_free(datetime->date_font);
  g_free(datetime->time_font);
  g_free(datetime->date_format);
  g_free(datetime->time_format);

  panel_slice_free(t_datetime, datetime);
}

/*
 * Construct the plugin
 */
static void datetime_construct(XfcePanelPlugin *plugin)
{
  /* create datetime plugin */
  t_datetime * datetime = datetime_new(plugin);

  /* add plugin to panel */
  gtk_container_add(GTK_CONTAINER(plugin), datetime->button);
  xfce_panel_plugin_add_action_widget(plugin, datetime->button);

  /* connect plugin signals to functions */
  g_signal_connect(plugin, "save",
      G_CALLBACK(datetime_write_rc_file), datetime);
  g_signal_connect(plugin, "free-data",
      G_CALLBACK(datetime_free), datetime);
  g_signal_connect(plugin, "size-changed",
      G_CALLBACK(datetime_set_size), datetime);
  g_signal_connect(plugin, "configure-plugin",
      G_CALLBACK(datetime_properties_dialog), datetime);
#ifdef HAS_PANEL_49
  g_signal_connect(plugin, "mode-changed", G_CALLBACK(datetime_set_mode), datetime);
#else
  g_signal_connect(plugin, "orientation-changed", G_CALLBACK(datetime_set_orientation), datetime);
#endif
  xfce_panel_plugin_menu_show_configure(plugin);
}


XFCE_PANEL_PLUGIN_REGISTER(datetime_construct);

