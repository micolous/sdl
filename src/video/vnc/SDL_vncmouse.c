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
	rfbFreeCursor(cursor->c);
	cursor->c = NULL;
	SDL_free(cursor);
}

WMcursor *VNC_CreateWMCursor(_THIS, Uint8 *data, Uint8 *mask, int w, int h, int hot_x, int hot_y) {
	WMcursor *cursor = SDL_malloc(sizeof(WMcursor));
	
	cursor->c = rfbMakeXCursor(w, h, data, mask);
	cursor->c->xhot = hot_x;
	cursor->c->yhot = hot_y;
	
	// prevent libvncserver from free()ing cursors.
	cursor->c->cleanup = 
	cursor->c->cleanupSource = 
	cursor->c->cleanupMask =
	cursor->c->cleanupRichSource = FALSE;
	
	return cursor;
}

int VNC_ShowWMCursor(_THIS, WMcursor *cursor) {
	// only allowed to do this when the server is active.
	if (this->hidden->vncs) {
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

