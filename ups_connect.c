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
 *  \file ups_connect.c
 *  UPS service connection and parsing functions.
 *  This file contains the code which connects to the upsd service and parses
 *  the results from the connection into UPSData structures.
 *
 *  $Author: chris $
 *  $Revision: 1.8 $
 *
 * I've no idea if this information is available anywhere but the contents of
 * this comment have been derived by reverse enginerring of the protocol so
 * expect errors. A thorough search with google and a manual search of the 
 * Belkin site turned up no information on the Belkin Sentry Bulldog upsd 
 * protocol hence some of this is guesswork!  
 *
 * Data obtained from the upsd service takes the form of tab seperated data 
 * fields of the generic form 
 *
 * DeltaUPS:<code>,00,<flag> <TSVs>
 *
 * When a program initially connects to the ups service (localhost.2710 from
 * the local machine) a sigificant amount of general status information is 
 * sent before the actual UPS status data. Much of this appears to be echoed
 * versions of the PRO_* files in the bulldog directory. The values we are
 * interested in start DeltaUPS:VAL00,00. Here are the fields I have been able
 * to positively identify:
 * <PRE>
 * DeltaUPS:VAL00,00,xxxx P\t  xxxx is of unknown purpose. P=1 if UPS present
 *                             otherwise P = 0 (I think!)
 * field 1 to 4 (usually 1\t536915997\t603979777\t8\t)     unknown
 * field 5                                                 Battery voltage
 * field 6                                                 unknown
 * field 7                                                 Battery level
 * field 8                                                 Input frequency
 * field 9                                                 Input voltage
 * fields 10 to 16 (usually -1\t)                          unknown
 * field 17                                                Output frequency
 * field 18                                                Output voltage
 * field 19 (usually -1\t)                                 unknown
 * field 20                                                Load level
 * fields 21 to 33 (usually -1\t)                          unknown
 * field 34                                                Temperature
 * Purpose of all later fields is unknown.
 * </PRE>
 * Log entries appear to be of the form
 *
 * DeltaUPS:LOG00,00,xxxx 0 <date> <time> '<'<message>'>'
 *
 * Log entries are followed by a CR+LF sequence (?)
 *
 * \sa 
 * http://www.belkin.com/ - UPS details <BR>
 * Unix Network Proramming Vol1, 2nd Ed (ISBN 0-13-490012-X) - network programming
 * reference
 * 
 */
/* $Id: ups_connect.c,v 1.8 2001/12/25 20:30:52 chris Exp $
 *
 * $Log: ups_connect.c,v $
 * Revision 1.8  2001/12/25 20:30:52  chris
 * Bugfix - log message was not being updated on refused connection.
 *
 * Revision 1.7  2001/12/24 17:02:08  chris
 * More inteilligent handling of refused connections (I hope..)
 *
 * Revision 1.6  2001/12/23 15:40:08  chris
 * Added halt function and tidied up a bit.
 *
 * Revision 1.5  2001/12/23 13:07:07  chris
 * UPS connection and result parsing more or less complete, some minor
 * tidying up and optimisation to do but ths code is more or less beta..
 *
 * Revision 1.4  2001/12/16 13:40:40  chris
 * Bah, hopefully the last of these rather pointless revision commits! L
 * ooks like doxygen can understand CVS author and revision tags
 * (it better be that considering I'm submiting this assuming it can!)
 *
 * Revision 1.3  2001/12/16 13:36:06  chris
 * Finished tidying up (and noticed author and revision were not being substituted properly..)
 *
 * Revision 1.2  2001/12/16 13:33:47  chris
 * Removed testing main, testing must now be done by other modules.
 *
 * Revision 1.1.1.1  2001/12/16 13:29:23  chris
 */

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<unistd.h>
#include<sys/select.h>
#include<sys/socket.h>
#include<netdb.h>
#include"ups_connect.h"

struct UPSData upsStatus; /*!< Global UPS data structure, must be synchronised across threads! */
pthread_mutex_t upsStatus_lock = PTHREAD_MUTEX_INITIALIZER; /*!< Synchronisation mutex for upsStatus */

