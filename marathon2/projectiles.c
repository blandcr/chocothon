/*
PROJECTILES.C
Friday, May 27, 1994 10:54:44 AM

Friday, July 15, 1994 12:28:36 PM
	added maximum range.
Monday, February 6, 1995 2:46:08 AM  (Jason')
	persistent/virulent projectiles; media detonation effects.
Tuesday, June 13, 1995 12:07:00 PM  (Jason)
	non-melee projectiles must start above media.
Monday, June 26, 1995 8:52:32 AM  (Jason)
	bouncing projectiles
Tuesday, August 1, 1995 3:31:08 PM  (Jason)
	guided projectiles bite on low levels
Thursday, August 17, 1995 9:35:13 AM  (Jason)
	wandering projectiles
Thursday, October 5, 1995 10:19:48 AM  (Jason)
	until we fix it, calling translate_projectile() is too time consuming on high levels.
Friday, October 6, 1995 8:35:04 AM  (Jason)
	simpler guided projectile model.
*/

#include "cseries.h"
#include "map.h"
#include "interface.h"
#include "effects.h"
#include "monsters.h"
#include "projectiles.h"
#include "player.h"
#include "scenery.h"
#include "media.h"
#include "game_sound.h"
#include "items.h"

/*
//translate_projectile() must set _projectile_hit_landscape bit
*/

//#ifdef mpwc
//#pragma segment objects
//#endif

/* ---------- constants */

enum
{
	GRAVITATIONAL_ACCELERATION= 1, // per tick
	
	WANDER_MAGNITUDE= WORLD_ONE/TICKS_PER_SECOND,
	
	MINIMUM_REBOUND_VELOCITY= GRAVITATIONAL_ACCELERATION*TICKS_PER_SECOND/3
};

enum /* translate_projectile() flags */
{
	_flyby_of_current_player= 0x0001,
	_projectile_hit= 0x0002,
	_projectile_hit_monster= 0x0004, // monster_index in *obstruction_index
	_projectile_hit_floor= 0x0008, // polygon_index in *obstruction_index
	_projectile_hit_media= 0x0010, // polygon_index in *obstruction_index
	_projectile_hit_landscape= 0x0020,
	_projectile_hit_scenery= 0x0040
};

enum /* things the projectile can hit in detonate_projectile() */
{
	_hit_nothing,
	_hit_floor,
	_hit_media,
	_hit_ceiling,
	_hit_wall,
	_hit_monster,
	_hit_scenery
};

#define MAXIMUM_PROJECTILE_ELEVATION (QUARTER_CIRCLE/2)

/* ---------- structures */

/* ---------- private prototypes */

#ifdef DEBUG
struct projectile_definition *get_projectile_definition(short type);
#else
#define get_projectile_definition(i) (projectile_definitions+(i))
#endif

/* ---------- globals */

/* import projectile definition structures, constants and globals */
#include "projectile_definitions.h"

/* if copy-protection fails, these are replaced externally with the rocket and the rifle bullet, respectively */
short alien_projectile_override= NONE;
short human_projectile_override= NONE;

/* ---------- private prototypes */

static short adjust_projectile_type(world_point3d *origin, short polygon_index, short type,
	short owner_index, short owner_type, short intended_target_index, fixed damage_scale);

static void update_guided_projectile(short projectile_index);

static void get_projectile_media_detonation_effect(short polygon_index, short media_detonation_effect,
	short *detonation_effect);

static word translate_projectile(short type, world_point3d *old_location, short old_polygon_index,
	world_point3d *new_location, short *new_polygon_index, short owner_index,
	short *obstruction_index);

/* ---------- code */

/* FALSE means don�t fire this (it�s in a floor or ceiling or outside of the map), otherwise
	the monster that was intersected first (or NONE) is returned in target_index */
