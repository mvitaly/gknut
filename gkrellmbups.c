/*      __       __
 *   __/ /_______\ \__     ___ ___ __ _                       _ __ ___ ___
 *__/ / /  .---.  \ \ \___/                                               \___
 *_/ | '  /  / /\  ` | \_/          (C) Copyright 2001, Chris Page         \__
 * \ | |  | / / |  | | / \  Released under the GNU General Public License  /
 *  >| .  \/ /  /  . |<   >--- --- -- -                       - -- --- ---<
 * / \_ \  `/__'  / _/ \ /  This program is free software released under   \
 * \ \__ \_______/ __/ / \   the GNU GPL. Please see the COPYING file in   /
 *  \  \_         _/  /   \   the distribution archive for more details   /
 * //\ \__  ___  __/ /\\ //\                                             /
 *- --\  /_/   \_\  /-- - --\                                           /-----
 *-----\_/       \_/---------\   ___________________________________   /------
 *                            \_/                                   \_/
 */
/** 
 *  \file gkrellmbups.c
 *  This file contains the functions and structures required to create, display
 *  and manage the plugin. The client thread implemented in ups_connect.c handles
 *  talking to the UPS, this code handles talking to GKrellM and GTK/GDK.
 *
 *  $Author: chris $
 *  $Revision: 1.7 $
 */
/* $Id: gkrellmbups.c,v 1.7 2002/01/05 13:48:21 chris Exp $
 *
 * $Log: gkrellmbups.c,v $
 * Revision 1.7  2002/01/05 13:48:21  chris
 * Added code to support non-UK UPS MAINS_MIN settings.
 *
 * Revision 1.6  2001/12/28 13:49:06  chris
 * Fixed up for release
 *
 * Revision 1.5  2001/12/28 13:22:38  chris
 * Added user control over the mains offset value.
 *
 * Revision 1.4  2001/12/26 16:20:35  chris
 * Tidied up the commenting and finished the pruning of the variables
 * I optimised away in the previous step.
 *
 * Revision 1.3  2001/12/25 20:29:29  chris
 * Some optimisations to the text format handling (moved into the chart
 * structure rather than being stuck in the global config) and added the
 * remaining plugin functionality.
 *
 * Revision 1.2  2001/12/23 15:39:52  chris
 * On-chart formatted display complete.
 *
 * Revision 1.1  2001/12/23 13:05:32  chris
 * Basic plugin functionality implemented - no save/lod/edit config or
 * other more useful facilities yet. They come next...
 *
 */

#include<gkrellm/gkrellm.h>
#include"gkrellmbups.h"
#include"ups_connect.h"

/*! Current plugin version number */
#define GKRELLMBUPS_VERSION  "0.3"

static GKrellMBUPS *bupsData; /*!< Dynamically allocated central data store.           */  
static BUPSConfig  *config;   /*!< Configuration options                               */
static Monitor     *mon;      /*!< Pointer to the statically allocated monitor.        */ 
static gint         style_id; /*!< Plugin style alowing custom themes for this plugin. */

static GtkWidget   *voltCombo;   /*!< voltage format string combo box.     */
static GtkWidget   *freqCombo;   /*!< frequency format string combo box.   */
static GtkWidget   *tempCombo;   /*!< temperature format string combo box. */
static GtkWidget   *mainsWidget; /*!< Mains voltage compensation.          */ 
static GtkWidget   *hostWidget;  /*!< Hostname string box.                 */
static GtkWidget   *portWidget;  /*!< Service port spin button.            */ 

/*! Descriptive text shown in the Help tab of the plugin configuration. */ 
static gchar *helpText[] = 
{
    "GKrellMBUPS displays the status of a Belkin UPS. Note that you must install Belikn's\n",
    "\"Sentry Bulldog\" software and have the Belkin uspd service running before this\n",
    "plugin will work correctly. This software ",
    "<b>is not ",
    "supplied with this plugin!\n",
    "\n",
    "<b>\"Mains\" setting\n",
    "Mains voltages are too high to show on a reasonable size chart while retaining any \n",
    "detail. This option allows you to set the value to subtract from the mains voltage \n",
    "before showing it on the chart - this makes changes in voltage far more visible. It \n",
    "should be set to approximately the voltage at which your UPS switches to battery \n",
    "supply (around 187v on a UK UPS).\n",
    "<b>Voltage chart:\n",
    "Substitution variables for the format string for chart labels:\n",
    "\t$i\tInput voltage level (in volts)\n", 
    "\t$o\tOutput voltage level (in volts)\n", 
    "\t$b\tBattery voltage level (in volts)\n", 
    "\t$l\tBattery level\n", 
    "\n",
    "<b>Frequency chart:\n",
    "Substitution variables for the format string for chart labels:\n",
    "\t$i\tInput frequency (in hertz)\n", 
    "\t$o\tOutput frequency (in hertz)\n", 
    "\n",
    "<b>Stats chart:\n",
    "Substitution variables for the format string for chart labels:\n",
    "\t$t\tUPS Temperature (in centigrade)\n", 
    "\t$l\tLoad level (as a percentage of maximum)\n", 
    "\n",
    "Left click on charts to toggle the text overlay. Middle click on the UPS panel to\n",
    "toggle a scrolling display of log messages from the UPS."
};

