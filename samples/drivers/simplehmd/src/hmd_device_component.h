//========= Copyright Valve Corporation ============//
// Custom Headset Window Component using IVRVirtualDisplay
//

#pragma once

#include "openvr_driver.h"
#include <windows.h>
#include <d3d11.h>
#include <thread>
#include <mutex>
#include <map>

class CHeadsetWindowComponent : public vr::IVRVirtualDisplay
{
public:
	CHeadsetWindowComponent();
	virtual ~CHeadsetWindowComponent();

	// Initialize the headset window
	bool Initialize();

	// Shutdown
	void Shutdown();

	// IVRVirtualDisplay
	virtual void Present( const vr::PresentInfo_t *pPresentInfo, uint32_t unPresentInfoSize ) override;
	virtual void WaitForPresent() override;
	virtual bool GetTimeSinceLastVsync( float *pfSecondsSinceLastVsync, uint64_t *pulFrameCounter ) override;

private:
	// Window management
	static LRESULT CALLBACK WndProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam );
	bool CreateHeadsetWindow();
	void DestroyHeadsetWindow();

	// Frame capture and rendering
	void RenderFrame( ID3D11Texture2D *pTexture );
	ID3D11Texture2D *GetSharedTexture( HANDLE sharedHandle );

	// D3D11 management
	bool InitializeD3D11();
	void ShutdownD3D11();

	// Timing
	double GetCurrentTime();

	// Member variables
	HWND m_hHeadsetWindow;
	ID3D11Device *m_pDevice;
	ID3D11DeviceContext *m_pContext;
	IDXGISwapChain *m_pSwapChain;
	ID3D11RenderTargetView *m_pRenderTargetView;

	// Timing info
	double m_flLastVsyncTimeInSeconds;
	uint64_t m_nVsyncCounter;
	
	// Synchronization
	std::mutex m_mutex;
	std::thread m_renderThread;
	bool m_bExiting;
	
	// Texture cache
	std::map< HANDLE, ID3D11Texture2D * > m_cachedTextures;
};
