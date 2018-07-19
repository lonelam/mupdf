#ifndef MUPDF_FITZ_DISPLAY_LIST_H
#define MUPDF_FITZ_DISPLAY_LIST_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/geometry.h"
#include "mupdf/fitz/device.h"

typedef enum fz_display_command_e
{
	FZ_CMD_FILL_PATH,
	FZ_CMD_STROKE_PATH,
	FZ_CMD_CLIP_PATH,
	FZ_CMD_CLIP_STROKE_PATH,
	FZ_CMD_FILL_TEXT,
	FZ_CMD_STROKE_TEXT,
	FZ_CMD_CLIP_TEXT,
	FZ_CMD_CLIP_STROKE_TEXT,
	FZ_CMD_IGNORE_TEXT,
	FZ_CMD_FILL_SHADE,
	FZ_CMD_FILL_IMAGE,
	FZ_CMD_FILL_IMAGE_MASK,
	FZ_CMD_CLIP_IMAGE_MASK,
	FZ_CMD_POP_CLIP,
	FZ_CMD_BEGIN_MASK,
	FZ_CMD_END_MASK,
	FZ_CMD_BEGIN_GROUP,
	FZ_CMD_END_GROUP,
	FZ_CMD_BEGIN_TILE,
	FZ_CMD_END_TILE,
	FZ_CMD_RENDER_FLAGS,
	FZ_CMD_DEFAULT_COLORSPACES,
	FZ_CMD_BEGIN_LAYER,
	FZ_CMD_BEGIN_MCITEM,
	FZ_CMD_END_LAYER
} fz_display_command;


// The display list is a list of nodes.
//* Each node is a structure consisting of a bitfield (that packs into a
//* 32 bit word).
//* The different fields in the bitfield identify what information is
//* present in the node.
//*
//*	cmd:	What type of node this is.
//*
//*	size:	The number of sizeof(fz_display_node) bytes that this node's
//*		data occupies. (i.e. &node[node->size] = the next node in the
//*		chain; 0 for end of list).
//*
//*	rect:	0 for unchanged, 1 for present.
//*
//*	path:	0 for unchanged, 1 for present.
//*
//*	cs:	0 for unchanged
//*		1 for devicegray (color defaults to 0)
//*		2 for devicegray (color defaults to 1)
//*		3 for devicergb (color defaults to 0,0,0)
//*		4 for devicergb (color defaults to 1,1,1)
//*		5 for devicecmyk (color defaults to 0,0,0,0)
//*		6 for devicecmyk (color defaults to 0,0,0,1)
//*		7 for present (color defaults to 0)
//*
//*	color:	0 for unchanged color, 1 for present.
//*
//*	alpha:	0 for unchanged, 1 for solid, 2 for transparent, 3
//*		for alpha value present.
//*
//*	ctm:	0 for unchanged,
//*		1 for change ad
//*		2 for change bc
//*		4 for change ef.
//*
//*	stroke:	0 for unchanged, 1 for present.
//*
//*	flags:	Flags (node specific meanings)
//*
//* Nodes are packed in the order:
//* header, rect, colorspace, color, alpha, ctm, stroke_state, path, private data.
struct fz_display_node_s
{
	unsigned int cmd : 5;
	unsigned int size : 9;
	unsigned int rect : 1;
	unsigned int path : 1;
	unsigned int cs : 3;
	unsigned int color : 1;
	unsigned int alpha : 2;
	unsigned int ctm : 3;
	unsigned int stroke : 1;
	unsigned int flags : 6;
};

typedef struct fz_display_node_s fz_display_node;

/*
/* fz_list_item is a linked list to store tags for items in display list.
*/
struct fz_list_item_s {
	int start;//the list index after begin tag
	int end;//the list index on end tag
	int id;
	struct fz_list_item_s * next;
};
typedef struct fz_list_item_s fz_list_item;
/*
* allocate a method and set end:= -1, start:= start, next:=nullptr, tag := tag
*/
fz_list_item * fz_new_list_item(fz_context * ctx, int start, int mcid);

/*
Display list device -- record and play back device commands.
*/