/*! Plugin ownership and version information show in the About table of the plugin configuration. */
static gchar aboutText[] = 
"GKrellMBUPS " GKRELLMBUPS_VERSION "\n" \
"GKrellM Belkin UPS Monitor plugin\n\n\n" \
"Copyright (C) 2001 Chris Page\n" \
"Chris <chris@starforge.co.uk>\n" \
"http://www.starforge.co.uk/gkrellm/\n\n" \
"Released under the GPL.\n";

/*! chart condig names for the voltage chart data entries     */
static gchar *voltNames[] = { "Input voltage", "Output voltage", "Battery voltage", NULL };

/*! chart config names for the frequency chart data entries   */
static gchar *freqNames[] = { "Input frequency", "Output frequency", NULL };

/*! chart config names for the temperature chart data entries */
static gchar *tempNames[] = { "Temperature", "Load", NULL };

/** Voltage chart text formatter.
 *  This replaces special "$" codes in the specified format sttring with
 *  values taken from upsStatus. Please see the switch in the body of the
 *  function for details about which codes are recognised. All unrecognised
 *  codes or other characters are simply copied to the buffer.
 *
 *  \par Arguments:
 *  \arg \c buffer - Destination buffer.
 *  \arg \c size - number of characters available in buffer (not including newline).
 *  \arg \c format - Format string to process into buffer.
 *
 *  \todo Find a way to unify formatVoltText(), formatFreqText() and 
 *  formatTempText() to reduce the amount of replication between them (maybe
 *  use varargs and pass the values to replace code with in that way?)
 */
static void formatVoltText(gchar *buffer, gint size, gchar *format)
{
    gchar *fpos;
    gchar  opt;
    gint   len;

    size--;
    *buffer = '\0';

    for(fpos = format; (*fpos != '\0') && (size != 0); fpos ++) {
        len = 1;
        if((*fpos == '$') && (*(fpos + 1) != '\0')) {
            opt = *(fpos + 1);
            switch(opt) {
                /* $i - input voltage (in volts) */
                case 'i': len = snprintf(buffer, size, "%3.1f", upsStatus.in_Voltage ); fpos ++; break;
                /* $o - output voltage (in volts) */
                case 'o': len = snprintf(buffer, size, "%3.1f", upsStatus.out_Voltage); fpos ++; break;
                /* $b - batteryvoltage (in volts) */
                case 'b': len = snprintf(buffer, size, "%3.1f", upsStatus.bat_Voltage); fpos ++; break;
                /* $l - battery level as a percentage. */
                case 'l': len = snprintf(buffer, size, "%3.1f", upsStatus.bat_Level  ); fpos ++; break;
                default: *buffer = *fpos; break;
            }
        } else {
            *buffer = *fpos;
        }

        size -= len;
        buffer += len;
    }
    *buffer = '\0';
}

/** Frequency chart text formatter.
 *  This does much the same thing as formatVoltText() except for the frequency chart.
 *
 *  \par Arguments:
 *  \arg \c buffer - Destination buffer.
 *  \arg \c size - number of characters available in buffer (not including newline).
 *  \arg \c format - Format string to process into buffer.
 */
static void formatFreqText(gchar *buffer, gint size, gchar *format)
{
    gchar *fpos;
    gchar  opt;
    gint   len;

    size--;
    *buffer = '\0';

    for(fpos = format; (*fpos != '\0') && (size != 0); fpos ++) {
        len = 1;
        if((*fpos == '$') && (*(fpos + 1) != '\0')) {
            opt = *(fpos + 1);
            switch(opt) {
                case 'i': len = snprintf(buffer, size, "%2.1f", upsStatus.in_Freq ); fpos ++; break;
                case 'o': len = snprintf(buffer, size, "%2.1f", upsStatus.out_Freq); fpos ++; break;
                default: *buffer = *fpos; break;
            }
        } else {
            *buffer = *fpos;
        }

        size -= len;
        buffer += len;
    }
    *buffer = '\0';
}

/** Frequency chart text formatter.
 *  This does much the same thing as formatVoltText() except for the 
 *  temperature chart.
 *
 *  \par Arguments:
 *  \arg \c buffer - Destination buffer.
 *  \arg \c size - number of characters available in buffer (not including newline).
 *  \arg \c format - Format string to process into buffer.
 */
static void formatTempText(gchar *buffer, gint size, gchar *format)
{
    gchar *fpos;
    gchar  opt;
    gint   len;

    size--;
    *buffer = '\0';

    for(fpos = format; (*fpos != '\0') && (size != 0); fpos ++) {
        len = 1;
        if((*fpos == '$') && (*(fpos + 1) != '\0')) {
            opt = *(fpos + 1);
            switch(opt) {
                case 't': len = snprintf(buffer, size, "%2.1f", upsStatus.ups_Temp); fpos ++; break;
                case 'l': len = snprintf(buffer, size, "%3.1f", upsStatus.ups_Load); fpos ++; break;
                default: *buffer = *fpos; break;
            }
        } else {
            *buffer = *fpos;
        }

        size -= len;
        buffer += len;
    }
    *buffer = '\0';
}

