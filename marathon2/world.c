/*
WORLD.C
Sunday, May 31, 1992 3:57:12 PM

Friday, January 15, 1993 9:59:14 AM
	added arctangent function.
Thursday, January 21, 1993 9:46:24 PM
	fixed arctangent function.  normalize_angle() could stand to be improved.
Saturday, January 23, 1993 9:46:34 PM
	fixed arctangent, hopefully for the last time.  normalize_angle() is a little faster.
Monday, January 25, 1993 3:01:47 PM
	arctangent works (tested at 0.5� increments against SANE�s tan), the only anomoly was
	apparently arctan(0)==180�.
Wednesday, January 27, 1993 3:49:04 PM
	final fix to arctangent, we swear.  recall lim(arctan(x)) as x approaches �/2 or 3�/4 is ��,
	depending on which side we come from.  because we didn't realize this, arctan failed in the
	case where x was very close to but slightly below �/2.  i think we�ve seen the last monster
	suddenly �panic� and bolt directly into a wall.
Sunday, July 25, 1993 11:51:42 PM
	the arctan of 0/0 is now (arbitrairly) �/2 because we�re sick of assert(y) failing.
Monday, June 20, 1994 4:15:06 PM
	bug fix in translate_point3d().
*/

#include "cseries.h"
#include "world.h"

#include <math.h>

//#ifdef mpwc
//#pragma segment render
//#endif

/* ---------- globals */

short *cosine_table;
short *sine_table;
static long *tangent_table;

static word random_seed= 0x1;
static word local_random_seed= 0x1;

/* ---------- code */

angle normalize_angle(
	angle theta)
{
	while (theta<0) theta+= NUMBER_OF_ANGLES;
	while (theta>=NUMBER_OF_ANGLES) theta-= NUMBER_OF_ANGLES;
	
	return theta;
}

/* remember this is not wholly accurate, both distance or the sine/cosine values could be
	negative, and the shift can�t make negative numbers zero; this is probably ok because
	we�ll have -1/1024th instead of zero, which is basically our margin for error anyway ... */
world_point2d *translate_point2d(
	world_point2d *point,
	world_distance distance,
	angle theta)
{
	assert(theta>=0&&theta<NUMBER_OF_ANGLES);
	assert(cosine_table[0]==TRIG_MAGNITUDE);
	
	point->x+= (distance*cosine_table[theta])>>TRIG_SHIFT;
	point->y+= (distance*sine_table[theta])>>TRIG_SHIFT;
	
	return point;
}

/* same comment as above */
world_point3d *translate_point3d(
	world_point3d *point,
	world_distance distance,
	angle theta,
	angle phi)
{
	world_distance transformed_distance;
	
	assert(theta>=0&&theta<NUMBER_OF_ANGLES);
	assert(phi>=0&&theta<NUMBER_OF_ANGLES);
	
	transformed_distance= (distance*cosine_table[phi])>>TRIG_SHIFT;
	point->x+= (transformed_distance*cosine_table[theta])>>TRIG_SHIFT;
	point->y+= (transformed_distance*sine_table[theta])>>TRIG_SHIFT;
	point->z+= (distance*sine_table[phi])>>TRIG_SHIFT;
	
	return point;
}

world_point2d *rotate_point2d(
	world_point2d *point,
	world_point2d *origin,
	angle theta)
{
	world_point2d temp;
	
	assert(theta>=0&&theta<NUMBER_OF_ANGLES);
	assert(cosine_table[0]==TRIG_MAGNITUDE);
	
	temp.x= point->x-origin->x;
	temp.y= point->y-origin->y;
	
	point->x= ((temp.x*cosine_table[theta])>>TRIG_SHIFT) + ((temp.y*sine_table[theta])>>TRIG_SHIFT) +
		origin->x;
	point->y= ((temp.y*cosine_table[theta])>>TRIG_SHIFT) - ((temp.x*sine_table[theta])>>TRIG_SHIFT) +
		origin->y;
	
	return point;
}

