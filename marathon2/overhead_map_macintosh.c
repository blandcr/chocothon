/*
OVERHEAD_MAP_MAC.C
Monday, August 28, 1995 1:41:36 PM  (Jason)
*/

static TextSpec overhead_map_name_font;

enum
{
	finfMAP= 129
};

struct annotation_definition
{
	RGBColor color;
	short font, face;
	
	short sizes[OVERHEAD_MAP_MAXIMUM_SCALE-OVERHEAD_MAP_MINIMUM_SCALE+1];
};

#define NUMBER_OF_ANNOTATION_DEFINITIONS (sizeof(annotation_definitions)/sizeof(struct annotation_definition))
struct annotation_definition annotation_definitions[]=
{
	{{0, 65535, 0}, monaco, bold, {5, 9, 12, 18}},
};

void initialize_overhead_map(
	void)
{
	TextSpec overhead_map_annotation_font;
	
	// get the overhead map name font index
	GetNewTextSpec(&overhead_map_name_font, finfMAP, 0);
	// get the annotation font only
	GetNewTextSpec(&overhead_map_annotation_font, finfMAP, 1);
	annotation_definitions[0].font= overhead_map_annotation_font.font;
}

/* ---------- private code */

#define NUMBER_OF_POLYGON_COLORS (sizeof(polygon_colors)/sizeof(RGBColor))
static RGBColor polygon_colors[]=
{
	{0, 12000, 0},
	{30000, 0, 0},
	{14*256, 37*256, 63*256},
	{76*256, 27*256, 0},
	{137*256, 0, 137*256},
	{70*256, 90*256, 0},
	{32768, 32768, 0}
};

static void draw_overhead_polygon(
	short vertex_count,
	short *vertices,
	short color,
	short scale)
{
	PolyHandle polygon;
	world_point2d *vertex;
	short i;

	#pragma unused (scale)

	assert(color>=0&&color<NUMBER_OF_POLYGON_COLORS);

	polygon= OpenPoly();
	vertex= &get_endpoint_data(vertices[vertex_count-1])->transformed;
	MoveTo(vertex->x, vertex->y);
	for (i=0;i<vertex_count;++i)
	{
		vertex= &get_endpoint_data(vertices[i])->transformed;
		LineTo(vertex->x, vertex->y);
	}
	ClosePoly();
	PenSize(1, 1);
	RGBForeColor(polygon_colors+color);
	FillPoly(polygon, &qd.black);
	KillPoly(polygon);

	return;
}

struct line_definition
{
	RGBColor color;
	short pen_sizes[OVERHEAD_MAP_MAXIMUM_SCALE-OVERHEAD_MAP_MINIMUM_SCALE+1];
};

#define NUMBER_OF_LINE_DEFINITIONS (sizeof(line_definitions)/sizeof(struct line_definition))
struct line_definition line_definitions[]=
{
	{{0, 65535, 0}, {1, 2, 2, 4}},
	{{0, 40000, 0}, {1, 1, 1, 2}},
	{{65535, 0, 0}, {1, 2, 2, 4}}
};

static void draw_overhead_line(
	short line_index,
	short color,
	short scale)
{
	struct line_data *line= get_line_data(line_index);
	world_point2d *vertex1= &get_endpoint_data(line->endpoint_indexes[0])->transformed;
	world_point2d *vertex2= &get_endpoint_data(line->endpoint_indexes[1])->transformed;
	struct line_definition *definition;

	assert(color>=0&&color<NUMBER_OF_LINE_DEFINITIONS);
	definition= line_definitions+color;
	
	RGBForeColor(&definition->color);
	PenSize(definition->pen_sizes[scale-OVERHEAD_MAP_MINIMUM_SCALE], definition->pen_sizes[scale-OVERHEAD_MAP_MINIMUM_SCALE]);
	MoveTo(vertex1->x, vertex1->y);
	LineTo(vertex2->x, vertex2->y);
	
#ifdef RENDER_DEBUG
	if (scale==OVERHEAD_MAP_MAXIMUM_SCALE)
	{
		world_point2d location;
		
		TextFont(monaco);
		TextFace(normal);
		TextSize(9);
		RGBForeColor(&rgb_white);

		location.x= (vertex1->x+vertex2->x)/2;
		location.y= (vertex1->y+vertex2->y)/2;
		psprintf(temporary, "%d", line_index);
		MoveTo(location.x, location.y);
		DrawString(temporary);
		
		psprintf(temporary, "%d", line->endpoint_indexes[0]);
		MoveTo(vertex1->x, vertex1->y);
		DrawString(temporary);

		psprintf(temporary, "%d", line->endpoint_indexes[1]);
		MoveTo(vertex2->x, vertex2->y);
		DrawString(temporary);
	}
#endif

	return;
}

struct thing_definition
{
	RGBColor color;
	short shape;
	short radii[OVERHEAD_MAP_MAXIMUM_SCALE-OVERHEAD_MAP_MINIMUM_SCALE+1];
};