gboolean haltThread = FALSE; /*!< Used to shut down the client thread from gkrellm, set to TRUE to halt then pthread_join */

static const gchar noUPS[]  = "UPS not connected";
static const gchar gotUPS[] = "UPS monitoring active";
static const gchar noMem[]  = "Out of memory";
static const gchar noSock[] = "Socket error";
static const gchar badHost[]= "Unable to find host";
static const gchar badConn[]= "Connection refused";
static const gchar disconHost[]= "Disconnecting from server";

static const gchar reqUtility[] = "REQ UTILITY\n";
static const gchar reqAcfreq[]  = "REQ ACFREQ\n";
static const gchar reqBattpct[] = "REQ BATTPCT\n";
static const gchar reqLoadpct[] = "REQ LOADPCT\n";
static const gchar reqStatus[]  = "REQ STATUS\n";

static gchar upsd_host[257];
static gint  upsd_port;

/** Clear the specified UPSData structure. 
 *  Use this to zero all the fields of a UPSData structure. Mainly intended to 
 *  simplify the initialisation of static structures.
 */
static void resetStatus(struct UPSData *target)
{
    target -> bat_Voltage = 1.0;
    target -> bat_Level   = 1.0;
    target -> in_Freq     = 1.0;
    target -> in_Voltage  = 1.0;
    target -> out_Freq    = 1.0;
    target -> out_Voltage = 1.0;
    target -> ups_Load    = 1.0;
    target -> ups_Temp    = 1.0;
    target -> ups_LastLog[0] = '\0';
    target -> ups_Present = FALSE;
}

/** Set the ups_LastLog field of a UPSData structure.
 *  Setting the ups_LastLog is slightly more work than using strcpy as I
 *  want to ensure that the buffer can not overflow (fairly vital as the
 *  socket fd is after the buffer in memory!). This uses strncpy, checks
 *  that the buffer is not overflowed and ensures that the string is 
 *  null terminated.
 *
 *  \par Arguments:
 *  \arg \c target - UPSData structure containing the log field to set.
 *  \arg \c log - String to set the ups_LastLog field to.
 */
static void setLastLog(struct UPSData *target, const gchar *log)
{
    int size = MIN(strlen(gotUPS), MAX_LOGSIZE - 1);

    strncpy(target -> ups_LastLog, gotUPS, size);
    target -> ups_LastLog[size] = 0;
}

/** Traverse string until a number of tabs have been encountered.
 *  The data generated by the upsd service is basically tab separated and
 *  most of the fields in the data appear to serve no purpose. This
 *  function allows a number of tabs to be skipped in one go, reducing
 *  the amount of waffle needed in parseVAL().
 *
 *  \par Arguments:
 *  \arg \c str - String to be processed.
 *  \arg \c count - Number of tags to skip.
 *  \return A pointer to the character after the last tab skipped, or the
 *  end of the string (if the string ended before enough tabs had been
 *  skipped.
 */
static gchar *skipTabs(gchar *str, int count)
{
    while(*str && count) {
        while(*str && (*str != '\t')) {
            str ++;
        }

        /* skip the tab we are over..*/
        if(*str == '\t') str++;
        count --;
    }
    return str;
}

/** Parse a string for UPS status data.
 *  This parses a string containing the status information generated
 *  by the upsd service into the supplied UPSData structure.
 *
 *  \note This function does not do any real checking on the string,
 *  it assumes it is a VAL string - do not expect it to parse any 
 *  other string without falling over or making a complete arse of it.
 *
 *  \par Arguments:
 *  \arg \c buffer - String to parse.
 *  \arg \c target - UPSData structure to fill in with parsed values.
 */
