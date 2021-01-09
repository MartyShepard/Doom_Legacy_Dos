// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: keys.h 538 2009-09-23 23:24:07Z smite-meister $
//
// Copyright (C) 1998-2000 by DooM Legacy Team.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//
// $Log: keys.h,v $
// Revision 1.2  2000/02/27 00:42:10  hurdler
// fix CR+LF problem
//
// Revision 1.1.1.1  2000/02/22 20:32:32  hurdler
// Initial import into CVS (v1.29 pr3)
//
//
// DESCRIPTION:
//
//
//-----------------------------------------------------------------------------


#ifndef __KEYS_H__
#define __KEYS_H__

//
// DOOM keyboard definition.
// This is the stuff configured by Setup.Exe.
//

// This is the key codes as posted by the keyboard handler,
// ascii codes are 0->127,
// scancodes are 0x80 + 0->127

#define KEY_NULL        0       // null key, triggers nothing

#define KEY_ESCAPE      27
#define KEY_SPACE       32

#define KEY_NUMLOCK     (0x80+69)
#define KEY_SCROLLLOCK  (0x80+70)

#define KEY_MINUS       45
#define KEY_EQUALS      61
#define KEY_BACKSPACE   8
#define KEY_TAB         9
#define KEY_ENTER       13

//
//  scancodes 71-83 (non-extended)
//
#define KEY_KEYPAD7     (0x80+71)
#define KEY_KEYPAD8     (0x80+72)
#define KEY_KEYPAD9     (0x80+73)
#define KEY_MINUSPAD    (0x80+74)
#define KEY_KEYPAD4     (0x80+75)
#define KEY_KEYPAD5     (0x80+76)
#define KEY_KEYPAD6     (0x80+77)
#define KEY_PLUSPAD     (0x80+78)
#define KEY_KEYPAD1     (0x80+79)
#define KEY_KEYPAD2     (0x80+80)
#define KEY_KEYPAD3     (0x80+81)
#define KEY_KEYPAD0     (0x80+82)
#define KEY_KPADDEL     (0x80+83)

//  windows95 keys...

#define KEY_LEFTWIN     (0x80+91)
#define KEY_RIGHTWIN    (0x80+92)
#define KEY_MENU        (0x80+93)

//
//  scancodes 71-83 EXTENDED are remapped
//  to these by the keyboard handler (just add 30)
//
#define KEY_KPADSLASH   (0x80+100)      //extended scancode 53 '/' remapped

#define KEY_HOME        (0x80+101)
#define KEY_UPARROW     (0x80+102)
#define KEY_PGUP        (0x80+103)
#define KEY_LEFTARROW   (0x80+105)
#define KEY_RIGHTARROW  (0x80+107)
#define KEY_END         (0x80+109)
#define KEY_DOWNARROW   (0x80+110)
#define KEY_PGDN        (0x80+111)
#define KEY_INS         (0x80+112)
#define KEY_DEL         (0x80+113)


#define KEY_F1          (0x80+0x3b)
#define KEY_F2          (0x80+0x3c)
#define KEY_F3          (0x80+0x3d)
#define KEY_F4          (0x80+0x3e)
#define KEY_F5          (0x80+0x3f)
#define KEY_F6          (0x80+0x40)
#define KEY_F7          (0x80+0x41)
#define KEY_F8          (0x80+0x42)
#define KEY_F9          (0x80+0x43)
#define KEY_F10         (0x80+0x44)
#define KEY_F11         (0x80+0x57)
#define KEY_F12         (0x80+0x58)

#define KEY_PAUSE       255

// these ones must be non-extended scancodes (rctrl,rshift,lalt)
#define KEY_SHIFT       (0x80+54)
#define KEY_CTRL        (0x80+29)
#define KEY_ALT         (0x80+56)

#define KEY_CAPSLOCK    (0x80+58)
#define KEY_CONSOLE     '`'

#define KEY_OPENBRACKETS
#define KEY_CLOSEBRACKETS

#endif
