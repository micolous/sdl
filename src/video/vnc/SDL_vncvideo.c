/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2009 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
        
    Michael Farrell
    micolous+sdl@gmail.com
*/
#include "SDL_config.h"

/* VNC server SDL driver.
 *
 * This acts as a simple VNC server using libvncserver, to allow you to use
 * an SDL application directly over VNC (ie: without an X-server).
 *
 * This code was written by Michael Farrell <micolous+sdl@gmail.com>
 */

#include "SDL_video.h"
#include "SDL_mouse.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"

#include "SDL_vncvideo.h"
#include "SDL_vncevents_c.h"
#include "SDL_vncmouse_c.h"

#define VNCVID_DRIVER_NAME "vnc"

/* Initialization/Query functions */
static int VNC_VideoInit(_THIS, SDL_PixelFormat *vformat);
static SDL_Rect **VNC_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags);
static SDL_Surface *VNC_SetVideoMode(_THIS, SDL_Surface *current, int width, int height, int bpp, Uint32 flags);
static void VNC_VideoQuit(_THIS);

/* Hardware surface functions */
static int VNC_AllocHWSurface(_THIS, SDL_Surface *surface);
static int VNC_LockHWSurface(_THIS, SDL_Surface *surface);
static void VNC_UnlockHWSurface(_THIS, SDL_Surface *surface);
static void VNC_FreeHWSurface(_THIS, SDL_Surface *surface);

/* etc. */
static void VNC_UpdateRects(_THIS, int numrects, SDL_Rect *rects);
static void VNC_SetCaption(_THIS, const char* title, const char* icon);
static int VNC_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors);
/* VNC driver bootstrap functions */

static int VNC_Available(void)
{
	const char *envr = SDL_getenv("SDL_VIDEODRIVER");
	if ((envr) && (SDL_strcmp(envr, VNCVID_DRIVER_NAME) == 0)) {
		return(1);
	}

	return(0);
}

static void VNC_DeleteDevice(SDL_VideoDevice *device)
{
	SDL_free(device->hidden);
	SDL_free(device);
}

static SDL_VideoDevice *VNC_CreateDevice(int devindex)
{
	SDL_VideoDevice *device;

	/* Initialize all variables that we clean on shutdown */
	device = (SDL_VideoDevice *)SDL_malloc(sizeof(SDL_VideoDevice));
	if ( device ) {
		SDL_memset(device, 0, (sizeof *device));
		device->hidden = (struct SDL_PrivateVideoData *)
				SDL_malloc((sizeof *device->hidden));
	}
	if ( (device == NULL) || (device->hidden == NULL) ) {
		SDL_OutOfMemory();
		if ( device ) {
			SDL_free(device);
		}
		return(0);
	}
	SDL_memset(device->hidden, 0, (sizeof *device->hidden));

	/* Set the function pointers */
	device->VideoInit = VNC_VideoInit;
	device->ListModes = VNC_ListModes;
	device->SetVideoMode = VNC_SetVideoMode;
	device->CreateYUVOverlay = NULL;
	device->SetColors = VNC_SetColors;
	device->UpdateRects = VNC_UpdateRects;
	device->VideoQuit = VNC_VideoQuit;
	device->AllocHWSurface = VNC_AllocHWSurface;
	device->CheckHWBlit = NULL;
	device->FillHWRect = NULL;
	device->SetHWColorKey = NULL;
	device->SetHWAlpha = NULL;
	device->LockHWSurface = VNC_LockHWSurface;
	device->UnlockHWSurface = VNC_UnlockHWSurface;
	device->FlipHWSurface = NULL;
	device->FreeHWSurface = VNC_FreeHWSurface;
	device->SetCaption = VNC_SetCaption;
	device->SetIcon = NULL;
	device->IconifyWindow = NULL;
	device->GrabInput = NULL;
	device->GetWMInfo = NULL;
	device->InitOSKeymap = VNC_InitOSKeymap;
	device->PumpEvents = VNC_PumpEvents;
	
	// we don't implement relative movement at all - everything is absolute
	// this is due to the nature of VNC
	device->FreeWMCursor = VNC_FreeWMCursor;
	device->CreateWMCursor = VNC_CreateWMCursor;
	device->ShowWMCursor = VNC_ShowWMCursor;

	device->free = VNC_DeleteDevice;
	return device;
}

VideoBootStrap VNC_bootstrap = {
	VNCVID_DRIVER_NAME, "SDL libvncserver video driver",
	VNC_Available, VNC_CreateDevice
};


int VNC_VideoInit(_THIS, SDL_PixelFormat *vformat)
{
	/*
	fprintf(stderr, "WARNING: You are using the SDL dummy video driver!\n");
	*/
	/* we change this during the SDL_SetVideoMode implementation... */
	vformat->BitsPerPixel = 8;
	vformat->BytesPerPixel = 3;
	
	// make a blank cursor
	this->hidden->hiddenCursor = rfbMakeXCursor(0, 0, "\0", "\0");
	/* We're done! */
	return(0);
}

