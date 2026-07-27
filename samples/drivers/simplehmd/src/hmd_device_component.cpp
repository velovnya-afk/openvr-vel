//========= Copyright Valve Corporation ============//
// Custom Headset Window Component using IVRVirtualDisplay
//

#include "hmd_device_component.h"
#include "driverlog.h"
#include <chrono>

// Window class name
static const wchar_t *k_pchHeadsetWindowClassName = L"SimpleHMDHeadsetWindow";
static const wchar_t *k_pchHeadsetWindowTitle = L"SimpleHMD Headset View";

// Static map to find component from window handle
static std::map< HWND, CHeadsetWindowComponent * > g_windowMap;
static std::mutex g_windowMapMutex;

CHeadsetWindowComponent::CHeadsetWindowComponent()
	: m_hHeadsetWindow( NULL )
	, m_pDevice( NULL )
	, m_pContext( NULL )
	, m_pSwapChain( NULL )
	, m_pRenderTargetView( NULL )
	, m_flLastVsyncTimeInSeconds( 0.0 )
	, m_nVsyncCounter( 0 )
	, m_bExiting( false )
{
}

CHeadsetWindowComponent::~CHeadsetWindowComponent()
{
	Shutdown();
}

bool CHeadsetWindowComponent::Initialize()
{
	DriverLog( "CHeadsetWindowComponent::Initialize()" );

	// Initialize D3D11
	if ( !InitializeD3D11() )
	{
		DriverLog( "Failed to initialize D3D11" );
		return false;
	}

	// Create headset window
	if ( !CreateHeadsetWindow() )
	{
		DriverLog( "Failed to create headset window" );
		ShutdownD3D11();
		return false;
	}

	// Initialize timing
	m_flLastVsyncTimeInSeconds = GetCurrentTime();
	m_nVsyncCounter = 0;

	DriverLog( "CHeadsetWindowComponent initialized successfully" );
	return true;
}

void CHeadsetWindowComponent::Shutdown()
{
	DriverLog( "CHeadsetWindowComponent::Shutdown()" );

	m_bExiting = true;

	DestroyHeadsetWindow();

	// Clean up cached textures
	{
		std::lock_guard< std::mutex > lock( m_mutex );
		for ( auto &pair : m_cachedTextures )
		{
			if ( pair.second )
				pair.second->Release();
		}
		m_cachedTextures.clear();
	}

	ShutdownD3D11();
}

bool CHeadsetWindowComponent::InitializeD3D11()
{
	DriverLog( "Initializing D3D11..." );

	D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
	D3D_FEATURE_LEVEL supportedFeatureLevel = D3D_FEATURE_LEVEL_11_0;

	HRESULT hr = D3D11CreateDevice(
		NULL,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		D3D11_CREATE_DEVICE_DEBUG,
		featureLevels,
		1,
		D3D11_SDK_VERSION,
		&m_pDevice,
		&supportedFeatureLevel,
		&m_pContext
	);

	if ( FAILED( hr ) )
	{
		DriverLog( "D3D11CreateDevice failed: 0x%08x", hr );
		return false;
	}

	DriverLog( "D3D11 initialized successfully" );
	return true;
}

void CHeadsetWindowComponent::ShutdownD3D11()
{
	if ( m_pRenderTargetView )
	{
		m_pRenderTargetView->Release();
		m_pRenderTargetView = NULL;
	}

	if ( m_pSwapChain )
	{
		m_pSwapChain->Release();
		m_pSwapChain = NULL;
	}

	if ( m_pContext )
	{
		m_pContext->Release();
		m_pContext = NULL;
	}

	if ( m_pDevice )
	{
		m_pDevice->Release();
		m_pDevice = NULL;
	}
}