boolean preflight_projectile(
	world_point3d *origin,
	short origin_polygon_index,
	world_point3d *destination,
	angle delta_theta,
	short type,
	short owner,
	short owner_type,
	short *obstruction_index)
{
	boolean legal_projectile= FALSE;
	struct projectile_definition *definition= get_projectile_definition(type);
	
	#pragma unused (delta_theta)
	
	/* will be used when we truly preflight projectiles */
	#pragma unused (owner_type)
	
	if (origin_polygon_index!=NONE)
	{
		world_distance dx= destination->x-origin->x, dy= destination->y-origin->y;
		angle elevation= arctangent(isqrt(dx*dx + dy*dy), destination->z-origin->z);
		
		if (elevation<MAXIMUM_PROJECTILE_ELEVATION || elevation>FULL_CIRCLE-MAXIMUM_PROJECTILE_ELEVATION)
		{
			struct polygon_data *origin_polygon= get_polygon_data(origin_polygon_index);
			
			if (origin->z>origin_polygon->floor_height && origin->z<origin_polygon->ceiling_height &&
				(origin_polygon->media_index==NONE || (definition->flags&_penetrates_media) || origin->z>get_media_data(origin_polygon->media_index)->height))
			{
				/* make sure it hits something */
				word flags= translate_projectile(type, origin, origin_polygon_index, destination, (short *) NULL, owner, obstruction_index);
				
				*obstruction_index= (flags&_projectile_hit_monster) ? get_object_data(*obstruction_index)->permutation : NONE;
				legal_projectile= TRUE;
			}
		}
	}
	
	return legal_projectile;
}

// pointless if not an area-of-effect weapon
void detonate_projectile(
	world_point3d *origin,
	short polygon_index,
	short type,
	short owner_index,
	short owner_type,
	fixed damage_scale)
{
	struct projectile_definition *definition= get_projectile_definition(type);
	struct damage_definition *damage= &definition->damage;
	
	damage->scale= damage_scale;
	damage_monsters_in_radius(NONE, owner_index, owner_type, origin, polygon_index,
		definition->area_of_effect, damage);
	if (definition->detonation_effect!=NONE) new_effect(origin, polygon_index, definition->detonation_effect, 0);

	return;
}

short new_projectile(
	world_point3d *origin,
	short polygon_index,
	world_point3d *vector,
	angle delta_theta, /* ��theta is added (in a circle) to the angle before firing */
	short type,
	short owner_index,
	short owner_type,
	short intended_target_index, /* can be NONE */
	fixed damage_scale)
{
	struct projectile_definition *definition;
	struct projectile_data *projectile;
	short projectile_index;

	type= adjust_projectile_type(origin, polygon_index, type, owner_index, owner_type, intended_target_index, damage_scale);
	definition= get_projectile_definition(type);

	for (projectile_index= 0, projectile= projectiles; projectile_index<MAXIMUM_PROJECTILES_PER_MAP;
		++projectile_index, ++projectile)
	{
		if (SLOT_IS_FREE(projectile))
		{
			angle facing, elevation;
			short object_index;
			struct object_data *object;

			facing= arctangent(vector->x, vector->y);
			elevation= arctangent(isqrt(vector->x*vector->x+vector->y*vector->y), vector->z);
			if (delta_theta)
			{
				if (!(definition->flags&_no_horizontal_error)) facing= normalize_angle(facing+random()%(2*delta_theta)-delta_theta);
				if (!(definition->flags&_no_vertical_error)) elevation= (definition->flags&_positive_vertical_error) ? normalize_angle(elevation+random()%delta_theta) :
					normalize_angle(elevation+random()%(2*delta_theta)-delta_theta);
			}
			
			object_index= new_map_object3d(origin, polygon_index, definition->collection==NONE ? NONE : BUILD_DESCRIPTOR(definition->collection, definition->shape), facing);
			if (object_index!=NONE)
			{
				object= get_object_data(object_index);
				
				projectile->type= (definition->flags&_alien_projectile) ?
					(alien_projectile_override==NONE ? type : alien_projectile_override) :
					(human_projectile_override==NONE ? type : human_projectile_override);
				projectile->object_index= object_index;
				projectile->owner_index= owner_index;
				projectile->target_index= intended_target_index;
				projectile->owner_type= owner_type;
				projectile->flags= 0;
				projectile->gravity= 0;
				projectile->ticks_since_last_contrail= projectile->contrail_count= 0;
				projectile->elevation= elevation;
				projectile->distance_travelled= 0;
				projectile->damage_scale= damage_scale;
				MARK_SLOT_AS_USED(projectile);

				SET_OBJECT_OWNER(object, _object_is_projectile);
				object->sound_pitch= definition->sound_pitch;
			}
			else
			{
				projectile_index= NONE;
			}
			
			break;
		}
	}
	if (projectile_index==MAXIMUM_PROJECTILES_PER_MAP) projectile_index= NONE;
	
	return projectile_index;
}