world_point2d *transform_point2d(
	world_point2d *point,
	world_point2d *origin,
	angle theta)
{
	world_point2d temp;
	
	assert(theta>=0&&theta<NUMBER_OF_ANGLES);
	assert(cosine_table[0]==TRIG_MAGNITUDE);
	
	temp.x= point->x-origin->x;
	temp.y= point->y-origin->y;
	
	point->x= ((temp.x*cosine_table[theta])>>TRIG_SHIFT) + ((temp.y*sine_table[theta])>>TRIG_SHIFT);
	point->y= ((temp.y*cosine_table[theta])>>TRIG_SHIFT) - ((temp.x*sine_table[theta])>>TRIG_SHIFT);
	
	return point;
}

world_point3d *transform_point3d(
	world_point3d *point,
	world_point3d *origin,
	angle theta,
	angle phi)
{
	world_point3d temporary;
	
	temporary.x= point->x-origin->x;
	temporary.y= point->y-origin->y;
	temporary.z= point->z-origin->z;
	
	/* do theta rotation (in x-y plane) */
	point->x= ((temporary.x*cosine_table[theta])>>TRIG_SHIFT) + ((temporary.y*sine_table[theta])>>TRIG_SHIFT);
	point->y= ((temporary.y*cosine_table[theta])>>TRIG_SHIFT) - ((temporary.x*sine_table[theta])>>TRIG_SHIFT);
	
	/* do phi rotation (in x-z plane) */
	if (phi)
	{
		temporary.x= point->x;
		/* temporary.z is already set */
		
		point->x= ((temporary.x*cosine_table[phi])>>TRIG_SHIFT) + ((temporary.z*sine_table[phi])>>TRIG_SHIFT);
		point->z= ((temporary.z*cosine_table[phi])>>TRIG_SHIFT) - ((temporary.x*sine_table[phi])>>TRIG_SHIFT);
		/* y-coordinate is unchanged */
	}
	else
	{
		point->z= temporary.z;
	}
	
	return point;
}

void build_trig_tables(
	void)
{
	short i;
	double two_pi= 8.0*atan(1.0);
	double theta;

	sine_table= (short *) malloc(sizeof(short)*NUMBER_OF_ANGLES);
	cosine_table= (short *) malloc(sizeof(short)*NUMBER_OF_ANGLES);
	tangent_table= (long *) malloc(sizeof(long)*NUMBER_OF_ANGLES);
	assert(sine_table&&cosine_table&&tangent_table);
	
	for (i=0;i<NUMBER_OF_ANGLES;++i)
	{
		theta= two_pi*(double)i/(double)NUMBER_OF_ANGLES;
		
		cosine_table[i]= (short) ((double)TRIG_MAGNITUDE*cos(theta)+0.5);
		sine_table[i]= (short) ((double)TRIG_MAGNITUDE*sin(theta)+0.5);
		
		if (i==0) sine_table[i]= 0, cosine_table[i]= TRIG_MAGNITUDE;
		if (i==QUARTER_CIRCLE) sine_table[i]= TRIG_MAGNITUDE, cosine_table[i]= 0;
		if (i==HALF_CIRCLE) sine_table[i]= 0, cosine_table[i]= -TRIG_MAGNITUDE;
		if (i==THREE_QUARTER_CIRCLE) sine_table[i]= -TRIG_MAGNITUDE, cosine_table[i]= 0;
		
		/* what we care about here is NOT accuracy, rather we�re concerned with matching the
			ratio of the existing sine and cosine tables as exactly as possible */
		if (cosine_table[i])
		{
			tangent_table[i]= ((TRIG_MAGNITUDE*sine_table[i])/cosine_table[i]);
		}
		else
		{
			/* we always take -�, even though the limit is ��, depending on which side you
				approach it from.  this is because of the direction we traverse the circle
				looking for matches during arctan. */
			tangent_table[i]= LONG_MIN;
		}
	}

	return;
}

