/*

	progress.c
	Saturday, October 28, 1995 10:56:17 PM- rdm created.

*/

//#include "macintosh_cseries.h"
#include "progress.h"

enum {
	dialogPROGRESS= 10002,
	iPROGRESS_BAR= 1,
	iPROGRESS_MESSAGE
};

/* ------- structures */
struct progress_data {
	DialogPtr dialog;
	GrafPtr old_port;
	UserItemUPP progress_bar_upp;
};

/* ------ private prototypes */
static pascal void draw_distribute_progress(DialogPtr dialog, short item_num);

/* ------ globals */
struct progress_data progress_data;

/* ------ calls */
void open_progress_dialog(
	short message_id)
{
	Rect item_box;
	short item_type;
	Handle item_handle;
		
	progress_data.dialog= GetNewDialog(dialogPROGRESS, NULL, (WindowPtr) -1);
	assert(progress_data.dialog);
	progress_data.progress_bar_upp= NewUserItemProc(draw_distribute_progress);
	assert(progress_data.progress_bar_upp);

	GetPort(&progress_data.old_port);
	SetPort(progress_data.dialog);
	GetDItem(progress_data.dialog, iPROGRESS_BAR, &item_type, &item_handle, &item_box);
	SetDItem(progress_data.dialog, iPROGRESS_BAR, item_type, (Handle) progress_data.progress_bar_upp, &item_box);

	/* Set the message.. */
	set_progress_dialog_message(message_id);

	ShowWindow(progress_data.dialog);
	DrawDialog(progress_data.dialog);
	SetCursor(*(GetCursor(watchCursor)));

	return;
}

void set_progress_dialog_message(
	short message_id)
{
	short item_type;
	Rect bounds;
	Handle item_handle;

	assert(progress_data.dialog);
	GetDItem(progress_data.dialog, iPROGRESS_MESSAGE, &item_type, &item_handle, &bounds);
	getpstr(temporary, strPROGRESS_MESSAGES, message_id);
	SetIText(item_handle, (StringPtr)temporary);
	
	return;
}

void close_progress_dialog(
	void)
{
	SetPort(progress_data.old_port);

	SetCursor(&qd.arrow);
	DisposeDialog(progress_data.dialog);
	DisposeRoutineDescriptor(progress_data.progress_bar_upp);

	return;
}

void draw_progress_bar(
	long sent, 
	long total)
{
	Rect bounds;
	Handle item;
	short item_type;
	short width;
	
	GetDItem(progress_data.dialog, iPROGRESS_BAR, &item_type, &item, &bounds);
	width= (sent*RECTANGLE_WIDTH(&bounds))/total;
	
	bounds.right= bounds.left+width;
	RGBForeColor(system_colors+gray15Percent);
	PaintRect(&bounds);
	ForeColor(blackColor);

	return;
}

void reset_progress_bar(
	void)
{
	draw_distribute_progress(progress_data.dialog, iPROGRESS_BAR);

	return;
}

/* --------- private code */
static pascal void draw_distribute_progress(
	DialogPtr dialog, 
	short item_num)
{
	Rect item_box;
	short item_type;
	Handle item_handle;
	GrafPtr old_port;
	
	GetPort(&old_port);
	SetPort(dialog);
	
	GetDItem(dialog, item_num, &item_type, &item_handle, &item_box);
	PenNormal();
	RGBForeColor(system_colors+windowHighlight);
	PaintRect(&item_box);
	ForeColor(blackColor);
	InsetRect(&item_box, -1, -1);
	FrameRect(&item_box);

	SetPort(old_port);

	return;
}