/** Draw the chart data and, optionally, text overlay. 
 *  As the user can opt to have a text over on the charts, this function
 *  is required to handle the drawing. 
 */  
static void drawChart(BUPSChart *chart)
{
    gchar     buf[128];
    buf[0] = '\0';

	gkrellm_draw_chartdata(chart -> chart);
    if(chart -> showText) {
        chart -> format(buf, sizeof(buf), chart -> textFormat);
        gkrellm_draw_chart_text(chart -> chart, style_id, buf);
    }
	gkrellm_draw_chart_to_screen(chart -> chart);
}

/** Draw the log panel, either drawing a static label or a scrolling log.
 *  This function handles the drawing of the ups "log message" panel, either
 *  showing a static "UPS" label or scrolling the last log message from
 *  the UPS service.
 */
static void drawLog(void)
{
    gint width;

    if(config -> showLog) {
        /* This next bit is taken from gkrellweather - I much prefer this scrolling 
         * setup to the more jumpy version used in some of the other panels
         */
        width = gkrellm_chart_width();
        bupsData -> logScr = (bupsData -> logScr + 1) % (2 * width);
        bupsData -> logDecal -> x_off = width - bupsData -> logScr;
        if(bupsData -> logText) {
            gkrellm_draw_decal_text(bupsData -> logDisplay, bupsData -> logDecal, bupsData -> logText, width - bupsData -> logScr);
        } else {
            if(upsStatus.ups_Present) {
                gkrellm_draw_decal_text(bupsData -> logDisplay, bupsData -> logDecal, "No log messsage waiting.", width - bupsData -> logScr);
            } else {
                gkrellm_draw_decal_text(bupsData -> logDisplay, bupsData -> logDecal, "No UPS detected!", width - bupsData -> logScr);
            }
        }
    } else {
        bupsData -> labelDecal -> x_off = bupsData -> labelX;
        gkrellm_draw_decal_text(bupsData -> logDisplay, bupsData -> labelDecal, bupsData -> logLabel, -1);
    }
}

/** Callback for handling chart ExposeEvent events.
 *  This will draw the backing pixmap for the relevant part of a panel
 *  or chart - note that unlike the examples (and indeed almost every
 *  other plugin I've looked at) this uses the event userdata pointer
 *  to optimise the pixmap identification process.
 */
static gint chartExposeEvent(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
    BUPSChart *chart  = (BUPSChart *)data;
    GdkPixmap *pixmap = NULL;

    /* find out exactly what has been exposed...*/
    if(widget == chart -> chart -> drawing_area) {
        pixmap = chart -> chart -> pixmap;
    } else if(widget == chart -> panel -> drawing_area) {
        pixmap = chart -> panel -> pixmap;
    }

    /* If we are responsible redraw it! */
    if(pixmap) {
        gdk_draw_pixmap(widget -> window,
                        widget -> style -> fg_gc[GTK_WIDGET_STATE(widget)],
                        pixmap, 
                        event -> area.x, event -> area.y, 
                        event -> area.x, event -> area.y,
                        event -> area.width, event -> area.height);
    }
	return FALSE;
}

/** Callback for handling ExposeEvent events sent to the log panel.
 *  Couldn't get much easier than this - just a simple pixmap copy! :)
 */
static gint exposeLogEvent(GtkWidget *widget, GdkEventExpose *event)
{
     gdk_draw_pixmap(widget -> window,
                     widget -> style -> fg_gc[GTK_WIDGET_STATE(widget)],
                     bupsData -> logDisplay -> pixmap, 
                     event -> area.x, event -> area.y, 
                     event -> area.x, event -> area.y,
                     event -> area.width, event -> area.height);
	return FALSE;
}

/** Callback for handling button events sent to the charts.
 *  Pressing the right mouse button, or double-left-clicking will open the 
 *  chartcofig window for the chart the user has selected. Single-left 
 *  clicking toggles the chart text overlay function.
 */
static void cbChartClick(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
    BUPSChart *target = (BUPSChart *)data;

	if((event -> button == 3) || (event -> button == 1 && event -> type == GDK_2BUTTON_PRESS)) {
		gkrellm_chartconfig_window_create(target -> chart);
	} else if((event -> button == 1) && (event -> type == GDK_BUTTON_PRESS)) {
        target -> showText = !target -> showText;
        gkrellm_config_modified();
        drawChart(target);
    }
}

/** Callback for handling button events sent to the log panel.
 *  A single right click will open the plugin configuration while lief-clicking
 *  toggles between the label and scrolling log displays.
 */
static void cbLogClick(GtkWidget *widget, GdkEventButton *event)
{
	if(event -> button == 3) {
		gkrellm_open_config_window(mon);
	} else if(event -> button == 2) {
        if(config -> showLog) {
            gkrellm_make_decal_invisible(bupsData -> logDisplay, bupsData -> logDecal);
            config -> showLog = FALSE;
            drawLog();
            gkrellm_make_decal_visible(bupsData -> logDisplay, bupsData -> labelDecal);
        } else {
            gkrellm_make_decal_invisible(bupsData -> logDisplay, bupsData -> labelDecal);
            config -> showLog = TRUE;
            drawLog();
            gkrellm_make_decal_visible(bupsData -> logDisplay, bupsData -> logDecal);
        }
		gkrellm_config_modified();
    }    
}

