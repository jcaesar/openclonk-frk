/*
 * OpenClonk, http://www.openclonk.org
 *
 * Copyright (c) 2005-2006, 2008-2011  Günther Brammer
 * Copyright (c) 2005, 2007, 2009  Peter Wortmann
 * Copyright (c) 2002, 2005-2007  Sven Eberhardt
 * Copyright (c) 2006, 2010  Armin Burgmeier
 * Copyright (c) 2009-2011  Nicolas Hake
 * Copyright (c) 2010  Benjamin Herr
 * Copyright (c) 2010  Martin Plicht
 * Copyright (c) 2001-2009, RedWolf Design GmbH, http://www.clonk.de
 *
 * Portions might be copyrighted by other authors who have contributed
 * to OpenClonk.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * See isc_license.txt for full license and disclaimer.
 *
 * "Clonk" is a registered trademark of Matthes Bender.
 * See clonk_trademark_license.txt for full license.
 */

/* A wrapper class to OS dependent event and window interfaces, WIN32 version */

#include "C4Include.h"
#include <C4Window.h>

#include <C4Application.h>
#include <C4AppWin32Impl.h>
#include <C4Config.h>
#include <C4Console.h>
#include <C4DrawGL.h>
#include <C4DrawD3D.h>
#include <C4FullScreen.h>
#include <C4GraphicsSystem.h>
#include <C4MouseControl.h>
#include <C4Rect.h>
#include <C4Version.h>
#include <C4Viewport.h>
#include <C4ViewportWindow.h>
#include <StdRegistry.h>
#include "resource.h"

#include <C4windowswrapper.h>
#include <mmsystem.h>
#include <shellapi.h>
// multimon.h comes with DirectX, some people don't have DirectX.
#ifdef HAVE_MULTIMON_H

// Lets try this unconditionally so that older windowses get the benefit
// even if the engine was compiled with a newer sdk. Or something.
#define COMPILE_MULTIMON_STUBS
#include <multimon.h>

#endif

#define C4ViewportClassName L"C4Viewport"
#define C4FullScreenClassName L"C4FullScreen"
#define ConsoleDlgClassName L"C4GUIdlg"
#define ConsoleDlgWindowStyle (WS_VISIBLE | WS_POPUP | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX)

LRESULT APIENTRY FullScreenWinProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static bool NativeCursorShown = true;
	// Process message
	switch (uMsg)
	{
	case WM_ACTIVATE:
		wParam = (LOWORD(wParam)==WA_ACTIVE || LOWORD(wParam)==WA_CLICKACTIVE);
		// fall through to next case
	case WM_ACTIVATEAPP:
		Application.Active = wParam != 0;
#ifdef USE_GL
		if (pGL)
		{
			if (Application.Active)
			{
				// restore textures
				if (pTexMgr) pTexMgr->IntUnlock();
				if (!Config.Graphics.Windowed)
				{
					Application.SetVideoMode(Config.Graphics.ResX, Config.Graphics.ResY, Config.Graphics.BitDepth, Config.Graphics.RefreshRate, Config.Graphics.Monitor, !Config.Graphics.Windowed);
				}
			}
			else
			{
				if (pTexMgr)
					pTexMgr->IntLock();
				if (pGL)
					pGL->TaskOut();
				if (!Config.Graphics.Windowed)
				{
					::ChangeDisplaySettings(NULL, 0);
					::ShowWindow(hwnd, SW_MINIMIZE);
				}
			}
		}
#endif
#ifdef USE_DIRECTX
		if (pD3D && Application.Active)
			pD3D->TaskIn();
		if (pD3D && !Application.Active)
			pD3D->TaskOut();
#endif
		// redraw background
		::GraphicsSystem.InvalidateBg();
		// Redraw after task switch
		if (Application.Active)
			::GraphicsSystem.Execute();
		// update cursor clip
		::MouseControl.UpdateClip();
		return false;
	case WM_PAINT:
		// Redraw after task switch
		if (Application.Active)
			::GraphicsSystem.Execute();
		break;
	case WM_DESTROY:
		Application.Quit();
		return 0;
	case WM_CLOSE:
		FullScreen.Close();
		return 0;
	case MM_MCINOTIFY:
		if (wParam == MCI_NOTIFY_SUCCESSFUL)
			Application.MusicSystem.NotifySuccess();
		return true;
	case WM_KEYUP:
		if (Game.DoKeyboardInput(wParam, KEYEV_Up, !!(lParam & 0x20000000), GetKeyState(VK_CONTROL) < 0, GetKeyState(VK_SHIFT) < 0, false, NULL))
			return 0;
		break;
	case WM_KEYDOWN:
		if (Game.DoKeyboardInput(wParam, KEYEV_Down, !!(lParam & 0x20000000), GetKeyState(VK_CONTROL) < 0, GetKeyState(VK_SHIFT) < 0, !!(lParam & 0x40000000), NULL))
			return 0;
		break;
	case WM_SYSKEYDOWN:
		if (wParam == 18) break;
		if (Game.DoKeyboardInput(wParam, KEYEV_Down, !!(lParam & 0x20000000), GetKeyState(VK_CONTROL) < 0, GetKeyState(VK_SHIFT) < 0, !!(lParam & 0x40000000), NULL))
			return 0;
		break;
	case WM_CHAR:
	{
		// UTF-8 has 1 to 4 data bytes, and we need a terminating \0
		char c[5] = {0};
		if(!WideCharToMultiByte(CP_UTF8, 0L, reinterpret_cast<LPCWSTR>(&wParam), 1, c, 4, 0, 0))
			return 0;
		// GUI: forward
		if (::pGUI->CharIn(c))
			return 0;
		return false;
	}
	case WM_USER_LOG:
		if (SEqual2((const char *)lParam, "IDS_"))
			Log(LoadResStr((const char *)lParam));
		else
			Log((const char *)lParam);
		return false;
	case WM_LBUTTONDOWN:
		C4GUI::MouseMove(C4MC_Button_LeftDown,LOWORD(lParam),HIWORD(lParam),wParam, NULL);
		break;
	case WM_LBUTTONUP: C4GUI::MouseMove(C4MC_Button_LeftUp,LOWORD(lParam),HIWORD(lParam),wParam, NULL); break;
	case WM_RBUTTONDOWN: C4GUI::MouseMove(C4MC_Button_RightDown,LOWORD(lParam),HIWORD(lParam),wParam, NULL); break;
	case WM_RBUTTONUP: C4GUI::MouseMove(C4MC_Button_RightUp,LOWORD(lParam),HIWORD(lParam),wParam, NULL); break;
	case WM_LBUTTONDBLCLK: C4GUI::MouseMove(C4MC_Button_LeftDouble,LOWORD(lParam),HIWORD(lParam),wParam, NULL); break;
	case WM_RBUTTONDBLCLK: C4GUI::MouseMove(C4MC_Button_RightDouble,LOWORD(lParam),HIWORD(lParam),wParam, NULL); break;
	case WM_MOUSEWHEEL: C4GUI::MouseMove(C4MC_Button_Wheel,LOWORD(lParam),HIWORD(lParam),wParam, NULL); break;
	case WM_MOUSEMOVE:
		C4GUI::MouseMove(C4MC_Button_None,LOWORD(lParam),HIWORD(lParam),wParam, NULL);
		// Hide cursor in client area
		if (NativeCursorShown)
		{
			NativeCursorShown = false;
			ShowCursor(FALSE);
		}
		break;
	case WM_NCMOUSEMOVE:
		// Show cursor on window frame
		if (!NativeCursorShown)
		{
			NativeCursorShown = true;
			ShowCursor(TRUE);
		}
		break;
	case WM_SIZE:
		// Notify app about render window size change
		switch (wParam)
		{
		case SIZE_RESTORED:
		case SIZE_MAXIMIZED:
			::Application.OnResolutionChanged(LOWORD(lParam), HIWORD(lParam));
			if(Application.pWindow) // this might be called from C4Window::Init in which case Application.pWindow is not yet set
				::SetWindowPos(Application.pWindow->hRenderWindow, NULL, 0, 0, LOWORD(lParam), HIWORD(lParam), SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOREDRAW | SWP_NOZORDER);
			break;
		}
		break;
	}
	return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

