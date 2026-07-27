//========= Copyright Valve Corporation ============//
// SimpleHMD device with custom headset window component
//

#include "hmd_device.h"
#include "hmd_device_component.h"
#include "driverlog.h"

// Static settings
static const char * const k_pch_SimpleHMD_Section = "driver_simplehmd";
static const char * const k_pch_SimpleHMD_SerialNumber_String = "serialNumber";
static const char * const k_pch_SimpleHMD_ModelNumber_String = "modelNumber";

CSimpleHMD::CSimpleHMD()
	: m_unObjectId( vr::k_unTrackedDeviceIndexInvalid )
	, m_pHeadsetWindow( NULL )
{
	// Get configuration from settings
	vr::VRSettings()->GetString( 
		k_pch_SimpleHMD_Section,
		k_pch_SimpleHMD_SerialNumber_String, 
		m_rchSerialNumber, 
		sizeof( m_rchSerialNumber ) 
	);

	vr::VRSettings()->GetString( 
		k_pch_SimpleHMD_Section,
		k_pch_SimpleHMD_ModelNumber_String, 
		m_rchModelNumber, 
		sizeof( m_rchModelNumber ) 
	);

	// Ensure we have defaults
	if ( m_rchSerialNumber[ 0 ] == 0 )
		strncpy_s( m_rchSerialNumber, sizeof( m_rchSerialNumber ), "SimpleHMD-123456", _TRUNCATE );

	if ( m_rchModelNumber[ 0 ] == 0 )
		strncpy_s( m_rchModelNumber, sizeof( m_rchModelNumber ), "SimpleHMD", _TRUNCATE );

	DriverLog( "CSimpleHMD created: Serial=%s, Model=%s", m_rchSerialNumber, m_rchModelNumber );
}

CSimpleHMD::~CSimpleHMD()
{
	if ( m_pHeadsetWindow )
	{
		delete m_pHeadsetWindow;
		m_pHeadsetWindow = NULL;
	}
}

vr::EVRInitError CSimpleHMD::Activate( uint32_t unObjectId )
{
	DriverLog( "CSimpleHMD::Activate( %d )", unObjectId );

	m_unObjectId = unObjectId;

	// Get property container
	vr::PropertyContainerHandle_t ulContainer = 
		vr::VRProperties()->TrackedDeviceToPropertyContainer( unObjectId );

	// Set HMD properties
	vr::VRProperties()->SetStringProperty( ulContainer,
		vr::Prop_ModelNumber_String, m_rchModelNumber );
	
	vr::VRProperties()->SetFloatProperty( ulContainer,
		vr::Prop_SecondsFromVsyncToPhotons_Float, 0.0f );

	vr::VRProperties()->SetBoolProperty( ulContainer,
		vr::Prop_HasVirtualDisplayComponent_Bool, true );

	// Set display properties
	vr::VRProperties()->SetInt32Property( ulContainer,
		vr::Prop_DisplayMCType_Int32, 0 );

	// Create and initialize headset window component
	m_pHeadsetWindow = new CHeadsetWindowComponent();
	if ( !m_pHeadsetWindow->Initialize() )
	{
		DriverLog( "Failed to initialize headset window component" );
		delete m_pHeadsetWindow;
		m_pHeadsetWindow = NULL;
		return vr::VRInitError_Driver_Failed;
	}

	DriverLog( "CSimpleHMD activated successfully" );
	return vr::VRInitError_None;
}

void CSimpleHMD::Deactivate()
{
	DriverLog( "CSimpleHMD::Deactivate()" );

	if ( m_pHeadsetWindow )
	{
		m_pHeadsetWindow->Shutdown();
		delete m_pHeadsetWindow;
		m_pHeadsetWindow = NULL;
	}

	m_unObjectId = vr::k_unTrackedDeviceIndexInvalid;
}

void CSimpleHMD::EnterStandby()
{
	DriverLog( "CSimpleHMD::EnterStandby()" );
}

void *CSimpleHMD::GetComponent( const char *pchComponentNameAndVersion )
{
	// Return our virtual display component
	if ( !_stricmp( pchComponentNameAndVersion, vr::IVRVirtualDisplay_Version ) )
	{
		DriverLog( "Returning IVRVirtualDisplay component" );
		return m_pHeadsetWindow;
	}

	return NULL;
}

void CSimpleHMD::DebugRequest( const char *pchRequest, char *pchResponseBuffer, uint32_t unResponseBufferSize )
{
	if ( unResponseBufferSize >= 1 )
		pchResponseBuffer[ 0 ] = 0;
}

vr::DriverPose_t CSimpleHMD::GetPose()
{
	vr::DriverPose_t pose = { 0 };
	pose.poseIsValid = true;
	pose.result = vr::TrackingResult_Running_OK;
	pose.deviceIsConnected = true;
	pose.qWorldFromDriverRotation.w = 1;
	pose.qWorldFromDriverRotation.x = 0;
	pose.qWorldFromDriverRotation.y = 0;
	pose.qWorldFromDriverRotation.z = 0;
	pose.qDriverFromHeadRotation.w = 1;
	pose.qDriverFromHeadRotation.x = 0;
	pose.qDriverFromHeadRotation.y = 0;
	pose.qDriverFromHeadRotation.z = 0;
	return pose;
}

std::string CSimpleHMD::GetSerialNumber()
{
	return m_rchSerialNumber;
}