/** Add latest chart values and check for log updates.
 *  Called fairly regularly, but this only does anythignn really interesting once
 *  a second - it locks the mutex on upsStatus and updates all three charts to
 *  the latest values from the client thread. Once done the log string is 
 *  checked and possibly duplicated. 
 */ 
static void updatePlugin(void)
{
    gint vala, valb, valc;

    if(GK.second_tick) {
        pthread_mutex_lock(&upsStatus_lock); /* best to do this even though we aren't writing */
        vala = LIM_FLOOR((gint)upsStatus.in_Voltage - config -> mains, 0);
        valb = LIM_FLOOR((gint)upsStatus.out_Voltage - config -> mains, 0);
        valc = LIM_FLOOR((gint)upsStatus.bat_Voltage, 0);
        gkrellm_store_chartdata(bupsData -> voltChart.chart, 0, vala, valb, valc);
        drawChart(&bupsData -> voltChart);

        vala = LIM_FLOOR((gint)upsStatus.in_Freq, 0);
        valb = LIM_FLOOR((gint)upsStatus.out_Freq, 0);
        gkrellm_store_chartdata(bupsData -> freqChart.chart, 0, vala, valb);
        drawChart(&bupsData -> freqChart);

        vala = LIM_FLOOR((gint)upsStatus.ups_Temp, 0);
        valb = LIM_FLOOR((gint)upsStatus.ups_Load, 0);
        gkrellm_store_chartdata(bupsData -> tempChart.chart, 0, vala, valb);
        drawChart(&bupsData -> tempChart);

        /* this bit MUST be inside a mutex on upsStatus or heaven knows what will happen when the 
         * thread updates ups_LastLog half way through the strdup ... 
         */
        vala = strlen(upsStatus.ups_LastLog);
        if(vala) {
            gkrellm_dup_string(&bupsData -> logText, upsStatus.ups_LastLog);
        }
        pthread_mutex_unlock(&upsStatus_lock);
    }
    drawLog();
    gkrellm_draw_panel_layers(bupsData -> logDisplay);
}

/** Create a new BUPSData chart.
 *  Simple enough to describe - this creates charts. What it actually does is more
 *  complicated, but that is best highlighted via th arguments:
 *
 *  \par Arguments:
 *  \arg \c vbox - the box into which a vbox containing a chart and panel should be added.
 *  \arg \c data - the BUPSData structure to fill in.
 *  \arg \c firstCreate - TRUE when this is the first tiem this has been called.
 *  \arg \c dataNames - array of names, one for each chartdata element (last element should be NULL).
 *  \arg \c name - Text to display as a label in the panel below the chart.
 *  \arg \c format - pointer to the function which formats this charts text overlay.
 */
static void createChart(GtkWidget *vbox, BUPSChart *data, gint firstCreate, gchar *dataNames[], gchar *name, void (*format)(gchar *, gint, gchar*))
{
    int count = 0;

    if(firstCreate) {
        /* Create a vbox into whcih the chart and panel can be added */
        data -> vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_add(GTK_CONTAINER(vbox), data -> vbox);
        gtk_widget_show(data -> vbox);

        /* Chart and panel creation... */
		data -> chart = gkrellm_chart_new0();
        data -> panel = data -> chart -> panel = gkrellm_panel_new0();
        data -> format = format;
    }

    gkrellm_set_chart_height_default(data -> chart, DEFAULT_CHARTHEIGHT);
    gkrellm_chart_create(data -> vbox, mon, data -> chart, &data -> config);

    while((count < MAX_DATA) && dataNames[count]) {
        data -> data[count] = gkrellm_add_default_chartdata(data -> chart, dataNames[count]);
        gkrellm_monotonic_chartdata(data -> data[count], FALSE);
        gkrellm_set_chartdata_draw_style_default(data -> data[count], CHARTDATA_LINE);
        gkrellm_set_chartdata_flags(data -> data[count], CHARTDATA_ALLOW_HIDE);
        count ++;
    }

	/* Set your own chart draw function if you have extra info to draw */
	gkrellm_set_draw_chart_function(data -> chart, drawChart, data);

	/* If this next call is made, then there will be a resolution spin
	 *  button on the chartconfig window so the user can change resolutions.
	 */
	gkrellm_chartconfig_grid_resolution_adjustment(data -> config, 
                                                   TRUE,
                                                   0, 
                                                   (gfloat) MIN_GRID_RES, 
                                                   (gfloat) MAX_GRID_RES, 
                                                   0, 0, 0, 70);
	gkrellm_chartconfig_grid_resolution_label(data -> config, "Units drawn on the chart");

    gkrellm_panel_configure(data -> panel, name, gkrellm_panel_style(style_id));
    gkrellm_panel_create(data -> vbox, mon, data -> panel);

	gkrellm_alloc_chartdata(data -> chart);

    if(firstCreate) {
        /* callbacks to redraw the widgets. As far as I can tell, most gkrellm plugins
         * (and certainly the built-in meters) ignore the user data for these: here we
         * can drmatically simplify the buton click and expose event handlers by passing
         * the BUPSData structure as the userdata.
         */
        gtk_signal_connect(GTK_OBJECT(data -> chart -> drawing_area),
                           "expose_event", 
                           (GtkSignalFunc)chartExposeEvent, data);
        gtk_signal_connect(GTK_OBJECT(data -> panel -> drawing_area),
                           "expose_event", 
                           (GtkSignalFunc)chartExposeEvent, data);
        /* callback to handle mouse clicks on the chart */
		gtk_signal_connect(GTK_OBJECT(data -> chart -> drawing_area),
                           "button_press_event", 
                           (GtkSignalFunc)cbChartClick, data);
    } else {
		drawChart(data);
	}
}