LRESULT APIENTRY ViewportWinProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// Determine viewport
	C4Viewport *cvp;
	if (!(cvp=::Viewports.GetViewport(hwnd)))
		return DefWindowProcW(hwnd, uMsg, wParam, lParam);

	// Process message
	switch (uMsg)
	{
		//---------------------------------------------------------------------------------------------------------------------------
	case WM_KEYDOWN:
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		switch (wParam)
		{
			// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		case VK_SCROLL:
			// key bound to this specific viewport. Don't want to pass this through C4Game...
			cvp->TogglePlayerLock();
			break;
			// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		default:
			if (Game.DoKeyboardInput(wParam, KEYEV_Down, !!(lParam & 0x20000000), GetKeyState(VK_CONTROL) < 0, GetKeyState(VK_SHIFT) < 0, !!(lParam & 0x40000000), NULL)) return 0;
			break;
			// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		}
		break;
		//---------------------------------------------------------------------------------------------------------------------------
	case WM_KEYUP:
		if (Game.DoKeyboardInput(wParam, KEYEV_Up, !!(lParam & 0x20000000), GetKeyState(VK_CONTROL) < 0, GetKeyState(VK_SHIFT) < 0, false, NULL)) return 0;
		break;
		//------------------------------------------------------------------------------------------------------------
	case WM_SYSKEYDOWN:
		if (wParam == 18) break;
		if (Game.DoKeyboardInput(wParam, KEYEV_Down, !!(lParam & 0x20000000), GetKeyState(VK_CONTROL) < 0, GetKeyState(VK_SHIFT) < 0, !!(lParam & 0x40000000), NULL)) return 0;
		break;
		//----------------------------------------------------------------------------------------------------------------------------------
	case WM_DESTROY:
		StoreWindowPosition(hwnd, FormatString("Viewport%i",cvp->Player+1).getData(), Config.GetSubkeyPath("Console"));
		break;
		//----------------------------------------------------------------------------------------------------------------------------------
	case WM_CREATE:
		DragAcceptFiles(hwnd, TRUE);
		break;
	case WM_CLOSE:
		cvp->pWindow->Close();
		break;

		//----------------------------------------------------------------------------------------------------------------------------------
	case WM_DROPFILES:
	{
		HDROP hDrop = (HDROP)(HANDLE) wParam;
		if (!Console.Editing) { Console.Message(LoadResStr("IDS_CNS_NONETEDIT")); return false; }

		int32_t iFileNum = DragQueryFile(hDrop,0xFFFFFFFF,NULL,0);
		POINT pntPoint;
		DragQueryPoint(hDrop,&pntPoint);
		wchar_t szFilename[500+1];
		for (int32_t cnt=0; cnt<iFileNum; cnt++)
		{
			DragQueryFileW(hDrop,cnt,szFilename,500);
			cvp->DropFile(StdStrBuf(szFilename).getData(), (float)pntPoint.x, (float)pntPoint.y);
		}
		DragFinish(hDrop);
		break;
	}
		//----------------------------------------------------------------------------------------------------------------------------------
	case WM_USER_DROPDEF:
		Game.DropDef(C4ID(lParam),cvp->ViewX+float(LOWORD(wParam))/cvp->Zoom,cvp->ViewY+float(HIWORD(wParam)/cvp->Zoom));
		break;
		//----------------------------------------------------------------------------------------------------------------------------------
	case WM_SIZE:
		cvp->UpdateOutputSize();
		break;
		//----------------------------------------------------------------------------------------------------------------------------------
	case WM_PAINT:
		::GraphicsSystem.Execute();
		break;
		//----------------------------------------------------------------------------------------------------------------------------------
	case WM_HSCROLL:
		switch (LOWORD(wParam))
		{
		case SB_THUMBTRACK: case SB_THUMBPOSITION: cvp->ViewX=HIWORD(wParam); break;
		case SB_LINELEFT: cvp->ViewX-=ViewportScrollSpeed; break;
		case SB_LINERIGHT: cvp->ViewX+=ViewportScrollSpeed; break;
		case SB_PAGELEFT: cvp->ViewX-=cvp->ViewWdt; break;
		case SB_PAGERIGHT: cvp->ViewX+=cvp->ViewWdt; break;
		}
		cvp->Execute();
		cvp->ScrollBarsByViewPosition();
		return 0;
		//----------------------------------------------------------------------------------------------------------------------------------
	case WM_VSCROLL:
		switch (LOWORD(wParam))
		{
		case SB_THUMBTRACK: case SB_THUMBPOSITION: cvp->ViewY=HIWORD(wParam); break;
		case SB_LINEUP: cvp->ViewY-=ViewportScrollSpeed; break;
		case SB_LINEDOWN: cvp->ViewY+=ViewportScrollSpeed; break;
		case SB_PAGEUP: cvp->ViewY-=cvp->ViewWdt; break;
		case SB_PAGEDOWN: cvp->ViewY+=cvp->ViewWdt; break;
		}
		cvp->Execute();
		cvp->ScrollBarsByViewPosition();
		return 0;
		//----------------------------------------------------------------------------------------------------------------------------------
	case WM_ACTIVATE:
		// Keep editing dialogs on top of the current viewport, but don't make them
		// float on other windows (i.e., no HWND_TOPMOST).
		// Also, don't use SetParent, since that activates the window, which in turn
		// posts a new WM_ACTIVATE to us, and so on, ultimately leading to a hang.
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			Console.Win32KeepDialogsFloating();
		}
		else
		{
			// FALLTHROUGH
		case WM_MOUSEACTIVATE:
			// WM_MOUSEACTIVATE is emitted when the user hovers over a window and pushes a mouse button.
			// Setting the window owner here avoids z-order flickering.
			Console.Win32KeepDialogsFloating(hwnd);
		}
		break;
		//----------------------------------------------------------------------------------------------------------------------------------
	}

	// Viewport mouse control
	if (::MouseControl.IsViewport(cvp) && (Console.EditCursor.GetMode()==C4CNS_ModePlay))
	{
		switch (uMsg)
		{
			//----------------------------------------------------------------------------------------------------------------------------------
		case WM_LBUTTONDOWN: C4GUI::MouseMove(C4MC_Button_LeftDown,LOWORD(lParam),HIWORD(lParam),wParam, cvp);  break;
			//----------------------------------------------------------------------------------------------------------------------------------
		case WM_LBUTTONUP: C4GUI::MouseMove(C4MC_Button_LeftUp,LOWORD(lParam),HIWORD(lParam),wParam, cvp);  break;
			//----------------------------------------------------------------------------------------------------------------------------------
		case WM_RBUTTONDOWN: C4GUI::MouseMove(C4MC_Button_RightDown,LOWORD(lParam),HIWORD(lParam),wParam, cvp); break;
			//----------------------------------------------------------------------------------------------------------------------------------
		case WM_RBUTTONUP: C4GUI::MouseMove(C4MC_Button_RightUp,LOWORD(lParam),HIWORD(lParam),wParam, cvp); break;
			//----------------------------------------------------------------------------------------------------------------------------------
		case WM_LBUTTONDBLCLK: C4GUI::MouseMove(C4MC_Button_LeftDouble,LOWORD(lParam),HIWORD(lParam),wParam, cvp);  break;
			//----------------------------------------------------------------------------------------------------------------------------------
		case WM_RBUTTONDBLCLK: C4GUI::MouseMove(C4MC_Button_RightDouble,LOWORD(lParam),HIWORD(lParam),wParam, cvp); break;
			//----------------------------------------------------------------------------------------------------------------------------------
		case WM_MOUSEMOVE:
			if ( Inside<int32_t>(LOWORD(lParam)-cvp->DrawX,0,cvp->ViewWdt-1)
			     && Inside<int32_t>(HIWORD(lParam)-cvp->DrawY,0,cvp->ViewHgt-1) )
				SetCursor(NULL);
			C4GUI::MouseMove(C4MC_Button_None,LOWORD(lParam),HIWORD(lParam),wParam, cvp);
			break;
			//----------------------------------------------------------------------------------------------------------------------------------
		case WM_MOUSEWHEEL:
			C4GUI::MouseMove(C4MC_Button_Wheel,LOWORD(lParam),HIWORD(lParam),wParam, cvp);
			break;
			//----------------------------------------------------------------------------------------------------------------------------------

		}
	}
	// Console edit cursor control
	else
	{
		// The state of the ALT key is not reported in wParam for mouse messages,
		// and for keyboard messages the key states are hidden in lParam. It's a mess. Let's just
		// query GetKeyState().
		DWORD dwKeyState = 0;
		if(GetKeyState(VK_CONTROL) < 0) dwKeyState |= MK_CONTROL;
		if(GetKeyState(VK_SHIFT) < 0) dwKeyState |= MK_SHIFT;
		if(GetKeyState(VK_MENU) < 0) dwKeyState |= MK_ALT;

		switch (uMsg)
		{
		case WM_KEYDOWN:
			Console.EditCursor.KeyDown(wParam, dwKeyState);
			break;
		case WM_KEYUP:
			Console.EditCursor.KeyUp(wParam, dwKeyState);
			break;
		case WM_SYSKEYDOWN:
			Console.EditCursor.KeyDown(wParam, dwKeyState);
			break;
		case WM_SYSKEYUP:
			Console.EditCursor.KeyUp(wParam, dwKeyState);
			break;
			//----------------------------------------------------------------------------------------------------------------------------------
		case WM_LBUTTONDOWN:
			// movement update needed before, so target is always up-to-date
			cvp->pWindow->EditCursorMove(LOWORD(lParam), HIWORD(lParam), dwKeyState);
			Console.EditCursor.LeftButtonDown(dwKeyState); break;
			//----------------------------------------------------------------------------------------------------------------------------------
		case WM_LBUTTONUP: Console.EditCursor.LeftButtonUp(dwKeyState); break;
			//----------------------------------------------------------------------------------------------------------------------------------
		case WM_RBUTTONDOWN: Console.EditCursor.RightButtonDown(dwKeyState); break;
			//----------------------------------------------------------------------------------------------------------------------------------
		case WM_RBUTTONUP: Console.EditCursor.RightButtonUp(dwKeyState); break;
			//----------------------------------------------------------------------------------------------------------------------------------
		case WM_MOUSEMOVE: cvp->pWindow->EditCursorMove(LOWORD(lParam), HIWORD(lParam), dwKeyState); break;
			//----------------------------------------------------------------------------------------------------------------------------------
		}
	}

	return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

