#import <AppKit/AppKit.h>
#import <mach/mach_time.h>

#import <stdio.h>
#import <stdlib.h>
#import <stdarg.h>
#import <dlfcn.h>

#undef true
#undef false

#import "common.h"
#import "vid_macosx_bc.h"

static mach_timebase_info_data_t tbinfo;
static int randomfd;
static NSAutoreleasePool *pool;
static void *CoreGraphicsLibrary = NULL;

bc_func_ptrs_t bc_func_ptrs;

static int load_missing_osx_symbols()
{
	memset(&bc_func_ptrs, 0, sizeof(bc_func_ptrs_t));
	
	if (NSAppKitVersionNumber < NSAppKitVersionNumber10_6)
		return 0;
	
	CoreGraphicsLibrary = dlopen("/System/Library/Frameworks/ApplicationServices.framework/Frameworks/CoreGraphics.framework/CoreGraphics", RTLD_NOW);
	if (!CoreGraphicsLibrary)
		return 1;
	
	bc_func_ptrs.CGDisplayCopyDisplayMode = dlsym(CoreGraphicsLibrary, "CGDisplayCopyDisplayMode");
	if (!bc_func_ptrs.CGDisplayCopyDisplayMode)
		return 1;
	
	bc_func_ptrs.CGBeginDisplayConfiguration = dlsym(CoreGraphicsLibrary, "CGBeginDisplayConfiguration");
	if (!bc_func_ptrs.CGBeginDisplayConfiguration)
		return 1;
	
	bc_func_ptrs.CGConfigureDisplayWithDisplayMode = dlsym(CoreGraphicsLibrary, "CGConfigureDisplayWithDisplayMode");
	if (!bc_func_ptrs.CGConfigureDisplayWithDisplayMode)
		return 1;
	
	bc_func_ptrs.CGCompleteDisplayConfiguration = dlsym(CoreGraphicsLibrary, "CGCompleteDisplayConfiguration");
	if (!bc_func_ptrs.CGCompleteDisplayConfiguration)
		return 1;
	
	bc_func_ptrs.CGCancelDisplayConfiguration = dlsym(CoreGraphicsLibrary, "CGCancelDisplayConfiguration");
	if (!bc_func_ptrs.CGCancelDisplayConfiguration)
		return 1;
	
	bc_func_ptrs.CGDisplayCopyAllDisplayModes = dlsym(CoreGraphicsLibrary, "CGDisplayCopyAllDisplayModes");
	if (!bc_func_ptrs.CGDisplayCopyAllDisplayModes)
		return 1;
	
	bc_func_ptrs.CGDisplayModeGetWidth = dlsym(CoreGraphicsLibrary, "CGDisplayModeGetWidth");
	if (!bc_func_ptrs.CGDisplayModeGetWidth)
		return 1;
	
	bc_func_ptrs.CGDisplayModeGetHeight = dlsym(CoreGraphicsLibrary, "CGDisplayModeGetHeight");
	if (!bc_func_ptrs.CGDisplayModeGetHeight)
		return 1;
	
	bc_func_ptrs.CGDisplayModeGetIOFlags = dlsym(CoreGraphicsLibrary, "CGDisplayModeGetIOFlags");
	if (!bc_func_ptrs.CGDisplayModeGetIOFlags)
		return 1;
	
	return 0;
}

static unsigned long long monotonictime()
{
	unsigned long long curtime;
	unsigned long long ret;

	curtime = mach_absolute_time();

	ret = (curtime/tbinfo.denom)*tbinfo.numer;
	ret += ((curtime%tbinfo.denom)*tbinfo.numer)/tbinfo.denom;

	return ret;
}

double Sys_DoubleTime(void)
{
	return (double)monotonictime() / 1000000000.0;
}

void Sys_MicroSleep(unsigned int microseconds)
{
        usleep(microseconds);
}