static void parseVAL(gchar *buffer, struct UPSData *target)
{
    buffer = skipTabs(buffer, 5);
    target -> bat_Voltage = (float)strtol(buffer, NULL, 10) / 10.0;
    buffer = skipTabs(buffer, 2);
    target -> bat_Level = (float)strtol(buffer, NULL, 10) / 10.0;
    buffer = skipTabs(buffer, 1);
    target -> in_Freq = (float)strtol(buffer, NULL, 10) / 10.0;
    buffer = skipTabs(buffer, 1);
    target -> in_Voltage = (float)strtol(buffer, NULL, 10) / 10.0;
    buffer = skipTabs(buffer, 8);
    target -> out_Freq = (float)strtol(buffer, NULL, 10) / 10.0;
    buffer = skipTabs(buffer, 1);
    target -> out_Voltage = (float)strtol(buffer, NULL, 10) / 10.0;
    buffer = skipTabs(buffer, 2);
    target -> ups_Load = (float)strtol(buffer, NULL, 10) / 10.0;
    buffer = skipTabs(buffer,14);
    target -> ups_Temp = (float)strtol(buffer, NULL, 10) / 10.0;           
    if(strlen(target -> ups_LastLog) == 0) setLastLog(target, gotUPS);
    target -> ups_Present = TRUE;
}

/** Parse a string for UPS log messages.
 *  This attempts to copy a UPS log message from the specified string into the
 *  ups_LastLog field of the supplied UPSData structure. This uses strncpy to
 *  ensure that the log buffer can not overflow.
 *
 *  \note This function can handle being called on a string even when it 
 *  doesn't contian log data, in this case the ups_LastLog of the specified
 *  UPSData structure is unchange. This is a waste of CPU time though so
 *  it's better to check the LOG id is in the string before calling this!
 *
 *  \par Arguments:
 *  \arg \c buffer - The string to parse.
 *  \arg \c target - The UPSData structure containing the ups_LastLog to update.
 */
static void parseLOG(gchar *buffer, struct UPSData *target)
{
    int position = 0;

    /* run along the string until we hit the start of the log text */
    while(*buffer && (*buffer != '<')) buffer++;

    /* foound a log entry before hitting the end of string? */
    if(*buffer == '<') {
        buffer++;

        /* copy characters until end of string, end of log or overflow */
        while(*buffer && (*buffer != '>') && (position < (MAX_LOGSIZE - 1))) {
            target -> ups_LastLog[position] = *buffer;
            buffer++;
            position++;
        }
        target -> ups_LastLog[position] = '\0';
    }
}

/** Parse a string for UPS information.
 *  This attempts to identify the type of information held in the specified
 *  string and uses one of the above parseXXX functions to extract useful
 *  data from the string and store it in the provided UPSData structure.
 *
 *  \par Arguments:
 *  \arg \c buffer - The string to parse. This must start "DeltaUPS:" followed
 *       by one of the codes discussed in the file comment otherwise it is 
 *       ignored and target is unchanged.
 *  \arg \c target - The UPSData structure into which the results are saved.
 *  
 *  \returns 0 if the string was not a valid or parseable UPS information 
 *  string, 1 if UPSData has been modified.
 */ 
static int parseDeltaUPS(gchar *buffer, struct UPSData *target)
{
    /* Bad buffer contents, ignore it. */
    if(strncmp(buffer, "DeltaUPS:", 9)) return 0;

    buffer += 9;

    if(!strncmp(buffer, "VAL00", 5)) {
        buffer += 14; /* Skip VAL00,00,xxxx */
         if(*buffer != '1') {
             /* UPS has been disconnected/failed - clear structure and set error in log */
            target -> bat_Voltage = 0.0;
            target -> bat_Level   = 0.0;
            target -> in_Freq     = 0.0;
            target -> in_Voltage  = 0.0;
            target -> out_Freq    = 0.0;
            target -> out_Voltage = 0.0;
            target -> ups_Load    = 0.0;
            target -> ups_Temp    = 0.0;
            setLastLog(target, noUPS);
            target -> ups_Present = FALSE;
         } else {
            /* UPS is present and working, parse values */
            parseVAL(buffer, target);
        }            
    } else if(!strncmp(buffer, "LOG00", 5)) {
        /* LOG entry.. we are only interested in the log text... */
        parseLOG(buffer, target);
    } else {
        return 0;
    }
        
    return 1;
}