LRESULT APIENTRY DialogWinProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// Determine dialog
	C4GUI::Dialog *pDlg = ::pGUI->GetDialog(hwnd);
	if (!pDlg) return DefWindowProc(hwnd, uMsg, wParam, lParam);

	// Process message
	switch (uMsg)
	{
		//---------------------------------------------------------------------------------------------------------------------------
	case WM_KEYDOWN:
		if (Game.DoKeyboardInput(wParam, KEYEV_Down, !!(lParam & 0x20000000), GetKeyState(VK_CONTROL) < 0, GetKeyState(VK_SHIFT) < 0, !!(lParam & 0x40000000), pDlg)) return 0;
		break;
		//---------------------------------------------------------------------------------------------------------------------------
	case WM_KEYUP:
		if (Game.DoKeyboardInput(wParam, KEYEV_Up, !!(lParam & 0x20000000), GetKeyState(VK_CONTROL) < 0, GetKeyState(VK_SHIFT) < 0, false, pDlg)) return 0;
		break;
		//------------------------------------------------------------------------------------------------------------
	case WM_SYSKEYDOWN:
		if (wParam == 18) break;
		if (Game.DoKeyboardInput(wParam, KEYEV_Down, !!(lParam & 0x20000000), GetKeyState(VK_CONTROL) < 0, GetKeyState(VK_SHIFT) < 0, !!(lParam & 0x40000000), pDlg)) return 0;
		break;
		//----------------------------------------------------------------------------------------------------------------------------------
	case WM_DESTROY:
	{
		const char *szID = pDlg->GetID();
		if (szID && *szID)
			StoreWindowPosition(hwnd, FormatString("ConsoleGUI_%s", szID).getData(), Config.GetSubkeyPath("Console"), false);
	}
	break;
	//----------------------------------------------------------------------------------------------------------------------------------
	case WM_CLOSE:
		pDlg->Close(false);
		break;
		//----------------------------------------------------------------------------------------------------------------------------------
	case WM_SIZE:
		// UpdateOutputSize
		break;
		//----------------------------------------------------------------------------------------------------------------------------------
	case WM_PAINT:
		// 2do: only draw specific dlg?
		//::GraphicsSystem.Execute();
		break;
		return 0;
		//----------------------------------------------------------------------------------------------------------------------------------
	case WM_LBUTTONDOWN: ::pGUI->MouseInput(C4MC_Button_LeftDown,LOWORD(lParam),HIWORD(lParam),wParam, pDlg, NULL); break;
		//----------------------------------------------------------------------------------------------------------------------------------
	case WM_LBUTTONUP: ::pGUI->MouseInput(C4MC_Button_LeftUp,LOWORD(lParam),HIWORD(lParam),wParam, pDlg, NULL); break;
		//----------------------------------------------------------------------------------------------------------------------------------
	case WM_RBUTTONDOWN: ::pGUI->MouseInput(C4MC_Button_RightDown,LOWORD(lParam),HIWORD(lParam),wParam, pDlg, NULL); break;
		//----------------------------------------------------------------------------------------------------------------------------------
	case WM_RBUTTONUP: ::pGUI->MouseInput(C4MC_Button_RightUp,LOWORD(lParam),HIWORD(lParam),wParam, pDlg, NULL); break;
		//----------------------------------------------------------------------------------------------------------------------------------
	case WM_LBUTTONDBLCLK: ::pGUI->MouseInput(C4MC_Button_LeftDouble,LOWORD(lParam),HIWORD(lParam),wParam, pDlg, NULL); break;
		//----------------------------------------------------------------------------------------------------------------------------------
	case WM_RBUTTONDBLCLK: ::pGUI->MouseInput(C4MC_Button_RightDouble,LOWORD(lParam),HIWORD(lParam),wParam, pDlg, NULL);  break;
		//----------------------------------------------------------------------------------------------------------------------------------
	case WM_MOUSEMOVE:
		//SetCursor(NULL);
		::pGUI->MouseInput(C4MC_Button_None,LOWORD(lParam),HIWORD(lParam),wParam, pDlg, NULL);
		break;
		//----------------------------------------------------------------------------------------------------------------------------------
	case WM_MOUSEWHEEL:
		::pGUI->MouseInput(C4MC_Button_Wheel,LOWORD(lParam),HIWORD(lParam),wParam, pDlg, NULL);
		break;
		//----------------------------------------------------------------------------------------------------------------------------------
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

C4Window::C4Window (): Active(false), pSurface(0), hWindow(0)
{
}
C4Window::~C4Window ()
{
}

C4Window * C4Window::Init(C4Window::WindowKind windowKind, C4AbstractApp * pApp, const char * Title, const C4Rect * size)
{
	Active = true;
	if (windowKind == W_Viewport)
	{
		static bool fViewportClassRegistered = false;
		if (!fViewportClassRegistered)
		{
			// Register viewport class
			WNDCLASSEXW WndClass;
			WndClass.cbSize=sizeof(WNDCLASSEX);
			WndClass.style         = CS_DBLCLKS | CS_BYTEALIGNCLIENT;
			WndClass.lpfnWndProc   = ViewportWinProc;
			WndClass.cbClsExtra    = 0;
			WndClass.cbWndExtra    = 0;
			WndClass.hInstance     = pApp->GetInstance();
			WndClass.hCursor       = LoadCursor (NULL, IDC_ARROW);
			WndClass.hbrBackground = (HBRUSH) COLOR_BACKGROUND;
			WndClass.lpszMenuName  = NULL;
			WndClass.lpszClassName = C4ViewportClassName;
			WndClass.hIcon         = LoadIcon (pApp->GetInstance(), MAKEINTRESOURCE (IDI_01_OCS) );
			WndClass.hIconSm       = LoadIcon (pApp->GetInstance(), MAKEINTRESOURCE (IDI_01_OCS) );
			if (!RegisterClassExW(&WndClass)) return NULL;
			fViewportClassRegistered = true;
		}
		// Create window
		hWindow = CreateWindowExW (
		            WS_EX_ACCEPTFILES,
		            C4ViewportClassName, GetWideChar(Title), C4ViewportWindowStyle,
		            CW_USEDEFAULT,CW_USEDEFAULT, size->Wdt, size->Hgt,
		            Console.hWindow,NULL,pApp->GetInstance(),NULL);
		if(!hWindow) return NULL;

		// We don't re-init viewport windows currently, so we don't need a child window
		// for now: Render into main window.
		hRenderWindow = hWindow;
	}
	else if (windowKind == W_Fullscreen)
	{
		// Register window class
		WNDCLASSEXW WndClass = {0};
		WndClass.cbSize        = sizeof(WNDCLASSEX);
		WndClass.style         = CS_DBLCLKS;
		WndClass.lpfnWndProc   = FullScreenWinProc;
		WndClass.hInstance     = pApp->GetInstance();
		WndClass.hbrBackground = (HBRUSH) COLOR_BACKGROUND;
		WndClass.lpszClassName = C4FullScreenClassName;
		WndClass.hIcon         = LoadIcon (pApp->GetInstance(), MAKEINTRESOURCE (IDI_00_C4X) );
		WndClass.hIconSm       = LoadIcon (pApp->GetInstance(), MAKEINTRESOURCE (IDI_00_C4X) );
		if (!RegisterClassExW(&WndClass)) return NULL;

		// Create window
		hWindow = CreateWindowExW  (
		            0,
		            C4FullScreenClassName,
		            GetWideChar(Title),
		            WS_OVERLAPPEDWINDOW,
		            CW_USEDEFAULT,CW_USEDEFAULT, size->Wdt, size->Hgt,
		            NULL,NULL,pApp->GetInstance(),NULL);
		if(!hWindow) return NULL;

		RECT rc;
		GetClientRect(hWindow, &rc);
		hRenderWindow = CreateWindowExW(0, L"STATIC", NULL, WS_CHILD,
		                                0, 0, rc.right - rc.left, rc.bottom - rc.top,
		                                hWindow, NULL, pApp->GetInstance(), NULL);
		if(!hRenderWindow) { DestroyWindow(hWindow); return NULL; }
		ShowWindow(hRenderWindow, SW_SHOW);

	#ifndef USE_CONSOLE
		// Show & focus
		ShowWindow(hWindow,SW_SHOWNORMAL);
		SetFocus(hWindow);
	#endif
	}
	else if (windowKind == W_GuiWindow)
	{
		static bool fDialogClassRegistered = false;
		if (!fDialogClassRegistered)
		{
			// register landscape viewport class
			WNDCLASSEXW WndClass;
			WndClass.cbSize=sizeof(WNDCLASSEX);
			WndClass.style         = CS_DBLCLKS | CS_BYTEALIGNCLIENT;
			WndClass.lpfnWndProc   = DialogWinProc;
			WndClass.cbClsExtra    = 0;
			WndClass.cbWndExtra    = 0;
			WndClass.hInstance     = pApp->GetInstance();
			WndClass.hCursor       = LoadCursor (NULL, IDC_ARROW); // - always use normal hw cursor
			WndClass.hbrBackground = (HBRUSH) COLOR_BACKGROUND;
			WndClass.lpszMenuName  = NULL;
			WndClass.lpszClassName = ConsoleDlgClassName;
			WndClass.hIcon         = LoadIcon (pApp->GetInstance(), MAKEINTRESOURCE (IDI_00_C4X) );
			WndClass.hIconSm       = LoadIcon (pApp->GetInstance(), MAKEINTRESOURCE (IDI_00_C4X) );
			if (!RegisterClassExW(&WndClass))
				return NULL;
		}
		Active = true;
		// calculate required size
		RECT rtSize;
		rtSize.left = 0;
		rtSize.top = 0;
		rtSize.right = size->Wdt;
		rtSize.bottom = size->Hgt;
		if (!::AdjustWindowRectEx(&rtSize, ConsoleDlgWindowStyle, false, 0))
			return NULL;
		// create it!
		if (!Title || !*Title) Title = "???";
		hWindow = ::CreateWindowExW(
		            0,
		            ConsoleDlgClassName, GetWideChar(Title),
		            ConsoleDlgWindowStyle,
		            CW_USEDEFAULT,CW_USEDEFAULT,rtSize.right-rtSize.left,rtSize.bottom-rtSize.top,
					::Console.hWindow,NULL,pApp->GetInstance(),NULL);
		hRenderWindow = hWindow;
		return hWindow ? this : 0;
	}
	return this;
}

bool C4Window::ReInit(C4AbstractApp* pApp)
{
	// We don't need to change anything with the window for any
	// configuration option changes on Windows.
	
	// However, re-create the render window so that another pixel format can
	// be chosen for it. The pixel format is chosen in CStdGLCtx::Init.

	RECT rc;
	GetClientRect(hWindow, &rc);
	HWND hNewRenderWindow = CreateWindowExW(0, L"STATIC", NULL, WS_CHILD,
	                                        0, 0, rc.right - rc.left, rc.bottom - rc.top,
	                                        hWindow, NULL, pApp->hInstance, NULL);
	if(!hNewRenderWindow) return false;

	ShowWindow(hNewRenderWindow, SW_SHOW);
	DestroyWindow(hRenderWindow);
	hRenderWindow = hNewRenderWindow;

	return true;
}

void C4Window::Clear()
{
	// Destroy window
	if (hRenderWindow) DestroyWindow(hRenderWindow);
	if (hWindow && hWindow != hRenderWindow) DestroyWindow(hWindow);
	hRenderWindow = NULL;
	hWindow = NULL;
}

bool C4Window::StorePosition(const char *szWindowName, const char *szSubKey, bool fStoreSize)
{
	return StoreWindowPosition(hWindow, szWindowName, szSubKey, fStoreSize) != 0;
}

bool C4Window::RestorePosition(const char *szWindowName, const char *szSubKey, bool fHidden)
{
	if (!RestoreWindowPosition(hWindow, szWindowName, szSubKey, fHidden))
		ShowWindow(hWindow,SW_SHOWNORMAL);
	return true;
}

void C4Window::SetTitle(const char *szToTitle)
{
	if (hWindow) SetWindowTextW(hWindow, szToTitle ? GetWideChar(szToTitle) : L"");
}

bool C4Window::GetSize(C4Rect * pRect)
{
	RECT r;
	if (!(hWindow && GetClientRect(hWindow,&r))) return false;
	pRect->x = r.left;
	pRect->y = r.top;
	pRect->Wdt = r.right - r.left;
	pRect->Hgt = r.bottom - r.top;
	return true;
}

void C4Window::SetSize(unsigned int cx, unsigned int cy)
{
	if (hWindow)
	{
		// If bordered, add border size
		RECT rect = {0, 0, cx, cy};
		::AdjustWindowRectEx(&rect, GetWindowLong(hWindow, GWL_STYLE), FALSE, GetWindowLong(hWindow, GWL_EXSTYLE));
		cx = rect.right - rect.left;
		cy = rect.bottom - rect.top;
		::SetWindowPos(hWindow, NULL, 0, 0, cx, cy, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOREDRAW | SWP_NOZORDER);

		// Also resize child window
		GetClientRect(hWindow, &rect);
		::SetWindowPos(hRenderWindow, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOREDRAW | SWP_NOZORDER);
	}
}

void C4Window::FlashWindow()
{
	// please activate me!
	if (hWindow)
		::FlashWindow(hWindow, FLASHW_ALL | FLASHW_TIMERNOFG);
}

void C4Window::EnumerateMultiSamples(std::vector<int>& samples) const
{
#ifdef USE_GL
	if(pGL && pGL->pMainCtx)
		samples = pGL->pMainCtx->EnumerateMultiSamples();
#endif

#ifdef USE_DIRECTX
	// TODO: Enumerate multi samples
#endif
}

/* CStdMessageProc */

bool CStdMessageProc::Execute(int iTimeout, pollfd *)
{
	// Peek messages
	MSG msg;
	while (PeekMessage(&msg,NULL,0,0,PM_REMOVE))
	{
		// quit?
		if (msg.message == WM_QUIT)
		{
			pApp->fQuitMsgReceived = true;
			return false;
		}
		// Dialog message transfer
		if (!pApp->pWindow || !pApp->pWindow->Win32DialogMessageHandling(&msg))
		{
			TranslateMessage(&msg); DispatchMessage(&msg);
		}
	}
	return true;
}

/* C4AbstractApp */

C4AbstractApp::C4AbstractApp() :
		Active(false), pWindow(NULL), fQuitMsgReceived(false),
		hInstance(NULL), fDspModeSet(false)
{
	ZeroMemory(&pfd, sizeof(pfd)); pfd.nSize = sizeof(pfd);
	ZeroMemory(&dspMode, sizeof(dspMode)); dspMode.dmSize =  sizeof(dspMode);
	ZeroMemory(&OldDspMode, sizeof(OldDspMode)); OldDspMode.dmSize =  sizeof(OldDspMode);
	hMainThread = NULL;
#ifdef _WIN32
	MessageProc.SetApp(this);
	Add(&MessageProc);
#endif
}

C4AbstractApp::~C4AbstractApp()
{
}

bool C4AbstractApp::Init(int argc, char * argv[])
{
	// Set instance vars
	hMainThread = ::GetCurrentThread();
	// Custom initialization
	return DoInit (argc, argv);
}

void C4AbstractApp::Clear()
{
	hMainThread = NULL;
}

void C4AbstractApp::Quit()
{
	PostQuitMessage(0);
}

bool C4AbstractApp::FlushMessages()
{

	// Always fail after quit message
	if (fQuitMsgReceived)
		return false;

	return MessageProc.Execute(0);
}

int GLMonitorInfoEnumCount;

static BOOL CALLBACK GLMonitorInfoEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
	// get to indexed monitor
	if (GLMonitorInfoEnumCount--) return true;
	// store it
	C4AbstractApp *pApp = (C4AbstractApp *) dwData;
	pApp->hMon = hMonitor;
	pApp->MonitorRect = *lprcMonitor;
	return true;
}

