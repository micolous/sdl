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

#include "SDL.h"
#include "../../events/SDL_sysevents.h"
#include "../../events/SDL_events_c.h"

#include "SDL_vncvideo.h"
#include "SDL_vncevents_c.h"

#include <rfb/rfb.h>
#include <rfb/rfbproto.h>
#include <rfb/keysym.h>

static int VNC_lastButtonMask = 0;

void VNC_PumpEvents(_THIS)
{
	// at the moment, libvncserver is operating on another thread, so we don't
	// pump events here
}

void VNC_InitOSKeymap(_THIS)
{
	// keymaps are irrelevant for VNC
}

void VNC_KbdAddEvent(rfbBool down, rfbKeySym keySym, struct _rfbClientRec* cl)
{
	// handle a keyboard event from the vnc client.
	SDL_keysym sdlkey;
	memset(&sdlkey, 0, sizeof(SDL_keysym));
	
	switch (keySym) {
		// we need to remap some keycodes because libvncserver and sdl treat them differently	
		case XK_Shift_L:     sdlkey.sym = SDLK_LSHIFT;       break;
		case XK_Shift_R:     sdlkey.sym = SDLK_RSHIFT;       break;
		
		case XK_Control_L:   sdlkey.sym = SDLK_LCTRL;        break;
		case XK_Control_R:   sdlkey.sym = SDLK_RCTRL;        break;

		case XK_Meta_L:      sdlkey.sym = SDLK_LMETA;        break;
		case XK_Meta_R:      sdlkey.sym = SDLK_RMETA;        break;

		case XK_Alt_L:       sdlkey.sym = SDLK_LALT;         break;
		case XK_Alt_R:       sdlkey.sym = SDLK_RALT;         break;
		
		case XK_Super_L:     sdlkey.sym = SDLK_LSUPER;       break;
		case XK_Super_R:     sdlkey.sym = SDLK_RSUPER;       break;
		
		case XK_Escape:			 sdlkey.sym = SDLK_ESCAPE;       break;
		case XK_BackSpace:   sdlkey.sym = SDLK_BACKSPACE;    break;
		case XK_Return:      sdlkey.sym = SDLK_RETURN;       break;		
		case XK_Sys_Req:     sdlkey.sym = SDLK_SYSREQ;       break;
		case XK_Pause:       sdlkey.sym = SDLK_PAUSE;        break;
		case XK_Tab:         sdlkey.sym = SDLK_TAB;          break;
		case XK_Clear:       sdlkey.sym = SDLK_CLEAR;        break;
		case XK_Menu:        sdlkey.sym = SDLK_MENU;         break;
		case XK_EuroSign:    sdlkey.sym = SDLK_EURO;         break;
		case XK_Undo:        sdlkey.sym = SDLK_UNDO;         break;
		case XK_Print:       sdlkey.sym = SDLK_PRINT;        break;
		case XK_Help:        sdlkey.sym = SDLK_HELP;         break;
		case XK_Break:       sdlkey.sym = SDLK_BREAK;        break;
		case XK_Mode_switch: sdlkey.sym = SDLK_MODE;         break;
		case XK_Multi_key:   sdlkey.sym = SDLK_COMPOSE;      break;
		
		case XK_Insert:      sdlkey.sym = SDLK_INSERT;       break;
		case XK_Delete:      sdlkey.sym = SDLK_DELETE;       break;
		case XK_Home:        sdlkey.sym = SDLK_HOME;         break;
		case XK_End:         sdlkey.sym = SDLK_END;          break;
		case XK_Page_Up:     sdlkey.sym = SDLK_PAGEUP;       break;
		case XK_Page_Down:   sdlkey.sym = SDLK_PAGEDOWN;     break;
		
		case XK_Left:        sdlkey.sym = SDLK_LEFT;         break;
		case XK_Right:       sdlkey.sym = SDLK_RIGHT;        break;
		case XK_Up:          sdlkey.sym = SDLK_UP;           break;
		case XK_Down:        sdlkey.sym = SDLK_DOWN;         break;

		case XK_Num_Lock:    sdlkey.sym = SDLK_NUMLOCK;      break;
		case XK_Caps_Lock:   sdlkey.sym = SDLK_CAPSLOCK;     break;
		case XK_Scroll_Lock: sdlkey.sym = SDLK_SCROLLOCK;    break;		

		// these characters are printable, so we need to translate them as well.
		case XK_KP_Decimal:  sdlkey.sym = SDLK_KP_PERIOD;    sdlkey.unicode = 0x002E; break;
		case XK_KP_Divide:   sdlkey.sym = SDLK_KP_DIVIDE;    sdlkey.unicode = 0x00F7; break;
		case XK_KP_Multiply: sdlkey.sym = SDLK_KP_MULTIPLY;  sdlkey.unicode = 0x00D7; break;
		case XK_KP_Subtract: sdlkey.sym = SDLK_KP_MINUS;     sdlkey.unicode = 0x2212; break;
		case XK_KP_Add:      sdlkey.sym = SDLK_KP_PLUS;      sdlkey.unicode = 0x002B; break;
		case XK_KP_Enter:    sdlkey.sym = SDLK_KP_ENTER;     sdlkey.unicode = 0x000D; break;
				
		// the majority of keys however are the same
		default:
			sdlkey.sym = keySym;
	}
	// trap dangerous characters that we haven't translated yet
	// because sometimes when these go through, things crash.
	if (keySym > 0xF000 && !sdlkey.sym) {
		printf("VNC: Warning, unknown high keycode %u recieved.\n", keySym);
		return;
	}
	
	// map to a unicode character.  we're pretty much just going to assume all is well.
	// we don't have a keymap, so pretty much everything is one-to-one.
	//
	// in accordance with the behaviour of other SDL modules, we don't report
	// this for keyup events.
	sdlkey.unicode = keySym > 0xF000 || !down ? sdlkey.unicode : keySym;
	
	// handle capital letters
	if (keySym >= XK_A && keySym <= XK_Z) {
		sdlkey.sym = keySym - (XK_a - XK_A);
		sdlkey.mod = KMOD_SHIFT;
	}
	
	// handle numpad
	if (keySym >= XK_KP_0 && keySym <= XK_KP_9) {
		sdlkey.sym = keySym - (XK_KP_0 - SDLK_KP0);
		sdlkey.unicode = 0x0030 + (keySym - XK_KP_0);
	}
	
	// handle f-keys
	if (keySym >= XK_F1 && keySym <= XK_F15)
		sdlkey.sym = keySym - (XK_F1 - SDLK_F1);
	
	//printf("VNC: %u(vnc) maps to %u(sdl)\n", keySym, sdlkey.sym);
	
	sdlkey.scancode = (uint8_t)(keySym & 0xFF);
	
	// pump that through to SDL
	SDL_PrivateKeyboard(down, &sdlkey);

}

