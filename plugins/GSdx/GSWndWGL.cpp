/*
 *	Copyright (C) 2007-2012 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "stdafx.h"
#include "GSWndWGL.h"

#ifdef _WIN32
GSWndWGL::GSWndWGL()
	: m_NativeWindow(NULL), m_NativeDisplay(NULL), m_context(NULL)
{
}

// Used by GSReplay. Perhaps the stuff used by GSReplay can be moved out? That way all
// the GSOpen 1 stuff can be removed. But that'll take a bit of thinking.
LRESULT CALLBACK GSWndWGL::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CLOSE:
		// This takes place before GSClose, so don't destroy the Window so we can clean up.
		ShowWindow(hWnd, SW_HIDE);
		// DestroyWindow(hWnd);
		return 0;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
}


void GSWndWGL::CreateContext(int major, int minor)
{
	if ( !m_NativeDisplay || !m_NativeWindow )
	{
		fprintf( stderr, "Wrong display/window\n" );
		exit(1);
	}

	// GL2 context are quite easy but we need GL3 which is another painful story...
	m_context = wglCreateContext(m_NativeDisplay);
	if (!m_context) {
		fprintf(stderr, "Failed to create a 2.0 context\n");
		throw GSDXRecoverableError();
	}

	// FIXME test it
	// Note: albeit every tutorial said that we need an opengl context to use the GL function wglCreateContextAttribsARB
	// On linux it works without the extra temporary context, not sure the limitation still applied
	if (major >= 3) {
		AttachContext();

		// Create a context
		int context_attribs[] =
		{
			WGL_CONTEXT_MAJOR_VERSION_ARB, major,
			WGL_CONTEXT_MINOR_VERSION_ARB, minor,
			// FIXME : Request a debug context to ease opengl development
			// Note: don't support deprecated feature (pre openg 3.1)
			//GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_DEBUG_BIT_ARB | GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
			WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB
#ifdef ENABLE_OGL_DEBUG
			| WGL_CONTEXT_DEBUG_BIT_ARB
#else
			| GL_CONTEXT_FLAG_NO_ERROR_BIT_KHR
#endif
			,
			WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
			0
		};

		PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
		if (!wglCreateContextAttribsARB) {
			fprintf(stderr, "Failed to init wglCreateContextAttribsARB function pointer\n");
			throw GSDXRecoverableError();
		}

		HGLRC context30 = wglCreateContextAttribsARB(m_NativeDisplay, NULL, context_attribs);
		if (!context30) {
			fprintf(stderr, "Failed to create a 3.x context with standard flags\n");
			// retry with more compatible option for (Mesa on Windows, OpenGL on WINE)
			context_attribs[2*2+1] = 0;

			context30 = wglCreateContextAttribsARB(m_NativeDisplay, NULL, context_attribs);
			if (!context30) {
				fprintf(stderr, "Failed to create a 3.x context with compatible flags\n");
				throw GSDXRecoverableError();
			}
		}

		DetachContext();
		wglDeleteContext(m_context);

		m_context = context30;
		fprintf(stderr, "3.x GL context successfully created\n");
	}
}

void GSWndWGL::AttachContext()
{
	if (!IsContextAttached()) {
		wglMakeCurrent(m_NativeDisplay, m_context);
		m_ctx_attached = true;
	}
}

void GSWndWGL::DetachContext()
{
	if (IsContextAttached()) {
		wglMakeCurrent(NULL, NULL);
		m_ctx_attached = false;
	}
}

//TODO: DROP ???
void GSWndWGL::CheckContext()
{
#if 0
	int glxMajorVersion, glxMinorVersion;
	glXQueryVersion(m_NativeDisplay, &glxMajorVersion, &glxMinorVersion);
	if (glXIsDirect(m_NativeDisplay, m_context))
		fprintf(stderr, "glX-Version %d.%d with Direct Rendering\n", glxMajorVersion, glxMinorVersion);
	else
		fprintf(stderr, "glX-Version %d.%d with Indirect Rendering !!! It won't support properly opengl\n", glxMajorVersion, glxMinorVersion);
#endif
}

bool GSWndWGL::Attach(void* handle, bool managed)
{
	m_NativeWindow = (HWND)handle;
	m_managed = managed;

	OpenWGLDisplay();

	CreateContext(3, 3);

	AttachContext();

	CheckContext();

	m_swapinterval = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");

	PopulateGlFunction();

	UpdateWindow(m_NativeWindow);

	return true;
}

void GSWndWGL::Detach()
{
	// Actually the destructor is not called when there is only a GSclose/GSshutdown
	// The window still need to be closed
	DetachContext();

	if (m_context) wglDeleteContext(m_context);
	m_context = NULL;

	CloseWGLDisplay();

	// Used by GSReplay.
	if (m_NativeWindow && m_managed)
	{
		DestroyWindow(m_NativeWindow);
		m_NativeWindow = NULL;
	}

}

void GSWndWGL::OpenWGLDisplay()
{
	GLuint	  PixelFormat;			// Holds The Results After Searching For A Match
	PIXELFORMATDESCRIPTOR pfd =			 // pfd Tells Windows How We Want Things To Be

	{
		sizeof(PIXELFORMATDESCRIPTOR),			  // Size Of This Pixel Format Descriptor
		1,										  // Version Number
		PFD_DRAW_TO_WINDOW |						// Format Must Support Window
		PFD_SUPPORT_OPENGL |						// Format Must Support OpenGL
		PFD_DOUBLEBUFFER,						   // Must Support Double Buffering
		PFD_TYPE_RGBA,							  // Request An RGBA Format
		32,										 // Select Our Color Depth
		0, 0, 0, 0, 0, 0,						   // Color Bits Ignored
		0,										  // 8bit Alpha Buffer
		0,										  // Shift Bit Ignored
		0,										  // No Accumulation Buffer
		0, 0, 0, 0,								 // Accumulation Bits Ignored
		24,										 // 24Bit Z-Buffer (Depth Buffer)
		8,										  // 8bit Stencil Buffer
		0,										  // No Auxiliary Buffer
		PFD_MAIN_PLANE,							 // Main Drawing Layer
		0,										  // Reserved
		0, 0, 0									 // Layer Masks Ignored
	};

	m_NativeDisplay = GetDC(m_NativeWindow);
	if (!m_NativeDisplay)
	{
		MessageBox(NULL, "(1) Can't Create A GL Device Context.", "ERROR", MB_OK | MB_ICONEXCLAMATION);
		throw GSDXRecoverableError();
	}
	PixelFormat = ChoosePixelFormat(m_NativeDisplay, &pfd);
	if (!PixelFormat)
	{
		MessageBox(NULL, "(2) Can't Find A Suitable PixelFormat.", "ERROR", MB_OK | MB_ICONEXCLAMATION);
		throw GSDXRecoverableError();
	}

	if (!SetPixelFormat(m_NativeDisplay, PixelFormat, &pfd))
	{
		MessageBox(NULL, "(3) Can't Set The PixelFormat.", "ERROR", MB_OK | MB_ICONEXCLAMATION);
		throw GSDXRecoverableError();
	}
}

void GSWndWGL::CloseWGLDisplay()
{
	if (m_NativeDisplay && !ReleaseDC(m_NativeWindow, m_NativeDisplay))				 // Are We Able To Release The DC
	{
		MessageBox(NULL, "Release Device Context Failed.", "SHUTDOWN ERROR", MB_OK | MB_ICONINFORMATION);
	}
	m_NativeDisplay = NULL;									 // Set DC To NULL
}

//TODO: GSopen 1 => Drop?
// Used by GSReplay. At least for now.
// More or less copy pasted from GSWndDX::Create and GSWndWGL::Attach with a few
// modifications
bool GSWndWGL::Create(const string& title, int w, int h)
{
	if(m_NativeWindow) return false;

	m_managed = true;

	WNDCLASS wc;

	memset(&wc, 0, sizeof(wc));

	wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS | CS_OWNDC;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = theApp.GetModuleHandle();
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszClassName = "GSWndOGL";

	if (!GetClassInfo(wc.hInstance, wc.lpszClassName, &wc))
	{
		if (!RegisterClass(&wc))
		{
			return false;
		}
	}

	DWORD style = WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_OVERLAPPEDWINDOW | WS_BORDER;

	GSVector4i r;

	GetWindowRect(GetDesktopWindow(), r);

	// Old GSOpen ModeWidth and ModeHeight are not necessary with this.
	bool remote = !!GetSystemMetrics(SM_REMOTESESSION);

	if (w <= 0 || h <= 0 || remote)
	{
		w = r.width() / 3;
		h = r.width() / 4;

		if (!remote)
		{
			w *= 2;
			h *= 2;
		}
	}

	r.left = (r.left + r.right - w) / 2;
	r.top = (r.top + r.bottom - h) / 2;
	r.right = r.left + w;
	r.bottom = r.top + h;

	AdjustWindowRect(r, style, FALSE);

	m_NativeWindow = CreateWindow(wc.lpszClassName, title.c_str(), style, r.left, r.top, r.width(), r.height(), NULL, NULL, wc.hInstance, (LPVOID)this);

	if (m_NativeWindow == NULL) return false;

	OpenWGLDisplay();

	CreateContext(3, 3);

	AttachContext();

	m_swapinterval = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");

	PopulateGlFunction();

	return true;

}

//Same as DX
GSVector4i GSWndWGL::GetClientRect()
{
	GSVector4i r;

	::GetClientRect(m_NativeWindow, r);

	return r;
}

void* GSWndWGL::GetProcAddress(const char* name, bool opt)
{
	void* ptr = (void*)wglGetProcAddress(name);
	if (ptr == NULL) {
		fprintf(stderr, "Failed to find %s\n", name);
		if (!opt)
			throw GSDXRecoverableError();
	}
	return ptr;
}

//TODO: check extensions supported or not
//FIXME : extension allocation
void GSWndWGL::SetVSync(bool enable)
{
	// m_swapinterval uses an integer as parameter
	// 0 -> disable vsync
	// n -> wait n frame
	if (m_swapinterval) m_swapinterval((int)enable);
}

void GSWndWGL::Flip()
{
	SwapBuffers(m_NativeDisplay);
}

void GSWndWGL::Show()
{
	if (!m_managed) return;

	// Used by GSReplay
	SetForegroundWindow(m_NativeWindow);
	ShowWindow(m_NativeWindow, SW_SHOWNORMAL);
	UpdateWindow(m_NativeWindow);
}

void GSWndWGL::Hide()
{
}

void GSWndWGL::HideFrame()
{
}

// Returns FALSE if the window has no title, or if th window title is under the strict
// management of the emulator.

bool GSWndWGL::SetWindowText(const char* title)
{
	if (!m_managed) return false;

	// Used by GSReplay.
	::SetWindowText(m_NativeWindow, title);

	return true;
}


#endif