/** Create the plugin charts and panels. 
 *  Much of the actual work for this is done by the createChart() function, only 
 *  the log display panel is actually created in teh body of this function - 
 *  createchart is called three times; once for the voltage display, once for 
 *  the frequency and once for the temperature. With a bit of fiddling it may
 *  be possible to make charts optional, but that's one for a future version..
 */
static void createPlugin(GtkWidget *vbox, gint firstCreate)
{
    gint labelWidth;

    if(firstCreate) {
        bupsData -> vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_add(GTK_CONTAINER(vbox), bupsData -> vbox);
        gtk_widget_show(bupsData -> vbox);

        bupsData -> logDisplay = gkrellm_panel_new0();
        bupsData -> logLabel   = "UPS";
        bupsData -> client     = launchClient(config -> host, config -> port);
    }
    
    createChart(bupsData -> vbox, &bupsData -> voltChart, firstCreate, voltNames, "Voltages", formatVoltText);
    createChart(bupsData -> vbox, &bupsData -> freqChart, firstCreate, freqNames, "Freq"    , formatFreqText);
    createChart(bupsData -> vbox, &bupsData -> tempChart, firstCreate, tempNames, "Stats"   , formatTempText);

	bupsData -> logStyle = gkrellm_meter_style(style_id);
    bupsData -> logDecal = gkrellm_create_decal_text(bupsData -> logDisplay, "Afp0",
                                                     gkrellm_meter_alt_textstyle(style_id), 
                                                     bupsData -> logStyle, -1, -1, -1);
    
    bupsData -> labelDecal = gkrellm_create_decal_text(bupsData -> logDisplay, bupsData -> logLabel,
                                                     gkrellm_meter_textstyle(style_id), 
                                                     bupsData -> logStyle, -1, -1, -1);

    labelWidth = gdk_string_width(bupsData -> labelDecal -> text_style.font, bupsData -> logLabel);
    if(labelWidth < bupsData -> labelDecal -> w) {
        bupsData -> labelX = (bupsData -> labelDecal -> w - labelWidth) / 2;
    } else {
        bupsData -> labelX = 0;
    }

    bupsData -> logScr = 0;

	gkrellm_panel_configure(bupsData -> logDisplay, NULL, bupsData -> logStyle);
	gkrellm_panel_create(vbox, mon, bupsData -> logDisplay);
     
    config -> showLog = !config -> showLog;
    drawLog();
    config -> showLog = !config -> showLog;
    drawLog();

    if(config -> showLog) {
        gkrellm_make_decal_visible(bupsData -> logDisplay, bupsData -> logDecal);
        gkrellm_make_decal_invisible(bupsData -> logDisplay, bupsData -> labelDecal);
    } else {
        gkrellm_make_decal_invisible(bupsData -> logDisplay, bupsData -> logDecal);
        gkrellm_make_decal_visible(bupsData -> logDisplay, bupsData -> labelDecal);
    }

    if(firstCreate) {
 		gtk_signal_connect(GTK_OBJECT(bupsData -> logDisplay -> drawing_area), 
                           "expose_event",
                           (GtkSignalFunc)exposeLogEvent, NULL);
		gtk_signal_connect(GTK_OBJECT(bupsData -> logDisplay -> drawing_area),
                           "button_press_event", 
                           (GtkSignalFunc)cbLogClick, NULL);
    }
}

/** Save the user settings.
 *  Write the configuration data to the specified file. Note that some of the 
 *  values come from the config structure, but the show chart texts, which are
 *  only proxied in the config structure, are taken from the chart data structs. 
 */
static void saveConfig(FILE *file)
{
    fprintf(file, "%s host %s\n"       , MONITOR_CONFIG_KEYWORD, config -> host);
    fprintf(file, "%s port %d\n"       , MONITOR_CONFIG_KEYWORD, config -> port);
    fprintf(file, "%s mains %d\n"      , MONITOR_CONFIG_KEYWORD, config -> mains);
    fprintf(file, "%s showlog %d\n"    , MONITOR_CONFIG_KEYWORD, config -> showLog);
    fprintf(file, "%s volt_format %s\n", MONITOR_CONFIG_KEYWORD, bupsData -> voltChart.textFormat);
    fprintf(file, "%s freq_format %s\n", MONITOR_CONFIG_KEYWORD, bupsData -> freqChart.textFormat);
    fprintf(file, "%s temp_format %s\n", MONITOR_CONFIG_KEYWORD, bupsData -> tempChart.textFormat);
    fprintf(file, "%s show_volt %d\n"  , MONITOR_CONFIG_KEYWORD, bupsData -> voltChart.showText);
    fprintf(file, "%s show_freq %d\n"  , MONITOR_CONFIG_KEYWORD, bupsData -> freqChart.showText);
    fprintf(file, "%s show_temp %d\n"  , MONITOR_CONFIG_KEYWORD, bupsData -> tempChart.showText);
	gkrellm_save_chartconfig(file, bupsData -> voltChart.config, MONITOR_CONFIG_KEYWORD, "volt");
	gkrellm_save_chartconfig(file, bupsData -> freqChart.config, MONITOR_CONFIG_KEYWORD, "freq");
	gkrellm_save_chartconfig(file, bupsData -> tempChart.config, MONITOR_CONFIG_KEYWORD, "temp");
}