void Sys_RandomBytes(void *target, unsigned int numbytes)
{
        ssize_t s;

        while(numbytes)
        {
                s = read(randomfd, target, numbytes);
                if (s < 0)
                {
                        Sys_Error("Could not read from randomfd");
                }

                numbytes -= s;
                target += s;
        }
}

unsigned long long Sys_IntTime()
{
	return monotonictime() / 1000;
}

const char *Sys_GetRODataPath(void)
{
	char *ret = NULL;
	
	ret = malloc([[[NSBundle mainBundle] resourcePath] lengthOfBytesUsingEncoding:NSUTF8StringEncoding] + strlen("/data") + 1);
	if (ret)
	{
		sprintf(ret, "%s/data", [[[NSBundle mainBundle] resourcePath] UTF8String]);
	}
	
	return ret;
}

const char *Sys_GetUserDataPath(void)
{
	char *ret = NULL;
	NSString *home = NSHomeDirectory();
	
	if (home)
	{
		ret = malloc([home lengthOfBytesUsingEncoding:NSUTF8StringEncoding] + strlen("/Library/Application Support/Fodquake") + 1);
		if (ret)
		{
			sprintf(ret, "%s/Library/Application Support/Fodquake", [home UTF8String]);
		}
	}
	
	return ret;
}

const char *Sys_GetLegacyDataPath(void)
{
	return NULL;
}

void Sys_FreePathString(const char *x)
{
	free((void*)x);
}

char *Sys_ConsoleInput(void)
{
	return 0;
}

void Sys_Printf(char *fmt, ...)
{
}

void Sys_Quit(void)
{
	if (CoreGraphicsLibrary)
	{
		dlclose(CoreGraphicsLibrary);
		CoreGraphicsLibrary = NULL;
	}
    
	exit(0);
}

void Sys_Error(char *error, ...)
{
	va_list va;
	char string[1024];

	va_start(va, error);
	vsnprintf(string, sizeof(string), error, va);
	va_end(va);
	fprintf(stderr, "Error: %s\n", string);

	Host_Shutdown();
	
	if (NSApp)
	{
		if (pool)
		{
			NSString *user = [[NSString alloc] initWithCString:string encoding:NSASCIIStringEncoding];
			if (user)
			{
				NSAlert *alert = [[NSAlert alloc] init];
				if (alert)
				{
					[alert setMessageText:@"Fodquake error"];
					[alert setInformativeText:user];
					[alert runModal];
					[alert release];
				}
				
				[user release];
			}
			
			[pool release];
		}
		
		[NSApp release];
	}
    
	if (CoreGraphicsLibrary)
	{
		dlclose(CoreGraphicsLibrary);
		CoreGraphicsLibrary = NULL;
	}

	exit(1);
}

void Sys_CvarInit(void)
{
}

void Sys_Init(void)
{
}

int main(int argc, char **argv)
{
	double time, oldtime, newtime;
	
	[NSApplication sharedApplication];
	if (NSApp == nil)
		Sys_Error("Error init shared application");
	
	pool = [[NSAutoreleasePool alloc] init];
	if (pool == nil)
		Sys_Error("Error creating auto release pool");

	mach_timebase_info(&tbinfo);

	COM_InitArgv(argc, argv);

	if (NSAppKitVersionNumber < NSAppKitVersionNumber10_5)
		Sys_Error("Fodquake requires Mac OS X 10.5 or higher");
	
	if (load_missing_osx_symbols() != 0)
		Sys_Error("Error loading CoreGraphics symbols");
	
	randomfd = open("/dev/urandom", O_RDONLY);
	if (randomfd == -1)
		Sys_Error("Unable to open /dev/urandom");

	Host_Init(argc, argv);

	oldtime = Sys_DoubleTime();
	while(1)
	{
		newtime = Sys_DoubleTime();
		time = newtime - oldtime;
		oldtime = newtime;

		Host_Frame(time);
	}

	if (CoreGraphicsLibrary)
	{
		dlclose(CoreGraphicsLibrary);
		CoreGraphicsLibrary = NULL;
	}
	
	return 0;
}