/* assumes �t==1 tick */
void move_projectiles(
	void)
{
	struct projectile_data *projectile;
	short projectile_index;
	
	for (projectile_index=0,projectile=projectiles;projectile_index<MAXIMUM_PROJECTILES_PER_MAP;++projectile_index,++projectile)
	{
		if (SLOT_IS_USED(projectile))
		{
			struct object_data *object= get_object_data(projectile->object_index);
			
//			if (!OBJECT_IS_INVISIBLE(object))
			{
				struct projectile_definition *definition= get_projectile_definition(projectile->type);
				short old_polygon_index= object->polygon;
				world_point3d new_location, old_location;
				short obstruction_index, new_polygon_index;
				
				new_location= old_location= object->location;
	
				/* update our object�s animation */
				animate_object(projectile->object_index);
				
				/* if we�re supposed to end when our animation loops, check this condition */
				if ((definition->flags&_stop_when_animation_loops) && (GET_OBJECT_ANIMATION_FLAGS(object)&_obj_last_frame_animated))
				{
					remove_projectile(projectile_index);
				}
				else
				{
					world_distance speed= definition->speed;
					unsigned long adjusted_definition_flags= 0;
					word flags;
					
					/* base alien projectile speed on difficulty level */
					if (definition->flags&_alien_projectile)
					{
						switch (dynamic_world->game_information.difficulty_level)
						{
							case _wuss_level: speed-= speed>>3; break;
							case _easy_level: speed-= speed>>4; break;
							case _major_damage_level: speed+= speed>>3; break;
							case _total_carnage_level: speed+= speed>>2; break;
						}
					}
	
					/* if this is a guided projectile with a valid target, update guidance system */				
					if ((definition->flags&_guided) && projectile->target_index!=NONE && (dynamic_world->tick_count&1)) update_guided_projectile(projectile_index);
					
					if (PROJECTILE_HAS_CROSSED_MEDIA_BOUNDARY(projectile)) adjusted_definition_flags= _penetrates_media;
					
					/* move the projectile and check for collisions; if we didn�t detonate move the
						projectile and check to see if we need to leave a contrail */
					if ((definition->flags&_affected_by_half_gravity) && (dynamic_world->tick_count&1)) projectile->gravity-= GRAVITATIONAL_ACCELERATION;
					if (definition->flags&_affected_by_gravity) projectile->gravity-= GRAVITATIONAL_ACCELERATION;
					if (definition->flags&_doubly_affected_by_gravity) projectile->gravity-= 2*GRAVITATIONAL_ACCELERATION;
					new_location.z+= projectile->gravity;
					translate_point3d(&new_location, speed, object->facing, projectile->elevation);
					if (definition->flags&_vertical_wander) new_location.z+= (random()&1) ? WANDER_MAGNITUDE : -WANDER_MAGNITUDE;
					if (definition->flags&_horizontal_wander) translate_point3d(&new_location, (random()&1) ? WANDER_MAGNITUDE : -WANDER_MAGNITUDE, NORMALIZE_ANGLE(object->facing+QUARTER_CIRCLE), 0);
					definition->flags^= adjusted_definition_flags;
					flags= translate_projectile(projectile->type, &old_location, object->polygon, &new_location, &new_polygon_index, projectile->owner_index, &obstruction_index);
					definition->flags^= adjusted_definition_flags;
					
					if (flags&_projectile_hit)
					{
						if ((flags&_projectile_hit_floor) && (definition->flags&_rebounds_from_floor) &&
							projectile->gravity<-MINIMUM_REBOUND_VELOCITY)
						{
							play_object_sound(projectile->object_index, definition->rebound_sound);
							projectile->gravity= - projectile->gravity + (projectile->gravity>>2); /* 0.75 */
						}
						else
						{
 							short monster_obstruction_index= (flags&_projectile_hit_monster) ? get_object_data(obstruction_index)->permutation : NONE;
							boolean destroy_persistent_projectile= FALSE;
							
							if (flags&_projectile_hit_scenery) damage_scenery(obstruction_index);
							
							/* cause damage, if we can */
							if (!PROJECTILE_HAS_CAUSED_DAMAGE(projectile))
							{
								struct damage_definition *damage= &definition->damage;
								
								damage->scale= projectile->damage_scale;
								if (definition->flags&_becomes_item_on_detonation)
								{
									if (monster_obstruction_index==NONE)
									{
										struct object_location location;
										
										location.p= object->location, location.p.z= 0;
										location.polygon_index= object->polygon;
										location.yaw= location.pitch= 0;
										location.flags= 0;
										new_item(&location, projectile->permutation);
										
										destroy_persistent_projectile= TRUE;
									}
									else
									{
										if (MONSTER_IS_PLAYER(get_monster_data(monster_obstruction_index)))
										{
											destroy_persistent_projectile= try_and_add_player_item(monster_index_to_player_index(monster_obstruction_index), projectile->permutation);
										}
									}
								}
								else
								{
									if (definition->area_of_effect)
									{
										damage_monsters_in_radius(monster_obstruction_index, projectile->owner_index, projectile->owner_type, &old_location, object->polygon, definition->area_of_effect, damage);
									}
									else
									{
										if (monster_obstruction_index!=NONE) damage_monster(monster_obstruction_index, projectile->owner_index, projectile->owner_type, &old_location, damage);
									}
								}
							}
								
							if ((definition->flags&_persistent) && !destroy_persistent_projectile)
							{
								SET_PROJECTILE_DAMAGE_STATUS(projectile, TRUE);
							}
							else
							{
								short detonation_effect= definition->detonation_effect;
								
								if (monster_obstruction_index!=NONE)
								{
									if (definition->flags&_bleeding_projectile)
									{
										detonation_effect= get_monster_impact_effect(monster_obstruction_index);
									}
									if (definition->flags&_melee_projectile)
									{
										short new_detonation_effect= get_monster_melee_impact_effect(monster_obstruction_index);
										if (new_detonation_effect!=NONE) detonation_effect= new_detonation_effect;
									}
								}
								if (flags&_projectile_hit_media) get_media_detonation_effect(get_polygon_data(obstruction_index)->media_index, definition->media_detonation_effect, &detonation_effect);
								if (flags&_projectile_hit_landscape) detonation_effect= NONE;
								
								if (detonation_effect!=NONE) new_effect(&new_location, new_polygon_index, detonation_effect, object->facing);
								
								if (!(definition->flags&_projectile_passes_media_boundary) || !(flags&_projectile_hit_media))
								{
									if ((definition->flags&_persistent_and_virulent) && !destroy_persistent_projectile && monster_obstruction_index!=NONE)
									{
										projectile->owner_index= monster_obstruction_index; /* keep going, but don�t hit this target again */
									}
									else
									{
										remove_projectile(projectile_index);
									}
								}
								else
								{
									SET_PROJECTILE_CROSSED_MEDIA_BOUNDARY_STATUS(projectile, TRUE);
								}
							}
						}
					}
					else
					{
						/* move to the new_polygon_index */
						translate_map_object(projectile->object_index, &new_location, new_polygon_index);
						
						/* should we leave a contrail at our old location? */
						if ((projectile->ticks_since_last_contrail+=1)>=definition->ticks_between_contrails)
						{
							if (definition->maximum_contrails==NONE || projectile->contrail_count<definition->maximum_contrails)
							{
								projectile->contrail_count+= 1;
								projectile->ticks_since_last_contrail= 0;
								if (definition->contrail_effect!=NONE) new_effect(&old_location, old_polygon_index, definition->contrail_effect, object->facing);
							}
						}
		
						if ((flags&_flyby_of_current_player) && !PROJECTILE_HAS_MADE_A_FLYBY(projectile))
						{
							SET_PROJECTILE_FLYBY_STATUS(projectile, TRUE);
							play_object_sound(projectile->object_index, definition->flyby_sound);
						}
		
						/* if we have a maximum range and we have exceeded it then remove the projectile */
						if (definition->maximum_range!=NONE)
						{
							if ((projectile->distance_travelled+= speed)>=definition->maximum_range)
							{
								remove_projectile(projectile_index);
							}
						}
					}
				}
			}
		}
	}
	
	return;
}