bool C4AbstractApp::GetIndexedDisplayMode(int32_t iIndex, int32_t *piXRes, int32_t *piYRes, int32_t *piBitDepth, int32_t *piRefreshRate, uint32_t iMonitor)
{
	// prepare search struct
	DEVMODEW dmode;
	ZeroMemory(&dmode, sizeof(dmode)); dmode.dmSize = sizeof(dmode);
	StdStrBuf Mon;
	if (iMonitor)
		Mon.Format("\\\\.\\Display%d", iMonitor+1);
	// check if indexed mode exists
	if (!EnumDisplaySettingsW(Mon.GetWideChar(), iIndex, &dmode)) return false;
	// mode exists; return it
	if (piXRes) *piXRes = dmode.dmPelsWidth;
	if (piYRes) *piYRes = dmode.dmPelsHeight;
	if (piBitDepth) *piBitDepth = dmode.dmBitsPerPel;
	if (piRefreshRate) *piRefreshRate = dmode.dmDisplayFrequency;
	return true;
}

void C4AbstractApp::RestoreVideoMode()
{
}

bool C4AbstractApp::SetVideoMode(unsigned int iXRes, unsigned int iYRes, unsigned int iColorDepth, unsigned int iRefreshRate, unsigned int iMonitor, bool fFullScreen)
{
#ifdef USE_DIRECTX
	if (pD3D)
	{
		if (!pD3D->SetVideoMode(iXRes, iYRes, iColorDepth, iMonitor, fFullScreen))
			return false;
		OnResolutionChanged(iXRes, iYRes);
		return true;
	}
#endif
#ifdef USE_GL
	SetWindowLong(pWindow->hWindow, GWL_EXSTYLE,
	              GetWindowLong(pWindow->hWindow, GWL_EXSTYLE) | WS_EX_APPWINDOW);
	bool fFound=false;
	DEVMODEW dmode;
	// if a monitor is given, search on that instead
	// get monitor infos
	GLMonitorInfoEnumCount = iMonitor;
	hMon = NULL;
	EnumDisplayMonitors(NULL, NULL, GLMonitorInfoEnumProc, (LPARAM) this);
	// no monitor assigned?
	if (!hMon)
	{
		// Okay for primary; then just use a default
		if (!iMonitor)
		{
			MonitorRect.left = MonitorRect.top = 0;
			MonitorRect.right = iXRes; MonitorRect.bottom = iYRes;
		}
		else return false;
	}
	StdStrBuf Mon;
	if (iMonitor)
		Mon.Format("\\\\.\\Display%d", iMonitor+1);

	ZeroMemory(&dmode, sizeof(dmode)); dmode.dmSize = sizeof(dmode);
	
	// Get current display settings
	if (!EnumDisplaySettingsW(Mon.GetWideChar(), ENUM_CURRENT_SETTINGS, &dmode))
		return false;
	int orientation = dmode.dmDisplayOrientation;
	// enumerate modes
	int i=0;
	while (EnumDisplaySettingsW(Mon.GetWideChar(), i++, &dmode))
		// compare enumerated mode with requested settings
		if (dmode.dmPelsWidth==iXRes && dmode.dmPelsHeight==iYRes && dmode.dmBitsPerPel==iColorDepth && dmode.dmDisplayOrientation==orientation
			&& (iRefreshRate == 0 || dmode.dmDisplayFrequency == iRefreshRate))
		{
			fFound=true;
			dspMode=dmode;
			break;
		}
	if (!fFound) return false;
	// change mode
	if (!fFullScreen)
	{
		ChangeDisplaySettings(NULL, CDS_RESET);
		SetWindowLong(pWindow->hWindow, GWL_STYLE,
		              GetWindowLong(pWindow->hWindow, GWL_STYLE) | (WS_CAPTION|WS_THICKFRAME|WS_BORDER));
	}
	else
	{
		dspMode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
		if (iRefreshRate != 0)
			dspMode.dmFields |= DM_DISPLAYFREQUENCY;
		if (ChangeDisplaySettingsExW(iMonitor ? Mon.GetWideChar() : NULL, &dspMode, NULL, CDS_FULLSCREEN, NULL) != DISP_CHANGE_SUCCESSFUL)
			{
				return false;
			}

		SetWindowLong(pWindow->hWindow, GWL_STYLE,
		              GetWindowLong(pWindow->hWindow, GWL_STYLE) & ~ (WS_CAPTION|WS_THICKFRAME|WS_BORDER));
	}

	::SetWindowPos(pWindow->hWindow, NULL, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW|SWP_FRAMECHANGED);
	pWindow->SetSize(dspMode.dmPelsWidth, dspMode.dmPelsHeight);
	OnResolutionChanged(iXRes, iYRes);
	return true;
#endif
}

