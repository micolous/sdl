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

#include "SDL_mouse.h"
#include "../../events/SDL_events_c.h"

#include "SDL_vncmouse_c.h"

#include <rfb/rfb.h>

/* The implementation dependent data for the window manager cursor */
struct WMcursor {
	rfbCursorPtr c;
};

void VNC_FreeWMCursor(_THIS, WMcursor *cursor) {
	SDL_free(cursor->c->source);
	SDL_free(cursor->c->mask);
	rfbFreeCursor(cursor->c);
	cursor->c = NULL;
	SDL_free(cursor);
}

WMcursor *VNC_CreateWMCursor(_THIS, Uint8 *data, Uint8 *mask, int w, int h, int hot_x, int hot_y) {
	WMcursor *cursor = SDL_malloc(sizeof(WMcursor));
	
	char* rfbData = SDL_malloc(w*h);
	char* rfbMask = SDL_malloc(w*h);
	
	if (!cursor || !rfbData || !rfbMask) {
		SDL_OutOfMemory();
		return NULL;
	}
	
	// Convert the cursor to rfb/x11 format
	// SDL stores a cursor as a series of bits, in MSB order
	// RFB expects it as a series of bytes, with space==0, x==1.
	int i;
	for (i=0; i<w*h; i++)
		rfbData[i] = data[i/8] & (1<<(7-(i%8))) ? 'x' : ' ';
	for (i=0; i<w*h; i++)
		rfbMask[i] = mask[i/8] & (1<<(7-(i%8))) ? 'x' : ' ';
	
	cursor->c = rfbMakeXCursor(w, h, rfbData, rfbMask);
	cursor->c->xhot = hot_x;
	cursor->c->yhot = hot_y;
	
	// prevent libvncserver from free()ing cursors and causing us to crash
	cursor->c->cleanup = 
	cursor->c->cleanupSource = 
	cursor->c->cleanupMask =
	cursor->c->cleanupRichSource = FALSE;
	
	/*
	printf("Created new %ix%i cursor, hotspot at %x,%x\n", w, h, hot_x, hot_y);
	printf("Cursor:\n");
	int y=0, x;
	for (; y<h; y++) {
		for (x=0; x<w; x++)
			printf("%02x", rfbData[(y*w)+x]);
		printf("\n");
	}
	
	printf("\nMask:\n");
	for (y=0; y<h; y++) {
		for (x=0; x<w; x++)
			printf("%02x", rfbMask[(y*w)+x]);
		printf("\n");
	}
	*/
	
	return cursor;
}

int VNC_ShowWMCursor(_THIS, WMcursor *cursor) {
	// only allowed to do this when the server is active.
	if (this->hidden->vncs) {
		// in view-only mode, don't actually set the mouse cursor, but lie about it
		// and report success so a software cursor is not implemented.
		if (this->hidden->viewOnly)
			return 1;
			
		if (cursor && cursor->c) {
			// show a specific cursor
			rfbSetCursor(this->hidden->vncs, cursor->c);
		} else {
			// hide the cursor
			rfbSetCursor(this->hidden->vncs, this->hidden->hiddenCursor);
		}
		return 1;
	} else {
		return 0;
	}
}

