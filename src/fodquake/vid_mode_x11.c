#include <stdlib.h>

#include "vid_mode_x11.h"
#include "vid_mode_xrandr.h"
#include "vid_mode_xf86vm.h"

static int xrandr;
static int xf86vm;

void vidmode_init()
{
	if (xrandr_init())
		xrandr = 1;

	if (xf86vm_init())
		xf86vm = 1;
}

void vidmode_shutdown()
{
	if (xrandr)
	{
		xrandr_shutdown();
		xrandr = 0;
	}

	if (xf86vm)
	{
		xf86vm_shutdown();
		xf86vm = 0;
	}
}

const char * const *Sys_Video_GetModeList(void)
{
	if (xrandr)
		return xrandr_GetModeList();

	if (xf86vm)
		return xf86vm_GetModeList();

	return 0;
}

void Sys_Video_FreeModeList(const char * const *displaymodes)
{
	unsigned int i;

	for(i=0;displaymodes[i];i++)
	{
		free((void *)displaymodes[i]);
	}

	free((void *)displaymodes);
}

const char *Sys_Video_GetModeDescription(const char *mode)
{
	const char *ret;

	ret = 0;

	if (xrandr)
		ret = xrandr_GetModeDescription(mode);

	if (xf86vm && ret == 0)
		ret = xf86vm_GetModeDescription(mode);

	return ret;
}

void Sys_Video_FreeModeDescription(const char *modedescription)
{
	free((void *)modedescription);
}

