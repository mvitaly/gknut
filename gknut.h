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
 *
 *  \file gkrellmbups.h
 *  Header for the GKrellM plugin.
 *  This header is mainly here to pull all the structure definitions and #defines
 *  out of the gkrellmbups.c file (I hate putting such things in a .c file - call
 *  it a style quirk..)
 *
 *  $Author: chris $
 *  $Revision: 1.5 $
 */
/* $Id: gkrellmbups.h,v 1.5 2002/01/05 13:48:21 chris Exp $
 *
 * $Log: gkrellmbups.h,v $
 * Revision 1.5  2002/01/05 13:48:21  chris
 * Added code to support non-UK UPS MAINS_MIN settings.
 *
 * Revision 1.4  2001/12/28 13:22:38  chris
 * Added user control over the mains offset value.
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

#ifndef _GKRELLMBUPS_H
#define _GKRELLMBUPS_H

/*! Convenience macro to make limiting values to l or greater easier. */
#define LIM_FLOOR(x, l) ((x) < (l)) ? (l) : (x)

#define CONFIG_NAME             "Gknut"  /*!< Name for the configuration tab.    */
#define MONITOR_CONFIG_KEYWORD  "gknut"  /*!< Configuration name.                */
#define STYLE_NAME              "gknut"  /*!< style name to allow custom themes. */
#define INSERT_BEFORE           MON_FS         /*!< Insert plugin before this monitor. */

#define	MIN_GRID_RES            1              /*!< Constrain grid resolution, lower end. */ 
#define	MAX_GRID_RES            40             /*!< Constrain grid resolution, upper end. */              

#define MAX_DATA                3              /*!< Maximum number of chartdata entries per chart */

/*! Value to subtract from input and output mains voltages.
 *  Mains voltages are typically 220 to 240 in the UK, this presented some problems with
 *  fitting any sort of meaningful data on the chart - either the chart is prohibitively
 *  large or any difference between the signals is swamped by the scale. Subtracting 
 *  MAINS_MIN from the values before display ensures that both signals will fit on the 
 *  display without significant loss of detail on a 40/50 pixel high monitor.
 *  \note MAINS_MIN should be approximately the same as the UPS low voltage transfer
 *  value (187 on a UK Belkin UPS)
 */
#ifndef MAINS_MIN
#define MAINS_MIN               190           
#endif

#define DEFAULT_HOST            "localhost"   /*!< address of the computer on which upsd is running */
#define DEFAULT_PORT            3305          /*!< port upsd is accepting connections on            */

#define DEFAULT_CHARTHEIGHT     40            /*!< 40 is probably a good trade between detail and screen use */  

/*! Structure containing data related to a single chart object.
 *  This structure contains pointers to the various elements which together form
 *  a single chart in the GKrellM window (chart, config, panel etc).
 */
typedef struct
{
    GtkWidget   *vbox;           /*!< Box into which the Chart and then Panel are added. */
    Chart       *chart;          /*!< The chart contained in vbox. */
    ChartData   *data[MAX_DATA]; /*!< The data shown in the chart. */
    ChartConfig *config;         /*!< Settings structure for the chart. */
    Style       *style;          /*!< chart and panel style. */
    Panel       *panel;          /*!< The panel shown beneath the chart, this is just a label really. */
    gboolean     showText;       /*!< True if the chart text overlay should be drawn. */
    char        *textFormat;     /*!< Text overlay format for this chart. */
    void       (*format)(gchar *, gint, gchar *); /*!< Text formatting function */
} BUPSChart;

/*! Central data store structure.
 *  This contains pointers to all the UPSChart structures and various gkrellm 
 *  objects used by the plugin.
 */
typedef struct
{
    BUPSChart  voltChart;   /*!< Input and output and battery voltage display.               */
    BUPSChart  freqChart;   /*!< Input and output frequency chart.                           */
    BUPSChart  tempChart;   /*!< Temperature and load chart (fixed max is 100).              */
    Panel     *logDisplay;  /*!< Panel on which a decal can scroll the last UPS log message. */
    Style     *logStyle;    /*!< Style data for the loag display panel.                      */
    Decal     *logDecal;    /*!< Decal used on logDisplay.                                   */
    gchar     *logLabel;    /*!< Text displayed when the log display is deactivated          */
    gchar     *logText;     /*!< Text displayed when the log display is activated            */
    gint       logScr;      /*!< Horizontal scroll                                           */
    Decal     *labelDecal;  /*!< Decal used on logDisplay.                                   */
    gint       labelX;      /*!< Horizontal position of the label                            */
    GtkWidget *vbox;
    pthread_t  client;      
} GKrellMBUPS;

/*! Maximum length of the hostname string the user can specify (plus one for the terminator) */
#define MAX_HOSTNAME 257

#define DEFAULT_VFORMAT "i:\\f$i,\\.o:\\f$o,\\nb:\\f$l%" 
#define DEFAULT_FFORMAT "i:\\f$i\\no:\\f$o" 
#define DEFAULT_TFORMAT "t:\\f$tC\\nl:\\f$l%" 

/*! Configuration option storage.
 *  The user-definable options are stored in this structure 
 */
typedef struct
{
    gchar        host[MAX_HOSTNAME]; /*!< Hostname on which the upsd service is running (default is localhost).     */
    gint         port;               /*!< Port on which the upsd server is accepting connections (2710 is default). */
    gboolean     showLog;            /*!< FALSE to show label, TRUE to show log.                                    */
    gint         mains;              /*!< Utility low battery transfer voltage or similar.                          */ 
} BUPSConfig;

#define CONFIG_BUFSIZE 256          /*!< Size of the buffers used for storing configuration data in loadConfig().  */

#endif /* _GKRELLMBUPS_H */
