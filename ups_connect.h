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
 *  \file ups_connect.h
 *  UPS service connection and parsing header.
 *  This file is the header for the ups_connect.c file, it contains the 
 *  structures visible to other modules along with some other bits and pieces..
 *
 *  $Author: chris $
 *  $Revision: 1.4 $
 */
/* $Id: ups_connect.h,v 1.4 2001/12/23 15:40:08 chris Exp $
 *
 * $Log: ups_connect.h,v $
 * Revision 1.4  2001/12/23 15:40:08  chris
 * Added halt function and tidied up a bit.
 *
 * Revision 1.3  2001/12/23 13:07:07  chris
 * UPS connection and result parsing more or less complete, some minor
 * tidying up and optimisation to do but ths code is more or less beta..
 *
 * Revision 1.2  2001/12/16 13:44:10  chris
 * Added GPL header and tags, tidied up a bit.
 *
 */

#ifndef UPS_CONNECT
#define UPS_CONNECT

#include<glib.h>
#include<pthread.h>

/*! Please keep logs under this size - I enforce it anyway... */
#define MAX_LOGSIZE 256 

/*! Maximum size of a single DeltaUPS line (the largest I've found is around 350 chars) */
#define MAX_LINESIZE 1024

/*! Maximum size of a single entry (see the aside in upsClient() for more on this #define */
#define MAX_ENTRYSIZE 213

/** Structure to store UPS status values.
 *  This contains all the values I have been able to reverse engineer from the
 *  upsd output. The ups connect code attemps to parse the output of upsd into
 *  the fields of an instance of this structure. 
 */
struct UPSData
{
    gfloat   bat_Voltage;              /*!< Current pattery power in volts. */
    gfloat   bat_Level;                /*!< Battery power as a percentage of maximum power. */
    gfloat   in_Freq;                  /*!< Frequency of the utility input. */
    gfloat   in_Voltage;               /*!< Utility input voltage (always around 250v in my house!) */
    gfloat   out_Freq;                 /*!< Frequency the UPS is chucking out. */
    gfloat   out_Voltage;              /*!< Voltage the UPS is supplying (usually 220v for me). */
    gfloat   ups_Load;                 /*!< Connected load value */
    gfloat   ups_Temp;                 /*!< Internal temperature. */
    gchar    ups_LastLog[MAX_LOGSIZE]; /*!< Last log message (or error message from us...) */
    gboolean ups_Present;              /*!< TRUE if UPS connected, FALSE otherwise.  */
    int      ups_Socket;               /*!< Socket which is connected to the upsd service. */
};

/* global variables exported from ups_connect.c */
extern struct UPSData upsStatus;       /*!< Global UPS data structure, must be synchronised across threads! */
extern pthread_mutex_t upsStatus_lock; /*!< Synchronisation mutex for upsStatus.                            */

/* functions exported from ups_connect.c */
extern pthread_t launchClient(gchar *hostname, gint port); /*!< Create the client thread and return the thread id. */
extern void haltClient(pthread_t tid);                     /*!< Force the specified client thread to exit.         */ 

#endif