/*
fz_display_list is a list containing drawing commands (text,
images, etc.). The intent is two-fold: as a caching-mechanism
to reduce parsing of a page, and to be used as a data
structure in multi-threading where one thread parses the page
and another renders pages.

Create a display list with fz_new_display_list, hand it over to
fz_new_list_device to have it populated, and later replay the
list (once or many times) by calling fz_run_display_list. When
the list is no longer needed drop it with fz_drop_display_list.
*/

struct fz_display_list_s
{
	fz_storable storable;
	fz_display_node *list;
	fz_rect mediabox;
	int max;
	int len;
};

typedef struct fz_display_list_s fz_display_list;


#define LIST_STACK_SIZE 96
struct fz_list_device_s
{
	fz_device super;

	fz_display_list *list;

	fz_path *path;
	float alpha;
	fz_matrix ctm;
	fz_stroke_state *stroke;
	fz_colorspace *colorspace;
	fz_color_params *color_params;
	float color[FZ_MAX_COLORS];
	fz_rect rect;

	int top;
	struct {
		fz_rect *update;
		fz_rect rect;
	} stack[LIST_STACK_SIZE];

	int itop;
	char istack[LIST_STACK_SIZE];

	fz_list_item * item_head, * item_tail;
	
	int tiled;
};

typedef struct fz_list_device_s fz_list_device;

/*
	fz_new_display_list: Create an empty display list.

	A display list contains drawing commands (text, images, etc.).
	Use fz_new_list_device for populating the list.

	mediabox: Bounds of the page (in points) represented by the display list.
*/
fz_display_list *fz_new_display_list(fz_context *ctx, fz_rect mediabox);

/*
	fz_new_list_device: Create a rendering device for a display list.

	When the device is rendering a page it will populate the
	display list with drawing commands (text, images, etc.). The
	display list can later be reused to render a page many times
	without having to re-interpret the page from the document file
	for each rendering. Once the device is no longer needed, free
	it with fz_drop_device.

	list: A display list that the list device takes ownership of.
*/
fz_device *fz_new_list_device(fz_context *ctx, fz_display_list *list);

/*
	fz_run_display_list: (Re)-run a display list through a device.

	list: A display list, created by fz_new_display_list and
	populated with objects from a page by running fz_run_page on a
	device obtained from fz_new_list_device.

	dev: Device obtained from fz_new_*_device.

	ctm: Transform to apply to display list contents. May include
	for example scaling and rotation, see fz_scale, fz_rotate and
	fz_concat. Set to fz_identity if no transformation is desired.

	scissor: Only the part of the contents of the display list
	visible within this area will be considered when the list is
	run through the device. This does not imply for tile objects
	contained in the display list.

	cookie: Communication mechanism between caller and library
	running the page. Intended for multi-threaded applications,
	while single-threaded applications set cookie to NULL. The
	caller may abort an ongoing page run. Cookie also communicates
	progress information back to the caller. The fields inside
	cookie are continually updated while the page is being run.
*/
void fz_run_display_list(fz_context *ctx, fz_display_list *list, fz_device *dev, fz_matrix ctm, fz_rect scissor, fz_cookie *cookie);

/*
	fz_keep_display_list: Keep a reference to a display list.
*/
fz_display_list *fz_keep_display_list(fz_context *ctx, fz_display_list *list);

/*
	fz_drop_display_list: Drop a reference to a display list, freeing it
	if the reference count reaches zero.
*/
void fz_drop_display_list(fz_context *ctx, fz_display_list *list);

/*
	fz_bound_display_list: Return the bounding box of the page recorded in a display list.
*/
fz_rect fz_bound_display_list(fz_context *ctx, fz_display_list *list);

/*
	Create a new image from a display list.

	w, h: The conceptual width/height of the image.

	transform: The matrix that needs to be applied to the given
	list to make it render to the unit square.

	list: The display list.
*/
fz_image *fz_new_image_from_display_list(fz_context *ctx, float w, float h, fz_display_list *list);

/*
	Check for a display list being empty

	list: The list to check.

	Returns true if empty, false otherwise.
*/
int fz_display_list_is_empty(fz_context *ctx, const fz_display_list *list);


#endif