void remove_projectile(
	short projectile_index)
{
	struct projectile_data *projectile= get_projectile_data(projectile_index);
	
	remove_map_object(projectile->object_index);
	MARK_SLOT_AS_FREE(projectile);
	
	return;
}

void remove_all_projectiles(
	void)
{
	struct projectile_data *projectile;
	short projectile_index;
	
	for (projectile_index=0,projectile=projectiles;projectile_index<MAXIMUM_PROJECTILES_PER_MAP;++projectile_index,++projectile)
	{
		if (SLOT_IS_USED(projectile)) remove_projectile(projectile_index);
	}
	
	return;
}

/* when a given monster is deactivated (or killed), all his active projectiles should become
	ownerless (or all sorts of neat little problems can occur) */
void orphan_projectiles(
	short monster_index)
{
	struct projectile_data *projectile;
	short projectile_index;

	/* first, adjust all current projectile's .owner fields */
	for (projectile_index=0,projectile=projectiles;projectile_index<MAXIMUM_PROJECTILES_PER_MAP;++projectile_index,++projectile)
	{
		if (projectile->owner_index==monster_index) projectile->owner_index= NONE;
		if (projectile->target_index==monster_index) projectile->target_index= NONE;
	}

	return;
}

void load_projectile_sounds(
	short projectile_type)
{
	if (projectile_type!=NONE)
	{
		struct projectile_definition *definition= get_projectile_definition(projectile_type);
		
		load_sound(definition->flyby_sound);
		load_sound(definition->rebound_sound);
	}
	
	return;
}