void VNC_PtrAddEvent (int buttonMask, int x, int y, struct _rfbClientRec* cl)
{
	// Handle a mouse event from the client
	
	// all mouse movements are absolute due to the nature of VNC
	// don't tell SDL the new event just yet.
	SDL_PrivateMouseMotion(VNC_lastButtonMask, FALSE, x, y);
	
	// check button status
	int buttonStateChanges = VNC_lastButtonMask ^ buttonMask;
	if (buttonStateChanges) {
		// button state has changed!
		int b;
		
		// find which buttons have been released
		int releasedButtons = buttonStateChanges & VNC_lastButtonMask;
		if (releasedButtons)
			for (b=1; b<=releasedButtons; b<<=1)
				if (releasedButtons & b)
					SDL_PrivateMouseButton(SDL_RELEASED, b, 0, 0);
		
		// find which buttons have been pressed
		int pressedButtons = buttonStateChanges & buttonMask;
		if (pressedButtons)
			for (b=1; b<=pressedButtons; b<<=1)
				if (pressedButtons & b)
					SDL_PrivateMouseButton(SDL_PRESSED, b, 0, 0);
		
		VNC_lastButtonMask = buttonMask;
	}
}


void VNC_InitEvents(_THIS)
{
	printf("VNC: Hooking up event handlers for VNC\n");
	rfbScreenInfoPtr vncs = this->hidden->vncs;
	
	vncs->kbdAddEvent = VNC_KbdAddEvent;
	vncs->ptrAddEvent = VNC_PtrAddEvent;

}

/* end of SDL_vncevents.c ... */

