/*
SCENERY.H
Thursday, December 1, 1994 12:19:13 PM  (Jason)
*/

/* ---------- prototypes/SCENERY.C */

void initialize_scenery(void);

short new_scenery(struct object_location *location, short scenery_type);

void animate_scenery(void);

void randomize_scenery_shapes(void);

void get_scenery_dimensions(short scenery_type, world_distance *radius, world_distance *height);
void damage_scenery(short object_index);
