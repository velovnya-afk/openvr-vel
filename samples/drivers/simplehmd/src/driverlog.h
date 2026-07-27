#pragma once

#include "openvr_driver.h"

extern void InitDriverLog( vr::IVRDriverLog *pDriverLog );
extern void DriverLog( const char *pchFormat, ... );
