//========= Copyright Valve Corporation ============//
// Example HMD driver for demonstrating custom headset window with IVRVirtualDisplay
//

#include "driverlog.h"
#include <stdio.h>
#include <stdarg.h>

static vr::IVRDriverLog *g_pDriverLog = NULL;

void InitDriverLog( vr::IVRDriverLog *pDriverLog )
{
	g_pDriverLog = pDriverLog;
}

void DriverLog( const char *pchFormat, ... )
{
	va_list args;
	va_start( args, pchFormat );

	char buffer[ 1024 ];
	vsnprintf( buffer, sizeof( buffer ), pchFormat, args );
	strcat( buffer, "\n" );

	if ( g_pDriverLog )
		g_pDriverLog->Log( buffer );

	va_end( args );
}