/** Load the user settings.
 *  Attepts to extract Useful Information(tm) from a string presented to the
 *  function buy GKrellM. Things are slightly more complicated than the average
 *  case by the fact that we need to distinguish between three chartconfig lines
 *  rather than the single chart mst plugins deal with.
 */
static void loadConfig(gchar *line)
{
    gchar keyword[31], name[31];
    gchar data[CONFIG_BUFSIZE], conf[CONFIG_BUFSIZE];

    if(2 == sscanf(line, "%31s %[^\n]", keyword, data)) {
        if(!strcmp(keyword, "host")) {
            strcpy(config -> host, data);
        } else if(!strcmp(keyword, "port")) {
            config -> port = strtol(data, NULL, 10);
        } else if(!strcmp(keyword, "mains")) {
            config -> mains = strtol(data, NULL, 10);
        } else if(!strcmp(keyword, "showlog")) {
            config -> showLog = strtol(data, NULL, 10);
        } else if(!strcmp(keyword, "volt_format")) {
            gkrellm_dup_string(&bupsData -> voltChart.textFormat, data);
        } else if(!strcmp(keyword, "freq_format")) {
            gkrellm_dup_string(&bupsData -> freqChart.textFormat, data);
        } else if(!strcmp(keyword, "temp_format")) {
            gkrellm_dup_string(&bupsData -> tempChart.textFormat, data);
        } else if(!strcmp(keyword, "show_volt")) {
            bupsData -> voltChart.showText = strtol(data, NULL, 10);
        } else if(!strcmp(keyword, "show_freq")) {
            bupsData -> freqChart.showText = strtol(data, NULL, 10);
        } else if(!strcmp(keyword, "show_temp")) {
            bupsData -> tempChart.showText = strtol(data, NULL, 10);
        } else if(!strcmp(keyword, GKRELLM_CHARTCONFIG_KEYWORD)) {
            /* line is a chartconfig, need to do a bit more parsing to work out 
             * which chart this is the config for..
             */
            if(2 == sscanf(data, "%31s %[^\n]", name, conf)) {
                if(!strcmp(name, "volt")) {
                    gkrellm_load_chartconfig(&bupsData -> voltChart.config, conf, 3);
                } else if(!strcmp(name, "freq")) {
                    gkrellm_load_chartconfig(&bupsData -> freqChart.config, conf, 2);
                } else if(!strcmp(name, "temp")) {
                    gkrellm_load_chartconfig(&bupsData -> tempChart.config, conf, 2);
                }
            }
        }
    }
}

/** Update the plugin configuration based on values in the user interface.
 *  This copies the values from the gadgets in the tab created by createTab()
 *  into the config structure. Note that this will only restart the client
 *  when the host or port have changed!
 */
static void applyConfig(void)
{
    gchar *contents;
    gint   portset;

    contents = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(voltCombo)->entry));
    if(gkrellm_dup_string(&bupsData -> voltChart.textFormat, contents)) {
        drawChart(&bupsData -> voltChart);
    }

    contents = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(freqCombo)->entry));
    if(gkrellm_dup_string(&bupsData -> freqChart.textFormat, contents)) {
        drawChart(&bupsData -> freqChart);
    }

    contents = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(tempCombo)->entry));
    if(gkrellm_dup_string(&bupsData -> tempChart.textFormat, contents)) {
        drawChart(&bupsData -> tempChart);
    }
    
    config -> mains = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(mainsWidget));

    contents = gtk_entry_get_text(GTK_ENTRY(hostWidget));
    portset  = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(portWidget));

    /* Only restart the client if we really have to as this can have a visible
     * effect on updates of the main window.
     */
    if(strcmp(contents, config -> host) || (portset != config -> port)) {
        haltClient(bupsData -> client);
        strcpy(config -> host, contents);
        config -> port = portset;
        bupsData -> client = launchClient(config -> host, config -> port);
    }
}

/** Create the configuration tab.
 *  Must admit I cheated here - made the interface in glade and hand-edited 
 *  the result. The help and about tabs are based on the ones from gkrellweather.
 */
