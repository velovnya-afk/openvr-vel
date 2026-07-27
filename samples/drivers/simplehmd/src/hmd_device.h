//========= Copyright Valve Corporation ============//
// SimpleHMD device with custom headset window component
//

#pragma once

#include "openvr_driver.h"

class CHeadsetWindowComponent;

class CSimpleHMD : public vr::ITrackedDeviceServerDriver
{
public:
	CSimpleHMD();
	virtual ~CSimpleHMD();

	// ITrackedDeviceServerDriver
	virtual vr::EVRInitError Activate( uint32_t unObjectId ) override;
	virtual void Deactivate() override;
	virtual void EnterStandby() override;
	virtual void *GetComponent( const char *pchComponentNameAndVersion ) override;
	virtual void DebugRequest( const char *pchRequest, char *pchResponseBuffer, uint32_t unResponseBufferSize ) override;
	virtual vr::DriverPose_t GetPose() override;

	// Custom methods
	std::string GetSerialNumber();

private:
	// Member variables
	uint32_t m_unObjectId;
	char m_rchSerialNumber[ 1024 ];
	char m_rchModelNumber[ 1024 ];

	// Component
	CHeadsetWindowComponent *m_pHeadsetWindow;
};
