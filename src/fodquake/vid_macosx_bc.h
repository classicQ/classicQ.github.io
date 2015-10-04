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

/*
 * Backwards Compatability
 * For supporting compiling and running with 10.5 SDK
 * but still using 10.6 API when available
 */

#import <AppKit/AppKit.h>

#ifndef NSAppKitVersionNumber10_5
#define NSAppKitVersionNumber10_5 949
#endif

#ifndef NSAppKitVersionNumber10_6
#define NSAppKitVersionNumber10_6 1038
typedef struct _CGDisplayConfigRef * CGDisplayConfigRef;
typedef struct CGDisplayMode * CGDisplayModeRef;
#endif

typedef struct {
	CGDisplayModeRef (*CGDisplayCopyDisplayMode)(CGDirectDisplayID display);
	CGError (*CGBeginDisplayConfiguration)(CGDisplayConfigRef *pConfigRef);
	CGError (*CGConfigureDisplayWithDisplayMode)(CGDisplayConfigRef config, CGDirectDisplayID display, CGDisplayModeRef mode, CFDictionaryRef options);
	CGError (*CGCompleteDisplayConfiguration)(CGDisplayConfigRef configRef, CGConfigureOption option);
	CGError (*CGCancelDisplayConfiguration)(CGDisplayConfigRef configRef);
	CFArrayRef (*CGDisplayCopyAllDisplayModes)(CGDirectDisplayID display, CFDictionaryRef options);
	size_t (*CGDisplayModeGetWidth)(CGDisplayModeRef mode);
	size_t (*CGDisplayModeGetHeight)(CGDisplayModeRef mode);
	uint32_t (*CGDisplayModeGetIOFlags)(CGDisplayModeRef mode);
} bc_func_ptrs_t;
