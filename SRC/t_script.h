// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// $Id: t_script.h 538 2009-09-23 23:24:07Z smite-meister $
//
// Copyright(C) 2000 Simon Howard
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// $Log: t_script.h,v $
// Revision 1.3  2003/05/30 22:44:07  hurdler
// add checkcvar function to FS
//
// Revision 1.2  2000/11/03 03:27:17  stroggonmeth
// Again with the bug fixing...
//
// Revision 1.1  2000/11/02 17:57:28  stroggonmeth
// FraggleScript files...
//
//
//--------------------------------------------------------------------------

#ifndef __T_SCRIPT_H__
#define __T_SCRIPT_H__

typedef struct runningscript_s runningscript_t;

#include "p_mobj.h"
#include "t_parse.h"

struct runningscript_s
{
  script_t *script;
  
  // where we are
  char *savepoint;

  enum
  {
    wt_none,        // not waiting
    wt_delay,       // wait for a set amount of time
    wt_tagwait,     // wait for sector to stop moving
    wt_scriptwait,  // wait for script to finish
  } wait_type;
  int wait_data;  // data for wait: tagnum, counter, script number etc
	
  // saved variables
  svariable_t *variables[VARIABLESLOTS];
  
  runningscript_t *prev, *next;  // for chain
  mobj_t *trigger;
};

void T_Init();
void T_ClearScripts();
void T_RunScript(int n);
void T_RunThingScript(int);
void T_PreprocessScripts();
void T_DelayedScripts();
mobj_t *MobjForSvalue(svalue_t svalue);

        // console commands
void T_Dump();
void T_ConsRun();

extern script_t levelscript;
//extern script_t *scripts[MAXSCRIPTS];       // the scripts
extern mobj_t *t_trigger;

void T_AddCommands();

#endif
