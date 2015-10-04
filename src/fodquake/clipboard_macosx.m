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

const char *Sys_Video_GetClipboardText(void *display)
{
	NSString *string = [[NSPasteboard generalPasteboard] stringForType:NSStringPboardType];
	char *text;	
	
	text = (char*)malloc(strlen([string cStringUsingEncoding:NSASCIIStringEncoding]) + 1);
	if (text == NULL)
	{
		return NULL;
	}
	
	sprintf(text, "%s", [string cStringUsingEncoding:NSASCIIStringEncoding]);
	
	return text;
}

void Sys_Video_FreeClipboardText(void *display, const char *text)
{
	free((char*)text);
}

void Sys_Video_SetClipboardText(void *display, const char *text)
{
	NSString *string = [[NSString alloc] initWithCString:text encoding:NSASCIIStringEncoding];
	
	[[NSPasteboard generalPasteboard] setString:string forType:NSStringPboardType];
	[string release];
	
	return;
}