/* one day we�ll come back here and actually make this run fast */
angle arctangent(
	world_distance x,
	world_distance y)
{
	long tangent;
	register long last_difference, new_difference;
	angle search_arc, theta;
	
	if (x)
	{
		tangent= (TRIG_MAGNITUDE*y)/x;
		
		if (tangent)
		{
			theta= (y>0) ? 1 : HALF_CIRCLE+1;
			if (tangent<0) theta+= QUARTER_CIRCLE;
			
			last_difference= tangent-tangent_table[theta-1];
			for (search_arc=QUARTER_CIRCLE-1;search_arc;search_arc--,theta++)
			{
				new_difference= tangent-tangent_table[theta];
				
				if ((last_difference<=0&&new_difference>=0) || (last_difference>=0&&new_difference<=0))
				{
					if (ABS(last_difference)<ABS(new_difference))
					{
						return theta-1;
					}
					else
					{
						return theta;
					}
				}
				
				last_difference= new_difference;
			}
			
			return theta==NUMBER_OF_ANGLES ? 0 : theta;
		}
		else
		{
			return x<0 ? HALF_CIRCLE : 0;
		}
	}
	else
	{
		/* so arctan(0,0)==�/2 (bill me) */
		return y<0 ? THREE_QUARTER_CIRCLE : QUARTER_CIRCLE;
	}
}

void set_random_seed(
	word seed)
{
	random_seed= seed ? seed : DEFAULT_RANDOM_SEED;
	
	return;
}

word get_random_seed(
	void)
{
	return random_seed;
}

word random(
	void)
{
	word seed= random_seed;
	
	if (seed&1)
	{
		seed= (seed>>1)^0xb400;
	}
	else
	{
		seed>>= 1;
	}

	return (random_seed= seed);
}

word local_random(
	void)
{
	word seed= local_random_seed;
	
	if (seed&1)
	{
		seed= (seed>>1)^0xb400;
	}
	else
	{
		seed>>= 1;
	}

	return (local_random_seed= seed);
}

world_distance guess_distance2d(
	world_point2d *p0,
	world_point2d *p1)
{
	long dx= (long)p0->x - p1->x;
	long dy= (long)p0->y - p1->y;
	long distance;
	
	if (dx<0) dx= -dx;
	if (dy<0) dy= -dy;
	distance= GUESS_HYPOTENUSE(dx, dy);
	
	return distance>SHORT_MAX ? SHORT_MAX : distance;
}

world_distance distance3d(
	world_point3d *p0,
	world_point3d *p1)
{
	long dx= (long)p0->x - p1->x;
	long dy= (long)p0->y - p1->y;
	long dz= (long)p0->z - p1->z;
	long distance= isqrt(dx*dx + dy*dy + dz*dz);
	
	return distance>SHORT_MAX ? SHORT_MAX : distance;
}

world_distance distance2d(
	world_point2d *p0,
	world_point2d *p1)
{
	return isqrt((p0->x-p1->x)*(p0->x-p1->x)+(p0->y-p1->y)*(p0->y-p1->y));
}