/** Read data from the upsd server and parse it into upsStatus.
 *  The actual client work is done by this routine - it reads from the server into a temporary
 *  buffer, then the buffer is copied into an accumulator buffer until a new DeltaUPS is 
 *  encountered in the temp buffer. At this point, parseDeltaUPS() is called to parse the
 *  accumulator, then the accululator write position is reset to the start of the accumulator
 *  buffer and the copy from the temporary buffer continues.
 *
 *  \par Arguments:
 *  \arg \c acc - buffer to use as an accumulator, must be at least MAX_LINESIZE characters in length.
 *  \arg \c temp - buffer to use as temporary store, again must be MAX_LINESIZE or greater.
 */
static void upsClient(gchar *acc, gchar *temp)
{
    gint readlen;
    gint accpos = 0;
    gint readpos;
    gchar *value;
    float blevel;

    /* continue reading from the server unit we are told to stop or the server shuts down.
     * (aside: This line was a bit of a problem - in testing the read() saturates the buffer
     * for the first few calls, then the process settles down to returning 214 characters
     * for each call to read() every second. This is fine, but if read() blocks until the 
     * buffer is full on some machines then it could take MAX_LINELEN\214 seconds for the
     * read to return - my temporary solution is to only use MAX_ENTRYSIZE bytes of temp
     * with MAX_ENTRYSIZE set to 214. It's an ugly hack, but it seems to work.)
     */
    while(!haltThread) {
        /* haltThread may have been set while blocked in the read, this is our Emergency Exit check.. */ 
        if(haltThread) return;

	write(upsStatus.ups_Socket, reqUtility, strlen(reqUtility));
	readlen = read(upsStatus.ups_Socket, temp, MAX_ENTRYSIZE);
	value = temp + strlen(reqUtility);
	temp[readlen - 3] = temp[readlen - 2];
	temp[readlen - 2] = 0;
        upsStatus.in_Voltage = (float)strtol(value, NULL, 10) / 10.0;

	write(upsStatus.ups_Socket, reqAcfreq, strlen(reqAcfreq));
	readlen = read(upsStatus.ups_Socket, temp, MAX_ENTRYSIZE);
	value = temp + strlen(reqAcfreq);
	temp[readlen - 4] = temp[readlen - 3];
	temp[readlen - 3] = temp[readlen - 2];
	temp[readlen - 2] = 0;
        upsStatus.out_Freq = (float)strtol(value, NULL, 10) / 100.0;

	write(upsStatus.ups_Socket, reqBattpct, strlen(reqBattpct));
	readlen = read(upsStatus.ups_Socket, temp, MAX_ENTRYSIZE);
	value = temp + strlen(reqBattpct);
	temp[readlen - 3] = temp[readlen - 2];
	temp[readlen - 2] = 0;
        bLevel = (float)strtol(value, NULL, 10) / 10.0;
	if(upsStatus.bat_Level > 100.0) upsStatus.ups_Load = 100.0;
	if(upsStatus.bat_Level < 0.0)   upsStatus.ups_Load = 0.0;

	write(upsStatus.ups_Socket, reqLoadpct, strlen(reqLoadpct));
	readlen = read(upsStatus.ups_Socket, temp, MAX_ENTRYSIZE);
	value = temp + strlen(reqLoadpct);
	temp[readlen - 3] = temp[readlen - 2];
	temp[readlen - 2] = 0;
        upsStatus.ups_Load = (float)strtol(value, NULL, 10) / 10.0;

	//if(strlen(upsStatus.ups_LastLog) == 0) 
	setLastLog(&upsStatus, gotUPS);
	upsStatus.ups_Present = TRUE;
	sleep(1);
    }
}

/** Set up the connection to the upsd service and start the client routine.
 *  This obtains the address of the host running the upsd service (normally
 *  localhost, but in theory you could remotely monitor your ups from another
 *  machine with this..), attempts to conenct to it and if it manages it the
 *  upsClient() routine is used to read into the upsStatus structure.
 *
 *  \todo The host lookup, connection and other tcp/ip code is IPv4 dependant.
 *  While this is not a great problem now, it may be at some point in the 
 *  future (although right now IPv6 takeup seems to be painfully slow so it
 *  is likely to be quite some time!). Really the whole client should be
 *  protocol independant.
 */