SDL_Rect **VNC_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags)
{
	// we only support truecolour visuals
	return format->palette ? NULL : (SDL_Rect **) -1;
}

SDL_Surface *VNC_SetVideoMode(_THIS, SDL_Surface *current,
				int width, int height, int bpp, Uint32 flags)
{
	if (bpp <= 8) {
		SDL_SetError("bpp <= 8 not supported by VNC output module.");
		return NULL;
	}

	if ( this->hidden->vncs && this->hidden->vncs->frameBuffer ) {
		SDL_free( this->hidden->vncs->frameBuffer );
	}
	
	printf("Setting mode %dx%d\n", width, height);


	void* newbuffer = SDL_malloc(width * height * (bpp / 8));
	rfbBool firstStart = TRUE;
	if ( ! newbuffer ) {
		SDL_SetError("Couldn't allocate buffer for requested mode");
		return(NULL);
	} else {
		if ( this->hidden->vncs ) {
			// i believe that this will automatically free() the existing framebuffer,
			// as attempts to free the old framebuffer result in segfault.
			rfbNewFramebuffer(this->hidden->vncs, newbuffer, width, height, 8, bpp/8, bpp/8);
			firstStart = FALSE;
		} else {
			this->hidden->vncs = rfbGetScreen(0, NULL, width, height, 8 /*bits per pixel */, bpp/8 /*bytes per pixel */, bpp/8);
			this->hidden->vncs->frameBuffer = newbuffer;
		}		
	}
	
	// clear buffer
	SDL_memset(this->hidden->vncs->frameBuffer, 0, width * height * (bpp / 8));

	/* Allocate the new pixel format for the screen */
	if ( ! SDL_ReallocFormat(current, bpp, 0,0,0,0) ) {
		SDL_free(this->hidden->vncs->frameBuffer);
		this->hidden->vncs->frameBuffer = NULL;
		SDL_SetError("Couldn't allocate new pixel format for requested mode");
		return(NULL);
	}
	
	this->hidden->vncs->serverFormat.redShift = current->format->Rshift;
	this->hidden->vncs->serverFormat.greenShift = current->format->Gshift;
	this->hidden->vncs->serverFormat.blueShift = current->format->Bshift;
	
	this->hidden->vncs->serverFormat.redMax = current->format->Rmask >> current->format->Rshift;
	this->hidden->vncs->serverFormat.greenMax = current->format->Gmask >> current->format->Gshift;
	this->hidden->vncs->serverFormat.blueMax = current->format->Bmask >> current->format->Bshift;
	
	current->w = width;
	current->h = height;
	
	// start server
	if (firstStart) {
		this->hidden->vncs->alwaysShared = TRUE;
		rfbSetCursor(this->hidden->vncs, this->hidden->hiddenCursor);
		rfbInitServer(this->hidden->vncs);
		VNC_InitEvents(this);
		rfbRunEventLoop(this->hidden->vncs, -1, TRUE);
	} else {
		// TODO make libvncclient push out the updated colourmap to clients.
	}

	/* Set up the new mode framebuffer */
	current->flags = flags & SDL_FULLSCREEN;
	current->pitch = current->w * (bpp / 8);
	current->pixels = this->hidden->vncs->frameBuffer;

	/* We're done */
	return(current);
}

/* We don't actually allow hardware surfaces other than the main one */
static int VNC_AllocHWSurface(_THIS, SDL_Surface *surface)
{
	return(-1);
}
static void VNC_FreeHWSurface(_THIS, SDL_Surface *surface)
{
	return;
}

/* We need to wait for vertical retrace on page flipped displays */
static int VNC_LockHWSurface(_THIS, SDL_Surface *surface)
{
	return(0);
}

static void VNC_UnlockHWSurface(_THIS, SDL_Surface *surface)
{
	return;
}

static void VNC_UpdateRects(_THIS, int numrects, SDL_Rect *rects)
{
	if (this->hidden->vncs) {
		int x = 0;
		for (; x<numrects; x++) {
			SDL_Rect r = rects[x];
			rfbMarkRectAsModified(this->hidden->vncs, r.x, r.y, r.w+r.x, r.h+r.y);
		}
	}
}

static void VNC_SetCaption(_THIS, const char* title, const char* icon)
{
	// TODO implement caption changing
}


int VNC_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors)
{
	// dummy function; we don't support setting colours, however it is required
	// to be implemented in the API.
	return 0;
}


/* Note:  If we are terminated, this could be called in the middle of
   another SDL video routine -- notably UpdateRects.
*/
void VNC_VideoQuit(_THIS)
{
	rfbShutdownServer(this->hidden->vncs, TRUE);
	if (this->screen->pixels != NULL)
	{
		SDL_free(this->screen->pixels);
		this->screen->pixels = NULL;
	}
}