/*
 * It requires more space to describe this implementation of the manual
 * square root algorithm than it did to code it.  The basic idea is that
 * the square root is computed one bit at a time from the high end.  Because
 * the original number is 32 bits (unsigned), the root cannot exceed 16 bits
 * in length, so we start with the 0x8000 bit.
 *
 * Let "x" be the value whose root we desire, "t" be the square root
 * that we desire, and "s" be a bitmask.  A simple way to compute
 * the root is to set "s" to 0x8000, and loop doing the following:
 *
 *      t = 0;
 *      s = 0x8000;
 *      do {
 *              if ((t + s) * (t + s) <= x)
 *                      t += s;
 *              s >>= 1;
 *      while (s != 0);
 *
 * The primary disadvantage to this approach is the multiplication.  To
 * eliminate this, we begin simplying.  First, we observe that
 *
 *      (t + s) * (t + s) == (t * t) + (2 * t * s) + (s * s)
 *
 * Therefore, if we redefine "x" to be the original argument minus the
 * current value of (t * t), we can determine if we should add "s" to
 * the root if
 *
 *      (2 * t * s) + (s * s) <= x
 *
 * If we define a new temporary "nr", we can express this as
 *
 *      t = 0;
 *      s = 0x8000;
 *      do {
 *              nr = (2 * t * s) + (s * s);
 *              if (nr <= x) {
 *                      x -= nr;
 *                      t += s;
 *              }
 *              s >>= 1;
 *      while (s != 0);
 *
 * We can improve the performance of this by noting that "s" is always a
 * power of two, so multiplication by "s" is just a shift.  Also, because
 * "s" changes in a predictable manner (shifted right after each iteration)
 * we can precompute (0x8000 * t) and (0x8000 * 0x8000) and then adjust
 * them by shifting after each step.  First, we let "m" hold the value
 * (s * s) and adjust it after each step by shifting right twice.  We
 * also introduce "r" to hold (2 * t * s) and adjust it after each step
 * by shifting right once.  When we update "t" we must also update "r",
 * and we do so by noting that (2 * (old_t + s) * s) is the same as
 * (2 * old_t * s) + (2 * s * s).  Noting that (s * s) is "m" and that
 * (r + 2 * m) == ((r + m) + m) == (nr + m):
 *
 *      t = 0;
 *      s = 0x8000;
 *      m = 0x40000000;
 *      r = 0;
 *      do {
 *              nr = r + m;
 *              if (nr <= x) {
 *                      x -= nr;
 *                      t += s;
 *                      r = nr + m;
 *              }
 *              s >>= 1;
 *              r >>= 1;
 *              m >>= 2;
 *      } while (s != 0);
 *
 * Finally, we note that, if we were using fractional arithmetic, after
 * 16 iterations "s" would be a binary 0.5, so the value of "r" when
 * the loop terminates is (2 * t * 0.5) or "t".  Because the values in
 * "t" and "r" are identical after the loop terminates, and because we
 * do not otherwise use "t"  explicitly within the loop, we can omit it.
 * When we do so, there is no need for "s" except to terminate the loop,
 * but we observe that "m" will become zero at the same time as "s",
 * so we can use it instead.
 *
 * The result we have at this point is the floor of the square root.  If
 * we want to round to the nearest integer, we need to consider whether
 * the remainder in "x" is greater than or equal to the difference
 * between ((r + 0.5) * (r + 0.5)) and (r * r).  Noting that the former
 * quantity is (r * r + r + 0.25), we want to check if the remainder is
 * greater than or equal to (r + 0.25).  Because we are dealing with
 * integers, we can't have equality, so we round up if "x" is strictly
 * greater than "r":
 *
 *      if (x > r)
 *              r++;
 */

long isqrt(
	register unsigned long x)
{
	register unsigned long r, nr, m;

	r= 0;
	m= 0x40000000;
	
	do
	{
		nr= r + m;
		if (nr<=x)
		{
			x-= nr;
			r= nr + m;
		}
		r>>= 1;
		m>>= 2;
	}
	while (m!=0);

	if (x>r) r+= 1;
	return r;
}

#ifdef OBSOLETE
world_distance guess_distance3d(
	world_point3d *p0,
	world_point3d *p1)
{
	world_distance dx= (p0->x<p1->x) ? (p1->x-p0->x) : (p0->x-p1->x);
	world_distance dy= (p0->y<p1->y) ? (p1->y-p0->y) : (p0->y-p1->y);
	world_distance dz= (p0->z<p1->z) ? (p1->z-p0->z) : (p0->z-p1->z);

	/* max + med/4 + min/4 formula from graphics gems; we�re just taking their word for it */
	return (dx>dy) ? ((dx>dz) ? (dx+(dy>>2)+(dz>>2)) : (dz+(dx>>2)+(dy>>2))) :
		((dy>dz) ? (dy+(dx>>2)+(dz>>2)) : (dz+(dx>>2)+(dy>>2)));
}
#endif
