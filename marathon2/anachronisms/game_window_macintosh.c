/*

	macintosh_game_window.c
	Tuesday, August 29, 1995 6:09:18 PM- rdm created.

*/

#include "macintosh_cseries.h"

#include "map.h"
#include "shell.h"
#include "preferences.h"
#include "my32bqd.h"
#include "images.h"
#include "screen_drawing.h"
#include "screen_definitions.h"
#include "interface.h"
#include "screen.h"

extern GrafPtr screen_window;
extern GWorldPtr world_pixels;

extern void update_everything(short time_elapsed);

/* ------------- code begins! */
void draw_panels(
	void)
{
	struct screen_mode_data new_mode= graphics_preferences->screen_mode;
	PicHandle picture;
	Rect destination= {320, 0, 480, 640};
	Rect source= {0, 0, 160, 640};

	new_mode.acceleration= _no_acceleration;
	new_mode.size= _100_percent;
	new_mode.high_resolution= TRUE;
	change_screen_mode(&new_mode, FALSE);

	myLockPixels(world_pixels);

	picture= get_picture_resource_from_images(INTERFACE_PANEL_BASE);
	if(picture)
	{
		_set_port_to_gworld();
		SetOrigin(0, 320);

		HLock((Handle) picture);
		DrawPicture(picture, &destination);
		HUnlock((Handle) picture);

		update_everything(NONE);
		ReleaseResource((Handle) picture);
	
		SetOrigin(0, 0);
		_restore_port();
	} else {
		/* Either they don't have the picture, or they are out of memory.  Most likely no memory */
		alert_user(fatalError, strERRORS, outOfMemory, ResError());
	}

	/* Note that you don't get here if the picture failed.. */
	{
		GrafPtr old_port;
		RGBColor old_forecolor, old_backcolor;
		
		GetPort(&old_port);
		SetPort(screen_window);

		GetForeColor(&old_forecolor);
		GetBackColor(&old_backcolor);
		RGBForeColor(&rgb_black);
		RGBBackColor(&rgb_white);
		
		/* Slam it to the screen. */
		CopyBits((BitMapPtr)*world_pixels->portPixMap, &screen_window->portBits, //(BitMapPtr)*screen_pixmap,
			&source, &destination, srcCopy, (RgnHandle) NULL);
		
		RGBForeColor(&old_forecolor);
		RGBBackColor(&old_backcolor);
		SetPort(old_port);
	}
	myUnlockPixels(world_pixels);

	change_screen_mode(&graphics_preferences->screen_mode, FALSE);
}