static void createTab(GtkWidget *tab)
{
    GtkWidget *note;
    GtkWidget *optLabel;
    GtkWidget *vbox1;
    GtkWidget *formats;
    GtkWidget *table1;
    GList *voltCombo_items = NULL;
    GList *freqCombo_items = NULL;
    GList *tempCombo_items = NULL;
    GtkWidget *tempLabel;
    GtkWidget *voltLabel;
    GtkWidget *freqLabel;
    GtkObject *mainsWidget_adj;
    GtkWidget *mainsLabel;
    GtkWidget *server;
    GtkWidget *table2;
    GtkWidget *hostLabel;
    GtkObject *portWidget_adj;
    GtkWidget *portLabel;
    GtkWidget *label;
    GtkWidget *frame;
    GtkWidget *text;
    GtkWidget *infoWindow;
    GtkWidget *aboutLabel;
    
    note = gtk_notebook_new();
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(note), GTK_POS_TOP);
    gtk_box_pack_start(GTK_BOX(tab), note, TRUE, TRUE, 0);

    vbox1 = gtk_vbox_new(FALSE, 0);
    gtk_container_border_width(GTK_CONTAINER(vbox1), 3);
    gtk_widget_show(vbox1);
    optLabel = gtk_label_new("Options");
    gtk_notebook_append_page(GTK_NOTEBOOK(note), vbox1, optLabel);

    formats = gtk_frame_new("Chart settings");
    gtk_widget_show(formats);
    gtk_box_pack_start(GTK_BOX(vbox1), formats, TRUE, TRUE, 0);

    table1 = gtk_table_new(4, 2, FALSE);
    gtk_container_border_width(GTK_CONTAINER(table1), 3);
    gtk_widget_show(table1);
    gtk_container_add(GTK_CONTAINER(formats), table1);
    gtk_table_set_row_spacings(GTK_TABLE(table1), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table1), 2);

    voltCombo = gtk_combo_new();
    gtk_widget_show(voltCombo);
    gtk_table_attach(GTK_TABLE(table1), voltCombo, 0, 1, 0, 1,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0), 0, 0);
    voltCombo_items = NULL;
    voltCombo_items = g_list_append(voltCombo_items, (gpointer)"i:\\f$i,\\.o:\\f$o,\\nb:\\f$b");
    voltCombo_items = g_list_append(voltCombo_items, (gpointer)"i:\\f$i,\\.o:\\f$o,\\nb:\\f$l%");
    voltCombo_items = g_list_append(voltCombo_items, (gpointer)"i:\\f$i,\\.o:\\f$o");
    voltCombo_items = g_list_append(voltCombo_items, (gpointer)"i:\\f$i,\\no:\\f$o");
    gtk_combo_set_popdown_strings(GTK_COMBO(voltCombo), voltCombo_items);
    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(voltCombo)->entry), bupsData -> voltChart.textFormat);
    g_list_free(voltCombo_items);

    freqCombo = gtk_combo_new();
    gtk_widget_show(freqCombo);
    gtk_table_attach(GTK_TABLE(table1), freqCombo, 0, 1, 1, 2,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0), 0, 0);
    freqCombo_items = NULL;
    freqCombo_items = g_list_append(freqCombo_items, (gpointer)"i:\\f$i\\no:\\f$o");
    freqCombo_items = g_list_append(freqCombo_items, (gpointer)"i:\\f$i,\\.o:\\f$o");
    gtk_combo_set_popdown_strings(GTK_COMBO(freqCombo), freqCombo_items);
    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(freqCombo)->entry), bupsData -> freqChart.textFormat);
    g_list_free(freqCombo_items);

    tempCombo = gtk_combo_new();
    gtk_widget_show(tempCombo);
    gtk_table_attach(GTK_TABLE(table1), tempCombo, 0, 1, 2, 3,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0), 0, 0);
    tempCombo_items = NULL;
    tempCombo_items = g_list_append(tempCombo_items, (gpointer)"t:\\f$tC\\nl:\\f$l%");
    tempCombo_items = g_list_append(tempCombo_items, (gpointer)"t:\\f$tC,\\.l:\\f$l%");
    gtk_combo_set_popdown_strings(GTK_COMBO(tempCombo), tempCombo_items);
    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(tempCombo)->entry), bupsData -> tempChart.textFormat);
    g_list_free(tempCombo_items);

    mainsWidget_adj = gtk_adjustment_new(config -> mains, 0, 250, 1, 10, 20);
    mainsWidget = gtk_spin_button_new(GTK_ADJUSTMENT(mainsWidget_adj), 1, 0);
    gtk_widget_show(mainsWidget);
    gtk_table_attach(GTK_TABLE(table1), mainsWidget, 0, 1, 3, 4,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0), 0, 0);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(mainsWidget), TRUE);

    voltLabel = gtk_label_new("Voltage chart format");
    gtk_widget_show(voltLabel);
    gtk_table_attach(GTK_TABLE(table1), voltLabel, 1, 2, 0, 1,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0), 0, 0);
    gtk_label_set_justify(GTK_LABEL(voltLabel), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment(GTK_MISC(voltLabel), 0, 0.5);

    freqLabel = gtk_label_new("Frequency chart format");
    gtk_widget_show(freqLabel);
    gtk_table_attach(GTK_TABLE(table1), freqLabel, 1, 2, 1, 2,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0), 0, 0);
    gtk_label_set_justify(GTK_LABEL(freqLabel), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment(GTK_MISC(freqLabel), 0, 0.5);

    tempLabel = gtk_label_new("Temperature chart format");
    gtk_widget_show(tempLabel);
    gtk_table_attach(GTK_TABLE(table1), tempLabel, 1, 2, 2, 3,
                    (GtkAttachOptions)(GTK_FILL),
                    (GtkAttachOptions)(0), 0, 0);
    gtk_label_set_justify(GTK_LABEL(tempLabel), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment(GTK_MISC(tempLabel), 0, 0.5);

    mainsLabel = gtk_label_new("Mains (see info page)");
    gtk_widget_show(mainsLabel);
    gtk_table_attach(GTK_TABLE(table1), mainsLabel, 1, 2, 3, 4,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0), 0, 0);
    gtk_label_set_justify(GTK_LABEL(mainsLabel), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment(GTK_MISC(mainsLabel), 0, 0.5);

    server = gtk_frame_new("UPS Server");
    gtk_widget_show(server);
    gtk_box_pack_start(GTK_BOX(vbox1), server, TRUE, TRUE, 0);

    table2 = gtk_table_new(2, 2, FALSE);
    gtk_container_border_width(GTK_CONTAINER(table2), 3);
    gtk_widget_show (table2);
    gtk_container_add(GTK_CONTAINER(server), table2);
    gtk_table_set_row_spacings(GTK_TABLE(table2), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table2), 2);

    hostWidget = gtk_entry_new_with_max_length(256);
    gtk_widget_show(hostWidget);
    gtk_table_attach(GTK_TABLE(table2), hostWidget, 0, 1, 0, 1,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0), 0, 0);
    gtk_entry_set_text(GTK_ENTRY(hostWidget), config -> host);

    hostLabel = gtk_label_new("Hostname");
    gtk_widget_show(hostLabel);
    gtk_table_attach(GTK_TABLE(table2), hostLabel, 1, 2, 0, 1,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0), 0, 0);
    gtk_label_set_justify(GTK_LABEL(hostLabel), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment(GTK_MISC(hostLabel), 0, 0.5);

    portWidget_adj = gtk_adjustment_new(config -> port, 0, 65535, 1, 10, 20);
    portWidget = gtk_spin_button_new(GTK_ADJUSTMENT(portWidget_adj), 1, 0);
    gtk_widget_show(portWidget);
    gtk_table_attach(GTK_TABLE(table2), portWidget, 0, 1, 1, 2,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0), 0, 0);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(portWidget), TRUE);

    portLabel = gtk_label_new("Port");
    gtk_widget_show(portLabel);
    gtk_table_attach(GTK_TABLE(table2), portLabel, 1, 2, 1, 2,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0), 0, 0);
    gtk_label_set_justify(GTK_LABEL(portLabel), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment(GTK_MISC(portLabel), 0, 0.5);

    /* Help Tab */
    frame = gtk_frame_new(NULL);
    gtk_container_border_width(GTK_CONTAINER(frame), 3);
    infoWindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(infoWindow),
                                   GTK_POLICY_AUTOMATIC, 
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(frame), infoWindow);

    text = gtk_text_new(NULL, NULL);

    gkrellm_add_info_text(text, helpText, sizeof(helpText)/sizeof(gchar*));
    gtk_text_set_editable(GTK_TEXT(text), FALSE);
    gtk_container_add(GTK_CONTAINER(infoWindow), text);

    label = gtk_label_new("Help");
    gtk_notebook_append_page(GTK_NOTEBOOK(note), frame, label);

    /* About tab */
    aboutLabel = gtk_label_new(aboutText);
    label = gtk_label_new("About");
    gtk_notebook_append_page(GTK_NOTEBOOK(note), aboutLabel, label);

}