void mark_projectile_collections(
	short projectile_type,
	boolean loading)
{
	if (projectile_type!=NONE)
	{
		struct projectile_definition *definition= get_projectile_definition(projectile_type);

		/* If the projectile is not invisible */
		if (definition->collection!=NONE)
		{
			/* mark the projectile collection */
			loading ? mark_collection_for_loading(definition->collection) : mark_collection_for_unloading(definition->collection);
		}
		
		/* mark the projectile�s effect�s collection */
		mark_effect_collections(definition->detonation_effect, loading);
		mark_effect_collections(definition->contrail_effect, loading);
	}
	
	return;
}

#ifdef DEBUG
struct projectile_data *get_projectile_data(
	short projectile_index)
{
	struct projectile_data *projectile;
	
	vassert(projectile_index>=0&&projectile_index<MAXIMUM_PROJECTILES_PER_MAP, csprintf(temporary, "projectile index #%d is out of range", projectile_index));
	
	projectile= projectiles+projectile_index;
	vassert(SLOT_IS_USED(projectile), csprintf(temporary, "projectile index #%d (%p) is unused", projectile_index, projectile));
	
	return projectile;
}
#endif

void drop_the_ball(
	world_point3d *origin,
	short polygon_index,
	short owner_index,
	short owner_type,
	short item_type)
{
	struct world_point3d vector;
	short projectile_index;

	vector.x= vector.y= vector.z= 0;
	projectile_index= new_projectile(origin, polygon_index, &vector, 0, _projectile_ball,
		owner_index, owner_type, NONE, FIXED_ONE);
	if (projectile_index!=NONE)
	{
		struct projectile_data *projectile= get_projectile_data(projectile_index);
		struct object_data *object= get_object_data(projectile->object_index);
		
		projectile->permutation= item_type;
		
		object->shape= get_item_shape(item_type);
	}

	return;
}

/* ---------- private code */

#ifdef DEBUG
struct projectile_definition *get_projectile_definition(
	short type)
{
	vassert(type>=0&&type<NUMBER_OF_PROJECTILE_TYPES, csprintf(temporary, "projectile type #%d is out of range", type));
	
	return projectile_definitions+type;
}
#endif

static short adjust_projectile_type(
	world_point3d *origin,
	short polygon_index,
	short type,
	short owner_index,
	short owner_type,
	short intended_target_index,
	fixed damage_scale)
{
	struct projectile_definition *definition= get_projectile_definition(type);
	short media_index= get_polygon_data(polygon_index)->media_index;
	
	#pragma unused (owner_index, owner_type, intended_target_index, damage_scale)
	
	if (media_index!=NONE)
	{
		if (get_media_data(media_index)->height>origin->z)
		{
			if (definition->media_projectile_promotion!=NONE) type= definition->media_projectile_promotion;
		}
	}
	
	return type;
}
	
#ifdef OBSOLETE
void guided_projectile_target(
	short projectile_index)
{
	struct projectile_data *projectile= get_projectile_data(projectile_index);
	struct object_data *projectile_object= get_object_data(projectile->object_index);
	world_point3d new_location;
	short new_polygon_index;
	short obstruction_index;
	
	// calculate a new_location, new_polygon_index
	new_location= object->location;
	translate_point3d(&new_location, speed, object->facing, projectile->elevation);
		
	// if we're pointing at anything, lock on
	if (translate_projectile(projectile->type, &object->location, object->polygon,
		&new_location, new_polygon_index, projectile->owner_index, &obstruction_index,
		(boolean *) NULL))
	{
		if (obstruction_index!=NONE)
		{
			projectile->target_index= obstruction_index;
		}
	}
	
	return;
}
#endif

#define MAXIMUM_GUIDED_DELTA_YAW 8
#define MAXIMUM_GUIDED_DELTA_PITCH 6

