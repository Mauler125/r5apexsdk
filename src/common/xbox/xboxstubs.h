//========= Copyright 1996-2004, Valve LLC, All rights reserved. ============
//
// Purpose: Win32 replacements for XBox.
//
//=============================================================================
#if !defined( XBOXSTUBS_H ) && !defined( _X360 )
#define XBOXSTUBS_H

typedef enum
{
	XK_BUTTON_UP,
	XK_BUTTON_DOWN,
	XK_BUTTON_LEFT,
	XK_BUTTON_RIGHT,

	XK_BUTTON_START,
	XK_BUTTON_BACK,

	XK_BUTTON_STICK1,
	XK_BUTTON_STICK2,

	XK_BUTTON_A,
	XK_BUTTON_B,
	XK_BUTTON_X,
	XK_BUTTON_Y,

	XK_BUTTON_LEFT_SHOULDER,
	XK_BUTTON_RIGHT_SHOULDER,

	XK_XBUTTON_LTRIGGER_PARTIAL,
	XK_XBUTTON_LTRIGGER_FULL,

	XK_XBUTTON_RTRIGGER_PARTIAL,
	XK_XBUTTON_RTRIGGER_FULL,

	XK_STICK1_UP,
	XK_STICK1_DOWN,
	XK_STICK1_LEFT,
	XK_STICK1_RIGHT,

	XK_STICK2_UP,
	XK_STICK2_DOWN,
	XK_STICK2_LEFT,
	XK_STICK2_RIGHT,

	XK_UP_DOWN,
	XK_LEFT_RIGHT,

	XK_MAX_KEYS,
} xKey_t;

#endif // XBOXSTUBS_H