static int upsdConnect(gchar *hostname, guint port)
{
    struct sockaddr_in  servaddr; /* needed for connect */
    struct hostent     *host;
    struct in_addr    **addrPtr;
    gchar *accumulator;  /* This contains the string in progress.. */
    gchar *tempStore;    /* This is filled by read() */

    resetStatus(&upsStatus);

    /* Yeah, this will cause all manner of fun if it picks up an IPv6 record,
     * but for now it'll do.. */
    if((host = gethostbyname(hostname)) == NULL) {
        strncpy(upsStatus.ups_LastLog, badHost, MIN(strlen(badHost), MAX_LOGSIZE));
        return 0;
    }

    /* Allocate the main accumulator string, Bomb if this fails.. */
    if((accumulator = (gchar *)malloc(MAX_LINESIZE)) == NULL) {
        strncpy(upsStatus.ups_LastLog, noMem, MIN(strlen(noMem), MAX_LOGSIZE));
        return 0;
    }

    /* Likewise for the temporary buffer */
    if((tempStore = (gchar *)malloc(MAX_LINESIZE)) == NULL) {
        strncpy(upsStatus.ups_LastLog, noMem, MIN(strlen(noMem), MAX_LOGSIZE));
        free(accumulator);
        return 0;
    }

    /* Attempt to connect to the service designated by port on the hosts obtained by the
     * call to gethostbyname(). This breaks as soon as the first successful connection is
     * established.
     */
    addrPtr = (struct in_addr **)host -> h_addr_list;
    for(; *addrPtr != NULL; addrPtr ++) {
        if((upsStatus.ups_Socket = socket(AF_INET, SOCK_STREAM, 0))) {
            bzero(&servaddr, sizeof(servaddr));
            servaddr.sin_family = AF_INET;
            servaddr.sin_port = htons(port);
            memcpy(&servaddr.sin_addr, *addrPtr, sizeof(struct in_addr));

            if(connect(upsStatus.ups_Socket, (struct sockaddr *)&servaddr, sizeof(servaddr)) == 0)
              break;

            close(upsStatus.ups_Socket);
            upsStatus.ups_Socket = 0;
        }
    }

    /* Deligate the actual communication work to the client code if a connection has
     * been established. 
     */
    if(*addrPtr != NULL) {
        upsClient(accumulator, tempStore);
    } else {
        upsStatus.ups_Socket = 0;
        pthread_mutex_lock(&upsStatus_lock);
        strcpy(upsStatus.ups_LastLog, badConn);
        pthread_mutex_unlock(&upsStatus_lock);
    }

    if(upsStatus.ups_Socket) {
        close(upsStatus.ups_Socket);
        upsStatus.ups_Socket = 0;
    }

    free(accumulator);
    free(tempStore);

    return(*addrPtr != NULL);
}

/** ups client thread entrypoint.
 *  The launchClient() function uses this as the start routine argument to a
 *  pthread_create() call. This is simply a wrapper for the upsdConnect()
 *  function which fills in the host and port from global variables.
 */
void *upsStart(void *arg)
{
    resetStatus(&upsStatus);
    upsdConnect(upsd_host, 3305); //upsd_port);
    return NULL;
}

/** Create the client thread and return the thread id.
 *  This creates a new client which connects to hostname and port. Directly
 *  creating the thread using upsStart() is fine if the host and port are
 *  unchanged from the last call to launchClient(), but in general this 
 *  should always be used when creating clients.
 */
pthread_t launchClient(gchar *hostname, gint port)
{
    pthread_t result;

    strcpy(upsd_host, hostname);
    upsd_port = port;

    pthread_create(&result, NULL, upsStart, NULL);
    return result;
}

/** Force the specified client thread to exit.
 *  This will tell the client thread(s) to halt and wait for thread 'tid' to  
 *  exit before continuting. Use sparingly or it may affect gkrellm updates!
 */
void haltClient(pthread_t tid)
{
    haltThread = TRUE;
    pthread_join(tid, NULL);
    haltThread = FALSE;
}
    
