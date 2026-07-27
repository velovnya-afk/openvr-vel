//========= Copyright Valve Corporation ============//
// SimpleHMD server driver - entry point
//

#include "openvr_driver.h"
#include "hmd_device.h"
#include "driverlog.h"

#include <vector>

static CSimpleHMD *g_pSimpleHMD = NULL;

class CServerDriver_SimpleHMD : public vr::IServerTrackedDeviceProvider
{
public:
	virtual vr::EVRInitError Init( vr::IVRDriverContext *pDriverContext ) override
	{
		VR_INIT_SERVER_DRIVER_CONTEXT( pDriverContext );

		DriverLog( "CServerDriver_SimpleHMD::Init()" );

		g_pSimpleHMD = new CSimpleHMD();

		vr::VRServerDriverHost()->TrackedDeviceAdded(
			g_pSimpleHMD->GetSerialNumber().c_str(),
			vr::TrackedDeviceClass_HMD,
			g_pSimpleHMD
		);

		return vr::VRInitError_None;
	}

	virtual void Cleanup() override
	{
		DriverLog( "CServerDriver_SimpleHMD::Cleanup()" );

		if ( g_pSimpleHMD )
		{
			delete g_pSimpleHMD;
			g_pSimpleHMD = NULL;
		}

		VR_CLEANUP_SERVER_DRIVER_CONTEXT();
	}

	virtual const char * const *GetInterfaceVersions() override
	{
		return vr::k_InterfaceVersions;
	}

	virtual void RunFrame() override
	{
		if ( g_pSimpleHMD )
		{
			// Update pose if needed
			vr::DriverPose_t pose = g_pSimpleHMD->GetPose();
			vr::VRServerDriverHost()->TrackedDevicePoseUpdated( 0, pose, sizeof( pose ) );
		}
	}

	virtual bool ShouldBlockStandbyMode() override
	{
		return false;
	}

	virtual void EnterStandby() override
	{
	}

	virtual void LeaveStandby() override
	{
	}
};

static CServerDriver_SimpleHMD g_serverDriverSimpleHMD;

extern "C" __declspec( dllexport )
void *HmdDriverFactory( const char *pInterfaceName, int *pReturnCode )
{
	if ( 0 == strcmp( vr::IServerTrackedDeviceProvider_Version, pInterfaceName ) )
	{
		return &g_serverDriverSimpleHMD;
	}

	if ( pReturnCode )
		*pReturnCode = vr::VRInitError_Init_InterfaceNotFound;

	return NULL;
}
