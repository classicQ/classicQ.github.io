/*
 Copyright (C) 2011-2012 Florian Zwoch
 Copyright (C) 2011-2012 Mark Olsen
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
 
 See the GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#import <AppKit/AppKit.h>
#import "vid_macosx_bc.h"

extern bc_func_ptrs_t bc_func_ptrs;

static int GetDictionaryInt(CFDictionaryRef theDict, const void *key)
{
	int value = 0;
	CFNumberRef numRef;
	
	numRef = (CFNumberRef)CFDictionaryGetValue(theDict, key);
	if (numRef != NULL)
	{
		CFNumberGetValue(numRef, kCFNumberIntType, &value);
	}
	
	return value;
}

const char * const *Sys_Video_GetModeList(void)
{
	char buf[64];
	const char **ret;
	unsigned int num_modes;
	unsigned int i;
	CFArrayRef modes;
	
	if (NSAppKitVersionNumber < NSAppKitVersionNumber10_6)
	{
		modes = CGDisplayAvailableModes(CGMainDisplayID());
	}
	else
	{
		modes = bc_func_ptrs.CGDisplayCopyAllDisplayModes(CGMainDisplayID(), NULL);
	}

	if (modes == NULL)
	{
		return NULL;
	}
	
	num_modes = CFArrayGetCount(modes);
	
	ret = malloc((num_modes + 1) * sizeof(*ret));
	if (ret == NULL)
	{
		return NULL;
	}
	
	for (i = 0; i < num_modes; i++)
	{
		unsigned int width;
		unsigned int height;
		unsigned int flags;
		CFDictionaryRef mode_legacy;
		CGDisplayModeRef mode;
		
		if (NSAppKitVersionNumber < NSAppKitVersionNumber10_6)
		{
			mode_legacy = (CFDictionaryRef)CFArrayGetValueAtIndex(modes, i);
			
			width = GetDictionaryInt(mode_legacy, kCGDisplayWidth);
			height = GetDictionaryInt(mode_legacy, kCGDisplayHeight);
			flags = GetDictionaryInt(mode_legacy, kCGDisplayIOFlags);
		}
		else
		{
			mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, i);
			
			width = bc_func_ptrs.CGDisplayModeGetWidth(mode);
			height = bc_func_ptrs.CGDisplayModeGetHeight(mode);
			flags = bc_func_ptrs.CGDisplayModeGetIOFlags(mode);
		}

		snprintf(buf, sizeof(buf), "macosx:%u,%u,%u", width, height, flags);
		
		ret[i] = strdup(buf);
		if (ret[i] == NULL)
		{
			unsigned int j;
			
			for (j = 0; j < i; j++)
			{
				free((char*)ret[j]);
			}
			
			free(ret);

			if (NSAppKitVersionNumber >= NSAppKitVersionNumber10_6)
			{
				CFRelease(modes);
			}
			
			return NULL;
		}
	}
	
	if (NSAppKitVersionNumber >= NSAppKitVersionNumber10_6)
	{
		CFRelease(modes);
	}
	
	ret[num_modes] = NULL;
	
	return ret;
}

void Sys_Video_FreeModeList(const char * const *displaymodes)
{
	unsigned int i;
	
	for (i = 0; displaymodes[i] != NULL; i++)
	{
		free((void*)displaymodes[i]);
	}
	
	free((void*)displaymodes);
}

const char *Sys_Video_GetModeDescription(const char *mode)
{
	char buf[64];
	unsigned int width;
	unsigned int height;
	unsigned int flags;
	
	if (sscanf(mode, "macosx:%u,%u,%u", &width, &height, &flags) != 3)
	{
		return NULL;
	}
	
	snprintf(buf, sizeof(buf), "%ux%u%s", width, height, (flags & kDisplayModeStretchedFlag) ? " (stretched)" : "");
	
	return strdup(buf);
}

void Sys_Video_FreeModeDescription(const char *modedescription)
{
	free((void*)modedescription);
}
