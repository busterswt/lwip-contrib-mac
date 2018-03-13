/* *  test.c *   * *  Created by Eric Pooch on 1/4/14. *  Copyright 2014 Eric Pooch. All rights reserved. * */#include <stdlib.h>//#include <stdbool.h>#include <time.h>#include <SegLoad.h>#include <Memory.h>#include <Files.h>#include <Errors.h>#include "arch/macos_debug.h"#include "lwip/sys.h"#include "lwip/sio.h"#include "fs.h"#include "lwip/init.h"#include "lwip/ip.h"#include "lwip/tcp_impl.h"#include "lwip/tcpip.h"#include "lwip/netif.h"#if SER_DEBUG#include <stdarg.h>#endif#include "test.h"#if PPP_SUPPORT#include "netif/ppp/ppp.h"#endif#define MIN_FREE_MEM 28000#if LWIP_HAVE_SLIPIF#include "netif/slipif.h"#endif//#include "echo.h"#include "httpserver_raw/httpd.h"#if SER_DEBUGsio_fd_t debug_sio;#endif#if PPP_SUPPORTsio_fd_t ppp_sio;struct ppp_pcb_s *ppps;void ppp_link_status_cb(ppp_pcb *pcb, int err_code, void *ctx);void test_intf(void);void ppp_poll(void);void apps_init(void);#if PPP_NOTIFY_PHASEvoid ppp_notify_phase_cb(ppp_pcb *pcb, u8_t phase, void *ctx);//static u8_t ppp_phase;#endif /*PPP_NOTIFY_PHASE*/#endif /*PPP_SUPPORT*/#if LWIP_HAVE_SLIPIFstatic struct netif slipif;void test_intf(void);/* (manual) host IP configuration */static ip_addr_t ipaddr, netmask, gw;#endif#if LWIP_HAVE_LOOPIFstatic struct netif loopif;#endif// lwIP interval timerstruct test_timer {    u32_t time;					// Current time counter in ms    u32_t interval;				// Interval in ms	void (*timed_func)(void);   // Function to call};// List of lwIP interval timers#define PPP_TMR_INTERVAL 16#define PING_TMR_INTERVAL 1200static struct test_timer test_timers[] = {#if LWIP_TCP    //{ 0, TCP_TMR_INTERVAL,          tcp_tmr },#endif#if LWIP_ARP    { 0, ARP_TMR_INTERVAL,          etharp_tmr },#endif#if IP_REASSEMBLY    { 0, IP_TMR_INTERVAL,           ip_reass_tmr },#endif#if LWIP_AUTOIP    { 0, AUTOIP_TMR_INTERVAL,       autoip_tmr },#endif#if LWIP_IGMP    { 0, IGMP_TMR_INTERVAL,         igmp_tmr },#endif#if LWIP_DNS    { 0, DNS_TMR_INTERVAL,          dns_tmr },#endif#if PPP_SUPPORT   //{ 0, PPP_TMR_INTERVAL,			ppp_poll },	{ 0, 15000,						test_intf },#endif#ifndef TEST_MAIN_DISABLE	{ 0, 60000, test_stop },#endif	{ 0, 0, NULL }};static short apps_started;static u32_t last_ticks;#ifndef TEST_MAIN_DISABLEintmain(void){	/*	 Initialize menus	 Initialize user settings	 Initialize browser window	*/	/* initialize the program */	/* Chat with modem/server */	/* Start PPP/SLIP */	test_init();	while (1)	{		test_poll();			}	return 0;}#endif/* This function initializes applications */void apps_init(void){#if LWIP_DNS_APP && LWIP_DNS  /* wait until the netif is up (for dhcp, autoip or ppp) */  //sys_timeout(5000, dns_dorequest, NULL);#endif /* LWIP_DNS_APP && LWIP_DNS */#if LWIP_CHARGEN_APP && LWIP_SOCKET  chargen_init();#endif /* LWIP_CHARGEN_APP && LWIP_SOCKET */#if LWIP_PING_APP && LWIP_RAW && LWIP_ICMP  ping_init();#endif /* LWIP_PING_APP && LWIP_RAW && LWIP_ICMP */#if LWIP_NETBIOS_APP && LWIP_UDP  netbios_init();#endif /* LWIP_NETBIOS_APP && LWIP_UDP */#if LWIP_HTTPD_APP && LWIP_TCP#ifdef LWIP_HTTPD_APP_NETCONN  http_server_netconn_init();#else /* LWIP_HTTPD_APP_NETCONN */  httpd_init();#endif /* LWIP_HTTPD_APP_NETCONN */#endif /* LWIP_HTTPD_APP && LWIP_TCP */#if LWIP_NETIO_APP && LWIP_TCP  netio_init();#endif /* LWIP_NETIO_APP && LWIP_TCP */#if LWIP_RTP_APP && LWIP_SOCKET && LWIP_IGMP  rtp_init();#endif /* LWIP_RTP_APP && LWIP_SOCKET && LWIP_IGMP */#if LWIP_SNTP_APP && LWIP_SOCKET  sntp_init();#endif /* LWIP_SNTP_APP && LWIP_SOCKET */#if LWIP_SHELL_APP && LWIP_NETCONN  shell_init();#endif /* LWIP_SHELL_APP && LWIP_NETCONN */#if LWIP_TCPECHO_APP#if LWIP_NETCONN && defined(LWIP_TCPECHO_APP_NETCONN)  tcpecho_init();#else /* LWIP_NETCONN && defined(LWIP_TCPECHO_APP_NETCONN) */  echo_init();#endif /* LWIP_TCPECHO_APP && LWIP_NETCONN */#endif#if LWIP_UDPECHO_APP && LWIP_NETCONN  udpecho_init();#endif /* LWIP_UDPECHO_APP && LWIP_NETCONN */#if LWIP_SOCKET_EXAMPLES_APP && LWIP_SOCKET  socket_examples_init();#endif /* LWIP_SOCKET_EXAMPLES_APP && LWIP_SOCKET */#ifdef LWIP_APP_INIT  LWIP_APP_INIT();#endif}#pragma segment LWUPDNvoid test_init(){	static short initialized;#if PPP_SUPPORT	const char *username = "myuser";	const char *password = "mypassword";	int error = PPPERR_NONE;	 	 // Only initialize once	if (ppp_sio && initialized)        return;#else	lwip_init();#endif    initialized = true;	//setvbuf(stdout, NULL,_IONBF, 0);    last_ticks = sys_now();#if SER_DEBUG	debug_sio = sio_open(1);	LWIP_ERROR("Unable to open Printer port\n", debug_sio != NULL, NULL);#endif#if PPP_SUPPORT	ppp_sio = sio_open(0);	LWIP_ERROR("Unable to open Modem port\n", ppp_sio != NULL, exit(EXIT_FAILURE));	ppps = ppp_new();	//LWIP_DEBUGF(MACOS_TRACE, ("ppp_set_auth()\n"));	//ppp_set_auth(ppps, PPPAUTHTYPE_ANY, NULL, NULL);	//ppp_set_auth(ppps, PPPAUTHTYPE_PAP, "login", "password");#if PPP_NOTIFY_PHASE	ppp_set_notify_phase_callback(ppps, ppp_notify_phase_cb);#endif	error = ppp_over_serial_create(ppps, ppp_sio, ppp_link_status_cb, NULL);	if (error != PPPERR_NONE)	{		ppp_link_status_cb(ppps, error, NULL);		return;	}		ppp_open(ppps, 0);#endif	#if LWIP_HAVE_LOOPIF    {        struct ip_addr addr, netmask, gateway;        /* Setup default loopback device instance */        IP4_ADDR(&addr, 127,0,0,1);        IP4_ADDR(&netmask, 255,255,255,0);        IP4_ADDR(&gateway, 127,0,0,1);        netif_add(&loopif, &addr, &netmask, &gateway, NULL,                  loopif_init, ip_input);        netif_set_default(&loopif);	}#endif /* LWIP_HAVE_LOOPIF */#if LWIP_HAVE_SLIPIF    {        struct ip_addr addr, netmask, gateway;        /* Setup default SLIP device instance */        IP4_ADDR(&addr, 192,168,11,15);        IP4_ADDR(&netmask, 255,255,255,0);        IP4_ADDR(&gateway, 192,168,11,1);        netif_add(&slipif, &addr, &netmask, &gateway, NULL,                  slipif_init, ip_input);        netif_set_default(&slipif);		netif_set_up(&slipif);	}		apps_init();#endif /* LWIP_HAVE_SLIPIF */}#if PPP_SUPPORT#if PPP_NOTIFY_PHASEvoidppp_notify_phase_cb(ppp_pcb *pcb, u8_t phase, void *ctx){/* * Values for phase from ppp.h.#define PPP_PHASE_DEAD          0#define PPP_PHASE_INITIALIZE    1#define PPP_PHASE_SERIALCONN    2#define PPP_PHASE_DORMANT       3#define PPP_PHASE_ESTABLISH     4#define PPP_PHASE_AUTHENTICATE  5#define PPP_PHASE_CALLBACK      6#define PPP_PHASE_NETWORK       7#define PPP_PHASE_RUNNING       8#define PPP_PHASE_TERMINATE     9#define PPP_PHASE_DISCONNECT    10#define PPP_PHASE_HOLDOFF       11#define PPP_PHASE_MASTER        12*///	ppp_phase = phase;//	switch(phase)/* open a non-modal window, and update a string with the status.*/	switch (phase)	{		case 1:		case 2:		case 4:		case 5:			/* release LCP */		case 7:			/* init ICPC */		case 8:			/* release ICPC */		default:			LWIP_DEBUGF(MACOS_TRACE, ("ppp_notify_phase_cb: %d", phase)); 			MACOS_DLOGF(MACOS_DEBUG, ("PPP Phase Changed to: %d\n", phase));			break;	}	LWIP_DEBUGF(MACOS_STATE, ("App Limit: %lu \n", (long)GetApplLimit() ));	LWIP_DEBUGF(MACOS_STATE, ("Free Memory: %lu \n", (long)FreeMem() ));}#endifvoidppp_link_status_cb(ppp_pcb *pcb, int err_code, void *ctx){	LWIP_UNUSED_ARG(ctx);    switch(err_code) {		case PPPERR_NONE:               /* No error. */        {						struct netif *pppnetif = &pcb->netif;			struct ppp_addrs *ppp_addrs = ppp_addrs(pcb);						netif_set_default(pppnetif);						MACOS_DLOGF(MACOS_STATE, ("\n"));            MACOS_DLOGF(MACOS_DEBUG, ("  Remote IP Addr\t: %s\n", ip_ntoa(&ppp_addrs->his_ipaddr)));			MACOS_DLOGF(MACOS_DEBUG, ("  Local IP Addr\t\t: %s\n", ip_ntoa(&ppp_addrs->our_ipaddr)));            MACOS_DLOGF(MACOS_DEBUG, ("  Subnet Mask\t\t: %s\n", ip_ntoa(&ppp_addrs->netmask)));            MACOS_DLOGF(MACOS_DEBUG, ("  DNS Server 1\t\t: %s\n", ip_ntoa(&ppp_addrs->dns1)));            MACOS_DLOGF(MACOS_DEBUG, ("  DNS Server 2\t\t: %s\n", ip_ntoa(&ppp_addrs->dns2)));						if ((struct ppp_addrs *)ctx)			{				LWIP_DEBUGF(MACOS_STATE, ("ppp_link_status_cb PPPERR__NONE -connect %d\n", err_code));			}			else			{				LWIP_DEBUGF(MACOS_STATE, ("ppp_link_status_cb PPPERR__NONE %d\n", err_code));			}			break;		}		case PPPERR_PARAM: {           /* Invalid parameter. */			LWIP_DEBUGF(MACOS_DEBUG, ("pppLinkStatusCallback: PPPERR_PARAM\n"));			break;		}		case PPPERR_OPEN: {            /* Unable to open PPP session. */			LWIP_DEBUGF(MACOS_DEBUG, ("pppLinkStatusCallback: PPPERR_OPEN\n"));			break;		}		case PPPERR_DEVICE: {          /* Invalid I/O device for PPP. */			LWIP_DEBUGF(MACOS_DEBUG, ("pppLinkStatusCallback: PPPERR_DEVICE\n"));			break;		}		case PPPERR_ALLOC: {           /* Unable to allocate resources. */			LWIP_DEBUGF(MACOS_DEBUG, ("pppLinkStatusCallback: PPPERR_ALLOC\n"));			break;		}		case PPPERR_USER: {            /* User interrupt. */			LWIP_DEBUGF(MACOS_DEBUG, ("pppLinkStatusCallback: PPPERR_USER\n"));			break;		}		case PPPERR_CONNECT: {         /* Connection lost. */			LWIP_DEBUGF(MACOS_DEBUG, ("pppLinkStatusCallback: PPPERR_CONNECT\n"));			break;		}		case PPPERR_AUTHFAIL: {        /* Failed authentication challenge. */			LWIP_DEBUGF(MACOS_DEBUG, ("pppLinkStatusCallback: PPPERR_AUTHFAIL\n"));			break;		}		case PPPERR_PROTOCOL: {        /* Failed to meet protocol. */			LWIP_DEBUGF(MACOS_DEBUG, ("pppLinkStatusCallback: PPPERR_PROTOCOL\n"));			break;		}		default: {			LWIP_DEBUGF((MACOS_DEBUG | LWIP_DBG_LEVEL_WARNING), ("pppLinkStatusCallback: unknown errCode %d\n", err_code));			break;		}	}	MACOS_DLOGF(MACOS_STATE, ("\n"));}#endif /*PPP_SUPPORT*/#pragma segment Mainvoid test_stop(){#if PPP_SUPPORT	if (ppps >= 0)	{		LWIP_DEBUGF(MACOS_STATE, ("Exiting on Schedule...\n"));		if (((long)FreeMem() < MIN_FREE_MEM) && apps_started)			LWIP_ERROR("Insufficent Free Memory to terminate PPP connection before Quitting.\n", false, return;);		LWIP_DEBUGF(MACOS_STATE, ("Still up...\n"));		ppp_close(ppps);		sys_msleep(500);	}	sio_close(ppp_sio);	ppp_sio = NULL;#endif#if SER_DEBUG	sio_close(debug_sio);	debug_sio = NULL;#endif#ifndef TEST_MAIN_DISABLE	MACOS_DLOGF(MACOS_STATE, ("Exiting on Schedule...\n"));	exit(0);#endif}voidtest_restart(void){	LWIP_DEBUGF(MACOS_TRACE, ("test_restart()\n"));	test_stop();	test_init();	}voidtest_poll(void){    u32_t ticks;	u32_t delta;	struct test_timer *curr_timer;#if LWIP_HAVE_SLIPIF    slipif_poll(&slipif);#endif#if PPP_SUPPORT	ppp_poll();	#endif    // Process timers	ticks = sys_now();	delta = ticks - last_ticks;	last_ticks = ticks;		// The following check skips a potential wrap-around in the	// system ticks counter.	// It also skips when sys_now hasn't incremented due to 16 ms passing per clock tick.    if (delta > 0)	{        for (curr_timer = test_timers; curr_timer->timed_func != NULL; curr_timer++)		{/*			if (curr_timer->active_phase == ppp_phase || curr_timer->active_phase == 0xFF )			{*/				curr_timer->time += delta;				if (curr_timer->time >= curr_timer->interval)				{					curr_timer->timed_func();					curr_timer->time -= curr_timer->interval;				}//			}        }    }}#if SER_DEBUGvoid ser_debug_print( const char *fmt, ...){	OSErr error;	int len;	unsigned char buffer[128];	va_list args;		va_start(args, fmt);	len = vsprintf((char *)buffer, fmt, args);	if (debug_sio)		error = sio_write(debug_sio, buffer, len);	/*else		printf((char *)buffer);*/			va_end(args);}#endif/* Check if connection is up yet. */#if PPP_SUPPORTvoidtest_intf(void){#if	LWIP_TCP				if ( !ppps->if_up )	{		MACOS_DLOGF(MACOS_STATE, ("PPP stuck at Phase %d.\n Check your connection and click 'OK' to renegotiate.\n", ppps->phase));		apps_started = false;		test_restart();	}	else if ( !apps_started )	{		/*	Start up the TCP apps here, because if we do it in the link status callback, 		the stack is too big, and there isn't enough memory available. */ 		Size grow;				UnloadSeg((Ptr) sio_open);		MaxMem(&grow);				LWIP_DEBUGF(MACOS_STATE, ("Free Mem: %lu \n", (long)FreeMem() ));		LWIP_DEBUGF(MACOS_STATE, ("Max Memory: %lu \n", grow));				LWIP_DEBUGF(MACOS_STATE, ("Starting Apps.\n"));				apps_init();		apps_started = true;	}	LWIP_DEBUGF(MACOS_STATE, ("App Limit: %lu \n", (long)GetApplLimit() ));	LWIP_DEBUGF(MACOS_STATE, ("Free Memory: %lu \n", (long)FreeMem() ));#endif}voidppp_poll(void){	u8_t buffer[MACOS_SIO_BUFF_SIZE];	int len;		len = sio_read(ppp_sio, buffer, MACOS_SIO_BUFF_SIZE);	if (len < 0)	{		LWIP_DEBUGF((MACOS_DEBUG | LWIP_DBG_LEVEL_SERIOUS),  ("Error reading from serial port: %d", len));	} else if (len > 0)	{		/*int i;		LWIP_DEBUGF(MACOS_DEBUG, ("ppp_poll(): got %d bytes: ", len));		for (i=0; i<len; i++) {  LWIP_DEBUGF(MACOS_DEBUG, ("%02x ", buffer[i]));}		LWIP_DEBUGF(MACOS_DEBUG, ("\n", len));*/		pppos_input(ppps, buffer, len);	} else	{		sys_msleep(1);	}}#endif