/* changes are at a rate of �1 angular unit per tick */
static void update_guided_projectile(
	short projectile_index)
{
	struct projectile_data *projectile= get_projectile_data(projectile_index);
	struct monster_data *target= get_monster_data(projectile->target_index);
	struct object_data *projectile_object= get_object_data(projectile->object_index);
	struct object_data *target_object= get_object_data(target->object_index);
	world_distance target_radius, target_height;
	world_point3d target_location;
	
	get_monster_dimensions(projectile->target_index, &target_radius, &target_height);
	target_location= target_object->location;
	target_location.z+= target_height>>1;
	
	switch (target_object->transfer_mode)
	{
		case _xfer_invisibility:
		case _xfer_subtle_invisibility:
			/* can�t hold lock on invisible targets unless on _total_carnage_level */
			if (dynamic_world->game_information.difficulty_level!=_total_carnage_level) break;
		default:
		{
			world_distance dx= target_location.x - projectile_object->location.x;
			world_distance dy= target_location.y - projectile_object->location.y;
			world_distance dz= target_location.z - projectile_object->location.z;
			short delta_yaw= MAXIMUM_GUIDED_DELTA_YAW+_normal_level-dynamic_world->game_information.difficulty_level;
			short delta_pitch= MAXIMUM_GUIDED_DELTA_PITCH+_normal_level-dynamic_world->game_information.difficulty_level;

			if (dx*sine_table[projectile_object->facing] - dy*cosine_table[projectile_object->facing] > 0)
			{
				// turn left
				delta_yaw= -delta_yaw;
			}
			
			dx= ABS(dx), dy= ABS(dy);
			if (GUESS_HYPOTENUSE(dx, dy)*sine_table[projectile->elevation] - dz*cosine_table[projectile->elevation] > 0)
			{
				// turn down
				delta_pitch= -delta_pitch;
			}

			projectile_object->facing= NORMALIZE_ANGLE(projectile_object->facing+delta_yaw);
			projectile->elevation= NORMALIZE_ANGLE(projectile->elevation+delta_pitch);

#if 0
			angle delta_pitch= HALF_CIRCLE - NORMALIZE_ANGLE(arctangent(guess_distance2d((world_point2d *)&target_location, (world_point2d *)&projectile_object->location), dz) - projectile->elevation);
			angle delta_yaw= HALF_CIRCLE - NORMALIZE_ANGLE(arctangent(dx, dy) - projectile_object->facing);
			short obstruction_index;
			word flags;

			switch (dynamic_world->game_information.difficulty_level)
			{
				case _wuss_level:
					if (dynamic_world->tick_count&7) return;
					break;
				case _easy_level:
					if (dynamic_world->tick_count&3) return;
					break;
			}

			switch (dynamic_world->game_information.difficulty_level)
			{
				case _major_damage_level:
				case _total_carnage_level:
					flags= translate_projectile(projectile->type, &projectile_object->location, projectile_object->polygon,
						&target_location, (short *) NULL, projectile->owner_index, &obstruction_index);
					if (!(flags&_projectile_hit_monster)) break; /* if we�re headed for a wall, don�t steer */
				default:
					projectile_object->facing= NORMALIZE_ANGLE(projectile_object->facing+PIN(delta_yaw, -maximum_delta_yaw, maximum_delta_yaw));
					projectile->elevation= NORMALIZE_ANGLE(projectile->elevation+PIN(delta_pitch, -maximum_delta_pitch, maximum_delta_pitch));
			}
#endif
		}
	}
	
	return;
}