bool CHeadsetWindowComponent::CreateHeadsetWindow()
{
	DriverLog( "Creating headset window..." );

	// Register window class
	WNDCLASSW wc = {};
	wc.lpfnWndProc = CHeadsetWindowComponent::WndProc;
	wc.lpszClassName = k_pchHeadsetWindowClassName;
	wc.style = CS_OWNDC;

	if ( !RegisterClassW( &wc ) )
	{
		DriverLog( "RegisterClassW failed: %d", GetLastError() );
		return false;
	}

	// Create window
	m_hHeadsetWindow = CreateWindowW(
		k_pchHeadsetWindowClassName,
		k_pchHeadsetWindowTitle,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		1280,
		720,
		NULL,
		NULL,
		NULL,
		this
	);

	if ( !m_hHeadsetWindow )
	{
		DriverLog( "CreateWindowW failed: %d", GetLastError() );
		return false;
	}

	// Register this component for the window
	{
		std::lock_guard< std::mutex > lock( g_windowMapMutex );
		g_windowMap[ m_hHeadsetWindow ] = this;
	}

	// Show window
	ShowWindow( m_hHeadsetWindow, SW_SHOW );
	UpdateWindow( m_hHeadsetWindow );

	// Create swap chain for this window
	IDXGIFactory *pFactory = NULL;
	IDXGIAdapter *pAdapter = NULL;

	IDXGIDevice *pDXGIDevice = NULL;
	if ( FAILED( m_pDevice->QueryInterface( __uuidof( IDXGIDevice ), (void **)&pDXGIDevice ) ) )
	{
		DriverLog( "Failed to get IDXGIDevice" );
		return false;
	}

	if ( FAILED( pDXGIDevice->GetAdapter( &pAdapter ) ) )
	{
		DriverLog( "Failed to get adapter" );
		pDXGIDevice->Release();
		return false;
	}

	if ( FAILED( pAdapter->GetParent( __uuidof( IDXGIFactory ), (void **)&pFactory ) ) )
	{
		DriverLog( "Failed to get factory" );
		pAdapter->Release();
		pDXGIDevice->Release();
		return false;
	}

	DXGI_SWAP_CHAIN_DESC desc = {};
	desc.BufferCount = 1;
	desc.BufferDesc.Width = 1280;
	desc.BufferDesc.Height = 720;
	desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.BufferDesc.RefreshRate.Numerator = 90;
	desc.BufferDesc.RefreshRate.Denominator = 1;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.OutputWindow = m_hHeadsetWindow;
	desc.SampleDesc.Count = 1;
	desc.Windowed = TRUE;

	HRESULT hr = pFactory->CreateSwapChain( m_pDevice, &desc, &m_pSwapChain );

	pFactory->Release();
	pAdapter->Release();
	pDXGIDevice->Release();

	if ( FAILED( hr ) )
	{
		DriverLog( "CreateSwapChain failed: 0x%08x", hr );
		return false;
	}

	// Create render target view
	ID3D11Texture2D *pBackBuffer = NULL;
	if ( FAILED( m_pSwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), (void **)&pBackBuffer ) ) )
	{
		DriverLog( "GetBuffer failed" );
		return false;
	}

	if ( FAILED( m_pDevice->CreateRenderTargetView( pBackBuffer, NULL, &m_pRenderTargetView ) ) )
	{
		DriverLog( "CreateRenderTargetView failed" );
		pBackBuffer->Release();
		return false;
	}

	pBackBuffer->Release();

	DriverLog( "Headset window created successfully" );
	return true;
}

void CHeadsetWindowComponent::DestroyHeadsetWindow()
{
	if ( m_hHeadsetWindow )
	{
		{
			std::lock_guard< std::mutex > lock( g_windowMapMutex );
			g_windowMap.erase( m_hHeadsetWindow );
		}

		DestroyWindow( m_hHeadsetWindow );
		m_hHeadsetWindow = NULL;
	}
}

LRESULT CALLBACK CHeadsetWindowComponent::WndProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	CHeadsetWindowComponent *pThis = NULL;

	if ( msg == WM_CREATE )
	{
		CREATESTRUCT *pCreate = (CREATESTRUCT *)lParam;
		pThis = (CHeadsetWindowComponent *)pCreate->lpCreateParams;
		SetWindowLongPtrW( hwnd, GWLP_USERDATA, (LONG_PTR)pThis );
	}
	else
	{
		pThis = (CHeadsetWindowComponent *)GetWindowLongPtrW( hwnd, GWLP_USERDATA );
	}

	if ( pThis )
	{
		switch ( msg )
		{
		case WM_CLOSE:
			PostQuitMessage( 0 );
			return 0;

		case WM_PAINT:
			{
				PAINTSTRUCT ps;
				HDC hdc = BeginPaint( hwnd, &ps );
				// Simple paint - clear to black
				FillRect( hdc, &ps.rcPaint, (HBRUSH)GetStockObject( BLACK_BRUSH ) );
				EndPaint( hwnd, &ps );
			}
			return 0;

		case WM_SIZE:
			// Window resized - could recreate swap chain here
			return 0;
		}
	}

	return DefWindowProcW( hwnd, msg, wParam, lParam );
}