struct thing_definition thing_definitions[NUMBER_OF_THINGS]=
{
	{{0, 0, 65535}, _rectangle_thing, {1, 2, 4, 8}}, /* civilian */
	{{65535, 65535, 65535}, _rectangle_thing, {1, 2, 3, 4}}, /* item */
	{{65535, 0, 0}, _rectangle_thing, {1, 2, 4, 8}}, /* non-player monster */
	{{65535, 65535, 0}, _rectangle_thing, {1, 1, 2, 3}}, /* projectiles */
	{{65535, 0, 0}, _circle_thing, {8, 16, 16, 16}}
};

static void draw_overhead_thing(
	world_point2d *center,
	angle facing,
	short color,
	short scale)
{
	Rect bounds;
	short radius;
	struct thing_definition *definition;

	#pragma unused (facing)

	assert(color>=0&&color<NUMBER_OF_THINGS);
	definition= thing_definitions+color;
	radius= definition->radii[scale-OVERHEAD_MAP_MINIMUM_SCALE];

	RGBForeColor(&definition->color);
	SetRect(&bounds, center->x-(radius>>1), center->y-(radius>>1), center->x+radius, center->y+radius);
	switch (definition->shape)
	{
		case _rectangle_thing:
			PaintRect(&bounds);
			break;
		case _circle_thing:
			PenSize(2, 2);
			FrameOval(&bounds);
			break;
		default:
			halt();
	}

	return;
}

struct entity_definition
{
	short front, rear, rear_theta;
};

#define NUMBER_OF_ENTITY_DEFINITIONS (sizeof(entity_definitions)/sizeof(struct entity_definition))
struct entity_definition entity_definitions[]=
{
	{16, 10, (7*NUMBER_OF_ANGLES)/20},
	{16, 10, (7*NUMBER_OF_ANGLES)/20},
	{16, 10, (7*NUMBER_OF_ANGLES)/20},
	{16, 10, (7*NUMBER_OF_ANGLES)/20},
	{16, 10, (7*NUMBER_OF_ANGLES)/20},
	{16, 10, (7*NUMBER_OF_ANGLES)/20},
	{16, 10, (7*NUMBER_OF_ANGLES)/20},
	{16, 10, (7*NUMBER_OF_ANGLES)/20},
};

static void draw_overhead_player(
	world_point2d *center,
	angle facing,
	short color,
	short scale)
{
	short i;
	PolyHandle polygon;
	world_point2d triangle[3];
	struct entity_definition *definition;
	RGBColor rgb_color;

	assert(color>=0&&color<NUMBER_OF_ENTITY_DEFINITIONS);
	definition= entity_definitions+color;

	/* Use our universal clut */
	_get_player_color(color, &rgb_color);

	triangle[0]= triangle[1]= triangle[2]= *center;
	translate_point2d(triangle+0, definition->front>>(OVERHEAD_MAP_MAXIMUM_SCALE-scale), facing);
	translate_point2d(triangle+1, definition->rear>>(OVERHEAD_MAP_MAXIMUM_SCALE-scale), normalize_angle(facing+definition->rear_theta));
	translate_point2d(triangle+2, definition->rear>>(OVERHEAD_MAP_MAXIMUM_SCALE-scale), normalize_angle(facing-definition->rear_theta));
	
	if (scale < 2)
	{
		RGBForeColor(&rgb_color);
		MoveTo(triangle[0].x, triangle[0].y);
		if (triangle[1].x != triangle[0].x || triangle[1].y != triangle[0].y)
			LineTo(triangle[1].x, triangle[1].y);
		else
			LineTo(triangle[2].x, triangle[2].y);
	}
	else
	{
		polygon= OpenPoly();
		MoveTo(triangle[2].x, triangle[2].y);
		for (i=0;i<3;++i) LineTo(triangle[i].x, triangle[i].y);
		ClosePoly();
		PenSize(1, 1);
		RGBForeColor(&rgb_color);
		FillPoly(polygon, &qd.black);
		KillPoly(polygon);
	}	
	
	return;
}

static void draw_overhead_annotation(
	world_point2d *location,
	short color,
	char *text,
	short scale)
{
	struct annotation_definition *definition;
	Str255 pascal_text;

	vassert(color>=0&&color<NUMBER_OF_ANNOTATION_DEFINITIONS, csprintf(temporary, "#%d is not a supported annotation type", color));
	definition= annotation_definitions+color;
	
	strcpy((char *)pascal_text, text);
	c2pstr((char *)pascal_text);
	MoveTo(location->x, location->y);
	TextFont(definition->font);
	TextFace(definition->face);
	TextSize(definition->sizes[scale-OVERHEAD_MAP_MINIMUM_SCALE]);
	RGBForeColor(&definition->color);
	DrawString(pascal_text);
	
	return;
}

static RGBColor map_name_color= {0, 0xffff, 0};

static void draw_map_name(
	struct overhead_map_data *data,
	char *name)
{
	Str255 pascal_name;
	
	strcpy((char *)pascal_name, name);
	c2pstr((char *)pascal_name);
	
	TextFont(overhead_map_name_font.font);
	TextFace(overhead_map_name_font.face);
	TextSize(overhead_map_name_font.size);
	RGBForeColor(&map_name_color);
	MoveTo(data->half_width - (StringWidth(pascal_name)>>1), 25);
	DrawString(pascal_name);
	
	return;
}