/* new_polygon_index==NULL means we�re preflighting */
word translate_projectile(
	short type,
	world_point3d *old_location,
	short old_polygon_index,
	world_point3d *new_location,
	short *new_polygon_index,
	short owner_index,
	short *obstruction_index)
{
	struct projectile_definition *definition= get_projectile_definition(type);
	short intersected_object_indexes[GLOBAL_INTERSECTING_MONSTER_BUFFER_SIZE];
	struct polygon_data *old_polygon;
	world_point3d intersection;
	world_distance media_height;
	short line_index;
	short intersected_object_count;
	short contact;
	word flags= 0;

	*obstruction_index= NONE;

	contact= _hit_nothing;	
	intersected_object_count= 0;
	old_polygon= get_polygon_data(old_polygon_index);
	if (new_polygon_index) *new_polygon_index= old_polygon_index;
	do
	{
		media_height= (old_polygon->media_index==NONE || (definition->flags&_penetrates_media)) ? SHORT_MIN : get_media_data(old_polygon->media_index)->height;
				
		/* add this polygon�s monsters to our non-redundant list of possible intersections */
		possible_intersecting_monsters(intersected_object_indexes, &intersected_object_count, GLOBAL_INTERSECTING_MONSTER_BUFFER_SIZE, old_polygon_index, TRUE);
		
 		line_index= find_line_crossed_leaving_polygon(old_polygon_index, (world_point2d *)old_location, (world_point2d *)new_location);
		if (line_index!=NONE)
		{
			/* we crossed a line: if the line is solid, we detonate immediately on the wall,
				otherwise we calculate our Z at the line and compare it to the ceiling and
				floor heights of the old and new polygon to see if we hit a wall between the
				polygons, or the floor or ceiling somewhere in the old polygon */

			struct line_data *line= get_line_data(line_index);

			find_line_intersection(&get_endpoint_data(line->endpoint_indexes[0])->vertex,
				&get_endpoint_data(line->endpoint_indexes[1])->vertex, old_location, new_location,
				&intersection);
			if (!LINE_IS_SOLID(line) || LINE_HAS_TRANSPARENT_SIDE(line))
			{
				short adjacent_polygon_index= find_adjacent_polygon(old_polygon_index, line_index);
				struct polygon_data *adjacent_polygon= get_polygon_data(adjacent_polygon_index);
				
				if (intersection.z>media_height && intersection.z>old_polygon->floor_height)
				{
					if (intersection.z<old_polygon->ceiling_height)
					{
						if (intersection.z>adjacent_polygon->floor_height&&intersection.z<adjacent_polygon->ceiling_height)
						{
							if (!LINE_HAS_TRANSPARENT_SIDE(line) || (!new_polygon_index && (definition->flags&(_usually_pass_transparent_side|_sometimes_pass_transparent_side))) ||
								((definition->flags&_usually_pass_transparent_side) && (random()&3)) ||
								((definition->flags&_sometimes_pass_transparent_side) && !(random()&3)))
							{
								/* no intersections, successfully entered new polygon */
								if (new_polygon_index) *new_polygon_index= adjacent_polygon_index;
								old_polygon_index= adjacent_polygon_index;
								old_polygon= adjacent_polygon;
							}
							else
							{
								/* hit and could not pass transparent texture */
								contact= _hit_wall;
							}
						}
						else
						{
							/* hit wall created by ceiling or floor of new polygon; test to see
								if we toggle a control panel */
							if (new_polygon_index && (definition->flags&_can_toggle_control_panels)) try_and_toggle_control_panel(old_polygon_index, line_index);
							contact= _hit_wall;
						}
					}
					else
					{
						/* hit ceiling of old polygon */
						if (adjacent_polygon->ceiling_transfer_mode==_xfer_landscape) flags|= _projectile_hit_landscape;
						contact= _hit_ceiling;
					}
				}
				else
				{
					/* hit floor or media of old polygon */
					*obstruction_index= old_polygon_index;
					if (adjacent_polygon->floor_transfer_mode==_xfer_landscape) flags|= _projectile_hit_landscape;
					contact= (old_polygon->floor_height>media_height) ? _hit_floor : _hit_media;
				}
			}
			else
			{
				/* hit wall created by solid line; test to see if we toggle a control panel */
				if (new_polygon_index && (definition->flags&_can_toggle_control_panels)) try_and_toggle_control_panel(old_polygon_index, line_index);
				if (line_is_landscaped(old_polygon_index, line_index, intersection.z)) flags|= _projectile_hit_landscape;
				contact= _hit_wall;
			}
		}
		else
		{
			/* make sure we didn�t hit the ceiling or floor in this polygon */
			if (new_location->z>media_height && new_location->z>old_polygon->floor_height)
			{
				if (new_location->z<old_polygon->ceiling_height)
				{
					/* we�re staying in this polygon and we�re finally done screwing around;
						the caller can look in *new_polygon_index to find out where we ended up */
				}
				else
				{
					/* hit ceiling of current polygon */
					contact= _hit_ceiling;
					if (old_polygon->ceiling_transfer_mode==_xfer_landscape) flags|= _projectile_hit_landscape;
				}
			}
			else
			{
				/* hit floor of current polygon */
				*obstruction_index= old_polygon_index;
				contact= (old_polygon->floor_height>media_height) ? _hit_floor : _hit_media;
				if (old_polygon->floor_transfer_mode==_xfer_landscape) flags|= _projectile_hit_landscape;
			}
		}
	}
	while (line_index!=NONE&&contact==_hit_nothing);
	
	/* ceilings and floor intersections still don�t have accurate intersection points, so calculate
		them */
	if (contact!=_hit_nothing)
	{
		switch (contact)
		{
			case _hit_media:
				find_floor_or_ceiling_intersection(media_height, old_location, new_location, &intersection);
				break;
			case _hit_floor:
				find_floor_or_ceiling_intersection(old_polygon->floor_height, old_location, new_location, &intersection);
				break;
			case _hit_ceiling:
				find_floor_or_ceiling_intersection(old_polygon->ceiling_height, old_location, new_location, &intersection);
				break;
		}
		
		/* change new_location to the point of intersection with the ceiling, floor, or wall */
		*new_location= intersection;
	}

	/* check our object list and find the best intersection ... if we find an intersection at all,
		then we hit this before we hit the wall, because the object list is checked against the
		clipped new_location. */
	{
		world_distance best_intersection_distance= 0;
		world_distance distance_traveled;
		world_distance best_radius;
		short best_intersection_object= NONE;
		short i;
		
		distance_traveled= distance2d((world_point2d *)old_location, (world_point2d *)new_location);
		for (i=0;i<intersected_object_count;++i)
		{
			struct object_data *object= get_object_data(intersected_object_indexes[i]);
			long separation= point_to_line_segment_distance_squared((world_point2d *)&object->location,
				(world_point2d *)old_location, (world_point2d *)new_location);
			world_distance radius, height;
				
			if (object->permutation!=owner_index) /* don�t hit ourselves */
			{
				long radius_squared;
				
				switch (GET_OBJECT_OWNER(object))
				{
					case _object_is_monster: get_monster_dimensions(object->permutation, &radius, &height); break;
					case _object_is_scenery: get_scenery_dimensions(object->permutation, &radius, &height); break;
					default: halt();
				}
				radius_squared= (radius+definition->radius)*(radius+definition->radius);
				
				if (separation<radius_squared) /* if we�re within radius^2 we passed through this monster */
				{
					world_distance distance= distance2d((world_point2d *)old_location, (world_point2d *)&object->location);
					world_distance projectile_z= distance_traveled ? 
						old_location->z + (distance*(new_location->z-old_location->z))/distance_traveled :
						old_location->z;
					
					if ((height>0 && projectile_z>=object->location.z && projectile_z<=object->location.z+height) ||
						(height<0 && projectile_z>=object->location.z+height && projectile_z<=object->location.z))
					{
						if (best_intersection_object==NONE || distance<best_intersection_distance)
						{
							best_intersection_object= intersected_object_indexes[i];
							best_intersection_distance= distance;
							best_radius= radius;

							switch (GET_OBJECT_OWNER(object))
							{
								case _object_is_monster: contact= _hit_monster; break;
								case _object_is_scenery: contact= _hit_scenery; break;
								default: halt();
							}
						}
					}
				}
				else
				{
					if (GET_OBJECT_OWNER(object)==_object_is_monster && separation<12*radius_squared) /* if we�re within (x*radius)^2 we passed near this monster */
					{
						if (MONSTER_IS_PLAYER(get_monster_data(object->permutation)) && 
							monster_index_to_player_index(object->permutation)==current_player_index)
						{
							flags|= _flyby_of_current_player;
						}
					}
				}
			}
		}
		
		if (best_intersection_object!=NONE) /* if we hit something, take it */
		{
			struct object_data *object= get_object_data(best_intersection_object);
			
			*obstruction_index= best_intersection_object;
			
			if (distance_traveled)
			{
				world_distance actual_distance_to_hit;
				
				actual_distance_to_hit= distance2d((world_point2d *)old_location, (world_point2d *) &object->location);
				actual_distance_to_hit-= best_radius;
				
				new_location->x= old_location->x + (actual_distance_to_hit*(new_location->x-old_location->x))/distance_traveled;
				new_location->y= old_location->y + (actual_distance_to_hit*(new_location->y-old_location->y))/distance_traveled;
				new_location->z= old_location->z + (actual_distance_to_hit*(new_location->z-old_location->z))/distance_traveled;
				
				if (new_polygon_index) *new_polygon_index= find_new_object_polygon((world_point2d *) &object->location,
					(world_point2d *) new_location, object->polygon);
			}
			else
			{
				*new_location= *old_location;
			}
		}
	}

	switch (contact)
	{
		case _hit_monster: flags|= _projectile_hit|_projectile_hit_monster; break;
		case _hit_floor: flags|= _projectile_hit|_projectile_hit_floor; break;
		case _hit_media: flags|= _projectile_hit|_projectile_hit_media; break;
		case _hit_scenery: flags|= _projectile_hit|_projectile_hit_scenery; break;
		case _hit_nothing: break;
		default: flags|= _projectile_hit; break;
	}

	/* returns TRUE if we hit something, FALSE otherwise */
	return flags;
}