ID3D11Texture2D *CHeadsetWindowComponent::GetSharedTexture( HANDLE sharedHandle )
{
	std::lock_guard< std::mutex > lock( m_mutex );

	auto it = m_cachedTextures.find( sharedHandle );
	if ( it != m_cachedTextures.end() )
	{
		return it->second;
	}

	// Open shared texture
	ID3D11Texture2D *pTexture = NULL;
	HRESULT hr = m_pDevice->OpenSharedResource(
		sharedHandle,
		__uuidof( ID3D11Texture2D ),
		(void **)&pTexture
	);

	if ( FAILED( hr ) )
	{
		DriverLog( "OpenSharedResource failed: 0x%08x", hr );
		return NULL;
	}

	m_cachedTextures[ sharedHandle ] = pTexture;
	return pTexture;
}

void CHeadsetWindowComponent::RenderFrame( ID3D11Texture2D *pTexture )
{
	if ( !pTexture || !m_pContext || !m_pRenderTargetView )
		return;

	// Get texture dimensions
	D3D11_TEXTURE2D_DESC desc;
	pTexture->GetDesc( &desc );

	// Set render target
	m_pContext->OMSetRenderTargets( 1, &m_pRenderTargetView, NULL );

	// Set viewport
	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = (float)desc.Width;
	viewport.Height = (float)desc.Height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	m_pContext->RSSetViewports( 1, &viewport );

	// Copy texture to render target
	m_pContext->CopySubresourceRegion( 
		m_pRenderTargetView,
		0,
		0,
		0,
		0,
		pTexture,
		0,
		NULL
	);

	// Present to screen
	if ( m_pSwapChain )
	{
		m_pSwapChain->Present( 1, 0 );
	}
}

void CHeadsetWindowComponent::Present( const vr::PresentInfo_t *pPresentInfo, uint32_t unPresentInfoSize )
{
	if ( !pPresentInfo )
		return;

	DriverLog( "CHeadsetWindowComponent::Present() - backbufferTextureHandle: %p", 
		(void*)pPresentInfo->backbufferTextureHandle );

	// Get the shared texture
	ID3D11Texture2D *pTexture = GetSharedTexture( (HANDLE)pPresentInfo->backbufferTextureHandle );
	if ( !pTexture )
	{
		DriverLog( "Failed to get shared texture" );
		return;
	}

	// Use keyed mutex for synchronization if available
	IDXGIKeyedMutex *pKeyedMutex = NULL;
	if ( SUCCEEDED( pTexture->QueryInterface( __uuidof( IDXGIKeyedMutex ), (void **)&pKeyedMutex ) ) )
	{
		if ( pKeyedMutex->AcquireSync( 0, 10 ) == S_OK )
		{
			RenderFrame( pTexture );
			pKeyedMutex->ReleaseSync( 0 );
		}
		pKeyedMutex->Release();
	}
	else
	{
		// Render without mutex
		RenderFrame( pTexture );
	}

	// Update timing
	m_flLastVsyncTimeInSeconds = pPresentInfo->flVSyncTimeInSeconds;
}

void CHeadsetWindowComponent::WaitForPresent()
{
	// Process window messages to keep the window responsive
	if ( m_hHeadsetWindow )
	{
		MSG msg;
		while ( PeekMessageW( &msg, m_hHeadsetWindow, 0, 0, PM_REMOVE ) )
		{
			TranslateMessage( &msg );
			DispatchMessageW( &msg );
		}
	}

	// Update vsync counter
	m_nVsyncCounter++;
}

bool CHeadsetWindowComponent::GetTimeSinceLastVsync( float *pfSecondsSinceLastVsync, uint64_t *pulFrameCounter )
{
	if ( !pfSecondsSinceLastVsync || !pulFrameCounter )
		return false;

	double flCurrentTime = GetCurrentTime();
	*pfSecondsSinceLastVsync = (float)(flCurrentTime - m_flLastVsyncTimeInSeconds);
	*pulFrameCounter = m_nVsyncCounter;

	return true;
}

double CHeadsetWindowComponent::GetCurrentTime()
{
	auto now = std::chrono::high_resolution_clock::now();
	auto duration = now.time_since_epoch();
	auto seconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() / 1e9;
	return seconds;
}