bool C4AbstractApp::SaveDefaultGammaRamp(_D3DGAMMARAMP &ramp)
{
#ifdef USE_DIRECTX
	if (pD3D)
	{
		return pD3D->SaveDefaultGammaRamp(ramp);
	}
#endif
	HDC hDC = GetDC(pWindow->hWindow);
	if (hDC)
	{
		bool r = !!GetDeviceGammaRamp(hDC, &ramp);
		if (!r)
		{
			Log("  Error getting default gamma ramp; using standard");
		}
		ReleaseDC(pWindow->hWindow, hDC);
		return r;
	}
	return false;
}

bool C4AbstractApp::ApplyGammaRamp(_D3DGAMMARAMP &ramp, bool fForce)
{
#ifdef USE_DIRECTX
	if (pD3D)
	{
		return pD3D->ApplyGammaRamp(ramp, fForce);
	}
#endif
	if (!Active && !fForce) return false;
	HDC hDC = GetDC(pWindow->hWindow);
	if (hDC)
	{
		bool r = !!SetDeviceGammaRamp(hDC, &ramp);
		ReleaseDC(pWindow->hWindow, hDC);
		return r;
	}
	return false;
}

void C4AbstractApp::MessageDialog(const char * message)
{
	MessageBoxW(0, GetWideChar(message), ADDL(C4ENGINECAPTION), MB_ICONERROR);
}