/** Create and initialise the config structure to default values.
 *  <insert Interesting And Informtive Comment here>
 */
static void createDefaultConfig(void)
{
    /* hope and pray this works... :/ */
    config = g_new0(BUPSConfig, 1); 

    /* set default configuration */
    strcpy(config -> host, DEFAULT_HOST);
    config -> port    = DEFAULT_PORT;
    config -> showLog = FALSE;
    config -> mains   = MAINS_MIN;
}

/** GKrellM Monitor structure for this plugin. 
 *  A pointer to this is returned by init_plugin.
 */
static Monitor bups_mon =
{
    CONFIG_NAME,            /*!< Name, for config tab.                   */
    0,                      /*!< Id,  0 if a plugin                      */
    createPlugin,           /*!< The create_plugin() function            */
    updatePlugin,           /*!< The update_plugin() function            */
    createTab,              /*!< The create_plugin_tab() config function */
    applyConfig,            /*!< The apply_plugin_config() function      */

    saveConfig,             /*!< The save_plugin_config() function       */
    loadConfig,             /*!< The load_plugin_config() function       */
    MONITOR_CONFIG_KEYWORD, /*!< config keyword                          */

    NULL,                   /*!< Undefined 2  */
    NULL,                   /*!< Undefined 1  */
    NULL,                   /*!< private      */

    INSERT_BEFORE,          /*!< Insert plugin before this monitor.      */
    NULL,                   /*!< Handle if a plugin filled in by GKrellM */
    NULL                    /*!< path if a plugin filled in by GKrellM   */
};

/** Plugin initialisation function. 
 */
Monitor *init_plugin(void)
{
    bupsData = g_new0(GKrellMBUPS, 1);
    createDefaultConfig();

	style_id = gkrellm_add_chart_style(&bups_mon, STYLE_NAME);
	mon = &bups_mon;
	return &bups_mon;
}