// Clipboard functions
bool C4AbstractApp::Copy(const StdStrBuf & text, bool fClipboard)
{
	if (!fClipboard) return false;
	bool fSuccess = true;
	// gain clipboard ownership
	if (!OpenClipboard(pWindow ? pWindow->hWindow : NULL)) return false;
	// must empty the global clipboard, so the application clipboard equals the Windows clipboard
	EmptyClipboard();
	int size = MultiByteToWideChar(CP_UTF8, 0, text.getData(), text.getSize(), 0, 0);
	HANDLE hglbCopy = GlobalAlloc(GMEM_MOVEABLE, size * sizeof(wchar_t));
	if (hglbCopy == NULL) { CloseClipboard(); return false; }
	// lock the handle and copy the text to the buffer.
	wchar_t *szCopyChar = (wchar_t *) GlobalLock(hglbCopy);
	fSuccess = !!MultiByteToWideChar(CP_UTF8, 0, text.getData(), text.getSize(), szCopyChar, size);
	GlobalUnlock(hglbCopy);
	// place the handle on the clipboard.
	fSuccess = fSuccess && !!SetClipboardData(CF_UNICODETEXT, hglbCopy);
	// close clipboard
	CloseClipboard();
	// return whether copying was successful
	return fSuccess;
}

StdStrBuf C4AbstractApp::Paste(bool fClipboard)
{
	if (!fClipboard) return StdStrBuf();
	// open clipboard
	if (!OpenClipboard(NULL)) return StdStrBuf();
	// get text from clipboard
	HANDLE hglb = GetClipboardData(CF_UNICODETEXT);
	if (!hglb) return StdStrBuf();
	StdStrBuf text((LPCWSTR) GlobalLock(hglb));
	// unlock mem
	GlobalUnlock(hglb);
	// close clipboard
	CloseClipboard();
	return text;
}

bool C4AbstractApp::IsClipboardFull(bool fClipboard)
{
	if (!fClipboard) return false;
	return !!IsClipboardFormatAvailable(CF_UNICODETEXT);
}

void C4Window::RequestUpdate()
{
	// just invoke directly
	PerformUpdate();
}

