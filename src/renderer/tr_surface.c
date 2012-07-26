/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2009 Darklegion Development

This file is part of Tremulous.

Tremulous is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Tremulous is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremulous; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// tr_surf.c
#include "tr_local.h"
#if idppc_altivec && !defined(MACOS_X)
#include <altivec.h>
#endif

/*

  THIS ENTIRE FILE IS BACK END

backEnd.currentEntity will be valid.

Tess_Begin has already been called for the surface's shader.

The modelview matrix will be set.

It is safe to actually issue drawing commands here if you don't want to
use the shader system.
*/


//============================================================================


/*
==============
RB_AddQuadStampExt
==============
*/
void RB_AddQuadStampExt( vec3_t origin, vec3_t left, vec3_t up, byte *color, float s1, float t1, float s2, float t2 ) {
	int			ndx;

	if ( tess.vertexPtr1 ) {
		ndx = tess.numVertexes;
		
		tess.vertexPtr2[ndx+0].xyz[0] = origin[0] + left[0] + up[0];
		tess.vertexPtr2[ndx+0].xyz[1] = origin[1] + left[1] + up[1];
		tess.vertexPtr2[ndx+0].xyz[2] = origin[2] + left[2] + up[2];
		tess.vertexPtr2[ndx+0].fogNum = (float)tess.fogNum;
		
		tess.vertexPtr2[ndx+1].xyz[0] = origin[0] - left[0] + up[0];
		tess.vertexPtr2[ndx+1].xyz[1] = origin[1] - left[1] + up[1];
		tess.vertexPtr2[ndx+1].xyz[2] = origin[2] - left[2] + up[2];
		tess.vertexPtr2[ndx+1].fogNum = (float)tess.fogNum;
		
		tess.vertexPtr2[ndx+2].xyz[0] = origin[0] - left[0] - up[0];
		tess.vertexPtr2[ndx+2].xyz[1] = origin[1] - left[1] - up[1];
		tess.vertexPtr2[ndx+2].xyz[2] = origin[2] - left[2] - up[2];
		tess.vertexPtr2[ndx+2].fogNum = (float)tess.fogNum;
		
		tess.vertexPtr2[ndx+3].xyz[0] = origin[0] + left[0] - up[0];
		tess.vertexPtr2[ndx+3].xyz[1] = origin[1] + left[1] - up[1];
		tess.vertexPtr2[ndx+3].xyz[2] = origin[2] + left[2] - up[2];
		tess.vertexPtr2[ndx+3].fogNum = (float)tess.fogNum;
		
		// sprites don't use normal, so I reuse it to store the
		// shadertimes
		tess.vertexPtr3[ndx+0].normal[0] = tess.shaderTime;
		tess.vertexPtr3[ndx+0].normal[1] = 0.0f;
		tess.vertexPtr3[ndx+0].normal[2] = 0.0f;
		tess.vertexPtr3[ndx+1].normal[0] = tess.shaderTime;
		tess.vertexPtr3[ndx+1].normal[1] = 0.0f;
		tess.vertexPtr3[ndx+1].normal[2] = 0.0f;
		tess.vertexPtr3[ndx+2].normal[0] = tess.shaderTime;
		tess.vertexPtr3[ndx+2].normal[1] = 0.0f;
		tess.vertexPtr3[ndx+2].normal[2] = 0.0f;
		tess.vertexPtr3[ndx+3].normal[0] = tess.shaderTime;
		tess.vertexPtr3[ndx+3].normal[1] = 0.0f;
		tess.vertexPtr3[ndx+3].normal[2] = 0.0f;
		
		// standard square texture coordinates
		tess.vertexPtr1[ndx].tc1[0] = tess.vertexPtr1[ndx].tc2[0] = s1;
		tess.vertexPtr1[ndx].tc1[1] = tess.vertexPtr1[ndx].tc2[1] = t1;
		
		tess.vertexPtr1[ndx+1].tc1[0] = tess.vertexPtr1[ndx+1].tc2[0] = s2;
		tess.vertexPtr1[ndx+1].tc1[1] = tess.vertexPtr1[ndx+1].tc2[1] = t1;
		
		tess.vertexPtr1[ndx+2].tc1[0] = tess.vertexPtr1[ndx+2].tc2[0] = s2;
		tess.vertexPtr1[ndx+2].tc1[1] = tess.vertexPtr1[ndx+2].tc2[1] = t2;
		
		tess.vertexPtr1[ndx+3].tc1[0] = tess.vertexPtr1[ndx+3].tc2[0] = s1;
		tess.vertexPtr1[ndx+3].tc1[1] = tess.vertexPtr1[ndx+3].tc2[1] = t2;
		
		// constant color all the way around
		// should this be identity and let the shader specify from entity?
		tess.vertexPtr3[ndx].color[0] = color[0];
		tess.vertexPtr3[ndx].color[1] = color[1];
		tess.vertexPtr3[ndx].color[2] = color[2];
		tess.vertexPtr3[ndx].color[3] = color[3];
		tess.vertexPtr3[ndx+1].color[0] = color[0];
		tess.vertexPtr3[ndx+1].color[1] = color[1];
		tess.vertexPtr3[ndx+1].color[2] = color[2];
		tess.vertexPtr3[ndx+1].color[3] = color[3];
		tess.vertexPtr3[ndx+2].color[0] = color[0];
		tess.vertexPtr3[ndx+2].color[1] = color[1];
		tess.vertexPtr3[ndx+2].color[2] = color[2];
		tess.vertexPtr3[ndx+2].color[3] = color[3];
		tess.vertexPtr3[ndx+3].color[0] = color[0];
		tess.vertexPtr3[ndx+3].color[1] = color[1];
		tess.vertexPtr3[ndx+3].color[2] = color[2];
		tess.vertexPtr3[ndx+3].color[3] = color[3];
	} else {
		ndx = 0; // should never be in a VBO
	}

	if ( tess.indexPtr.p16 ) {
		if ( tess.minIndex > ndx )
			tess.minIndex = ndx;
		if ( tess.maxIndex < ndx + 5 )
			tess.maxIndex = ndx + 5;
		
		// triangle indexes for a simple quad
		if ( tess.indexInc == sizeof(GLuint) ) {
			tess.indexPtr.p32[ tess.numIndexes ] = ndx;
			tess.indexPtr.p32[ tess.numIndexes + 1 ] = ndx + 1;
			tess.indexPtr.p32[ tess.numIndexes + 2 ] = ndx + 3;
			
			tess.indexPtr.p32[ tess.numIndexes + 3 ] = ndx + 3;
			tess.indexPtr.p32[ tess.numIndexes + 4 ] = ndx + 1;
			tess.indexPtr.p32[ tess.numIndexes + 5 ] = ndx + 2;
		} else {
			tess.indexPtr.p16[ tess.numIndexes ] = ndx;
			tess.indexPtr.p16[ tess.numIndexes + 1 ] = ndx + 1;
			tess.indexPtr.p16[ tess.numIndexes + 2 ] = ndx + 3;
			
			tess.indexPtr.p16[ tess.numIndexes + 3 ] = ndx + 3;
			tess.indexPtr.p16[ tess.numIndexes + 4 ] = ndx + 1;
			tess.indexPtr.p16[ tess.numIndexes + 5 ] = ndx + 2;
		}
	}
	
	tess.numVertexes += 4;
	tess.numIndexes += 6;
}

/*
==============
RB_AddQuadStamp
==============
*/
void RB_AddQuadStamp( vec3_t origin, vec3_t left, vec3_t up, byte *color ) {
	RB_AddQuadStampExt( origin, left, up, color, 0, 0, 1, 1 );
}

/*
==============
RB_SurfaceSprite
==============
*/
static void RB_SurfaceSprite( void ) {
	vec3_t		left, up;
	float		radius;

	// calculate the xyz locations for the four corners
	radius = backEnd.currentEntity->e.radius;
	if ( backEnd.currentEntity->e.rotation == 0 ) {
		VectorScale( backEnd.viewParms.or.axis[1], radius, left );
		VectorScale( backEnd.viewParms.or.axis[2], radius, up );
	} else {
		float	s, c;
		float	ang;
		
		ang = M_PI * backEnd.currentEntity->e.rotation / 180;
		s = sin( ang );
		c = cos( ang );

		VectorScale( backEnd.viewParms.or.axis[1], c * radius, left );
		VectorMA( left, -s * radius, backEnd.viewParms.or.axis[2], left );

		VectorScale( backEnd.viewParms.or.axis[2], c * radius, up );
		VectorMA( up, s * radius, backEnd.viewParms.or.axis[1], up );
	}
	if ( backEnd.viewParms.isMirror ) {
		VectorSubtract( vec3_origin, left, left );
	}

	RB_AddQuadStamp( backEnd.currentEntity->e.origin, left, up, backEnd.currentEntity->e.shaderRGBA );
}


/*
=============
RB_SurfacePolychain
=============
*/
static void RB_SurfacePolychain( surfaceType_t *surface ) {
	srfPoly_t *p = (srfPoly_t *)surface;
	int		i;
	int		numv;
	vaWord1_t	*vertexPtr1;
	vaWord2_t	*vertexPtr2;
	vaWord3_t	*vertexPtr3;

	if ( tess.vertexPtr1 ) {
		vertexPtr1 = tess.vertexPtr1 + tess.numVertexes;
		vertexPtr2 = tess.vertexPtr2 + tess.numVertexes;
		vertexPtr3 = tess.vertexPtr3 + tess.numVertexes;
		
		// fan triangles into the tess array
		numv = tess.numVertexes;
		for ( i = 0; i < p->numVerts; i++ ) {
			VectorCopy ( p->verts[i].xyz, *(vec3_t *)vertexPtr2->xyz );
			Vector2Copy( p->verts[i].st,  vertexPtr1->tc1 );
			*(int *)(&vertexPtr3->color) = *(int *)p->verts[ i ].modulate;
			vertexPtr2->fogNum = tess.fogNum;
			
			vertexPtr1++;
			vertexPtr2++;
			vertexPtr3++;
		}
	} else {
		numv = 0; //ERROR, should never have coordinates in VBO
	}

	if ( tess.indexPtr.p16 ) {
		if ( tess.minIndex > numv )
			tess.minIndex = numv;
		if ( tess.maxIndex < numv + p->numVerts - 1 )
			tess.maxIndex = numv + p->numVerts - 1;
		
		// generate fan indexes into the tess array
		if ( tess.indexInc == sizeof(GLushort) ) {
			GLushort	*indexPtr = tess.indexPtr.p16 + tess.numIndexes;
			for ( i = 0; i < p->numVerts-2; i++ ) {
				indexPtr[0] = (GLushort)numv;
				indexPtr[1] = (GLushort)(numv + i + 1);
				indexPtr[2] = (GLushort)(numv + i + 2);
				indexPtr += 3;
			}
		} else {
			GLuint		*indexPtr32 =tess.indexPtr.p32 + tess.numIndexes;
			for ( i = 0; i < p->numVerts-2; i++ ) {
				indexPtr32[0] = numv;
				indexPtr32[1] = numv + i + 1;
				indexPtr32[2] = numv+ i + 2;
				indexPtr32 += 3;
			}
		}
	}
	
	tess.numVertexes += p->numVerts;
	tess.numIndexes  += 3*(p->numVerts - 2);
}


/*
=============
RB_SurfaceTriangles
=============
*/
static void RB_SurfaceTriangles( surfaceType_t *surface ) {
	srfTriangles_t *srf = (srfTriangles_t *)surface;
	int			i;
	drawVert_t	*dv;
	int			dlightBits;
	GLushort	*indexPtr;
	GLuint		*indexPtr32;
	vaWord1_t	*vertexPtr1;
	vaWord2_t	*vertexPtr2;
	vaWord3_t	*vertexPtr3;
	int             numv;

	dlightBits = srf->dlightBits[backEnd.smpFrame];
	tess.dlightBits |= dlightBits;

	if ( tess.vertexPtr1 ) {
		numv = tess.numVertexes;
		
		vertexPtr1 = tess.vertexPtr1 + numv;
		vertexPtr2 = tess.vertexPtr2 + numv;
		vertexPtr3 = tess.vertexPtr3 + numv;
		dv = srf->verts;
		
		for ( i = 0 ; i < srf->numVerts ; i++, dv++ ) {
			VectorCopy( dv->xyz, *(vec3_t *)vertexPtr2->xyz );
			VectorCopy( dv->normal, vertexPtr3->normal );
			Vector2Copy( dv->st,       vertexPtr1->tc1 );
			Vector2Copy( dv->lightmap, vertexPtr1->tc2 );
			*(int *)&vertexPtr3->color = *(int *)dv->color;
			vertexPtr2->fogNum = tess.fogNum;
			vertexPtr1++;
			vertexPtr2++;
			vertexPtr3++;
		}
	} else {
		numv = srf->vboStart;
	}

	if ( tess.indexPtr.p16 ) {
		if ( tess.minIndex > numv )
			tess.minIndex = numv;
		if ( tess.maxIndex < numv + srf->numVerts - 1 )
			tess.maxIndex = numv + srf->numVerts - 1;
		
		if ( tess.indexInc == sizeof(GLushort) ) {
			indexPtr = tess.indexPtr.p16 + tess.numIndexes;
			
			for ( i = 0 ; i < srf->numIndexes ; i += 3 ) {
				indexPtr[ i + 0 ] = (GLushort)(numv + srf->indexes[ i + 0 ]);
				indexPtr[ i + 1 ] = (GLushort)(numv + srf->indexes[ i + 1 ]);
				indexPtr[ i + 2 ] = (GLushort)(numv + srf->indexes[ i + 2 ]);
			}
		} else {
			indexPtr32 = tess.indexPtr.p32 + tess.numIndexes;
			for ( i = 0 ; i < srf->numIndexes ; i += 3 ) {
				indexPtr32[ i + 0 ] = numv + srf->indexes[ i + 0 ];
				indexPtr32[ i + 1 ] = numv + srf->indexes[ i + 1 ];
				indexPtr32[ i + 2 ] = numv + srf->indexes[ i + 2 ];
			}
		}
	}

	tess.numIndexes += srf->numIndexes;
	tess.numVertexes += srf->numVerts;
}



/*
==============
RB_SurfaceBeam
==============
*/
static void RB_SurfaceBeam( void ) 
{
#define NUM_BEAM_SEGS 6
	refEntity_t *e;
	int	i;
	vec3_t perpvec;
	vec3_t direction, normalized_direction;
	vec3_t	points[2 * (NUM_BEAM_SEGS+1)];
	vec3_t	*start_point, *end_point;
	vec3_t oldorigin, origin;

	e = &backEnd.currentEntity->e;

	oldorigin[0] = e->oldorigin[0];
	oldorigin[1] = e->oldorigin[1];
	oldorigin[2] = e->oldorigin[2];

	origin[0] = e->origin[0];
	origin[1] = e->origin[1];
	origin[2] = e->origin[2];

	normalized_direction[0] = direction[0] = oldorigin[0] - origin[0];
	normalized_direction[1] = direction[1] = oldorigin[1] - origin[1];
	normalized_direction[2] = direction[2] = oldorigin[2] - origin[2];

	if ( VectorNormalize( normalized_direction ) == 0 )
		return;

	PerpendicularVector( perpvec, normalized_direction );

	VectorScale( perpvec, 4, perpvec );

	start_point = &points[0];
	end_point = &points[1];
	for ( i = 0; i < NUM_BEAM_SEGS ; i++ )
	{
		RotatePointAroundVector( *start_point, normalized_direction, perpvec, (360.0/NUM_BEAM_SEGS)*i );
//		VectorAdd( *start_point, origin, *start_point );
		VectorAdd( *start_point, direction, *end_point );

		start_point += 2; end_point += 2;
	}
	VectorCopy( points[0], *start_point );
	VectorCopy( points[1], *end_point );

	GL_Program( 0 );
	GL_VBO( 0, 0 );
	GL_BindImages( 1, &tr.whiteImage );

	GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE );

	GL_Color4f( 1.0f, 0.0f, 0.0f, 1.0f );

	qglVertexPointer( 3, GL_FLOAT, 0, points );
	qglDrawArrays( GL_TRIANGLE_STRIP, 0, 2*(NUM_BEAM_SEGS+1) );
}

//================================================================================

static void DoRailCore( const vec3_t start, const vec3_t end, const vec3_t up, float len, float spanWidth )
{
	float		spanWidth2;
	int			vbase;
	float		t = len / 256.0f;

	spanWidth2 = -spanWidth;

	if ( tess.vertexPtr1 ) {
		vbase = tess.numVertexes;
		
		// FIXME: use quad stamp?
		VectorMA( start, spanWidth, up, tess.vertexPtr2[vbase].xyz );
		tess.vertexPtr1[vbase].tc1[0] = 0;
		tess.vertexPtr1[vbase].tc1[1] = 0;
		tess.vertexPtr3[vbase].color[0] = backEnd.currentEntity->e.shaderRGBA[0] * 0.25;
		tess.vertexPtr3[vbase].color[1] = backEnd.currentEntity->e.shaderRGBA[1] * 0.25;
		tess.vertexPtr3[vbase].color[2] = backEnd.currentEntity->e.shaderRGBA[2] * 0.25;

		VectorMA( start, spanWidth2, up, tess.vertexPtr2[vbase+1].xyz );
		tess.vertexPtr1[vbase+1].tc1[0] = 0;
		tess.vertexPtr1[vbase+1].tc1[1] = 1;
		tess.vertexPtr3[vbase+1].color[0] = backEnd.currentEntity->e.shaderRGBA[0];
		tess.vertexPtr3[vbase+1].color[1] = backEnd.currentEntity->e.shaderRGBA[1];
		tess.vertexPtr3[vbase+1].color[2] = backEnd.currentEntity->e.shaderRGBA[2];
		
		VectorMA( start, spanWidth, up, tess.vertexPtr2[vbase+2].xyz );
		tess.vertexPtr1[vbase+2].tc1[0] = t;
		tess.vertexPtr1[vbase+2].tc1[1] = 0;
		tess.vertexPtr3[vbase+2].color[0] = backEnd.currentEntity->e.shaderRGBA[0];
		tess.vertexPtr3[vbase+2].color[1] = backEnd.currentEntity->e.shaderRGBA[1];
		tess.vertexPtr3[vbase+2].color[2] = backEnd.currentEntity->e.shaderRGBA[2];
		
		VectorMA( start, spanWidth, up, tess.vertexPtr2[vbase+3].xyz );
		tess.vertexPtr1[vbase+3].tc1[0] = t;
		tess.vertexPtr1[vbase+3].tc1[1] = 1;
		tess.vertexPtr3[vbase+3].color[0] = backEnd.currentEntity->e.shaderRGBA[0];
		tess.vertexPtr3[vbase+3].color[1] = backEnd.currentEntity->e.shaderRGBA[1];
		tess.vertexPtr3[vbase+3].color[2] = backEnd.currentEntity->e.shaderRGBA[2];
	} else {
		vbase = 0;
	}
	
	if ( tess.indexPtr.p16 ) {
		tess.indexPtr.p16[tess.numIndexes] = vbase;
		tess.indexPtr.p16[tess.numIndexes+1] = vbase + 1;
		tess.indexPtr.p16[tess.numIndexes+2] = vbase + 2;
		
		tess.indexPtr.p16[tess.numIndexes+3] = vbase + 2;
		tess.indexPtr.p16[tess.numIndexes+4] = vbase + 1;
		tess.indexPtr.p16[tess.numIndexes+5] = vbase + 3;
	}

	tess.numVertexes += 4;
	tess.numIndexes += 6;
}

static void DoRailDiscs( int numSegs, const vec3_t start, const vec3_t dir, const vec3_t right, const vec3_t up )
{
	int i;
	vec3_t	pos[4];
	vec3_t	v;
	int		spanWidth = r_railWidth->integer;
	int		vbase;
	float c, s;
	float		scale = 0.25;

	if ( numSegs > 1 )
		numSegs--;
	if ( !numSegs )
		return;

	for ( i = 0; i < 4; i++ )
	{
		c = cos( DEG2RAD( 45 + i * 90 ) );
		s = sin( DEG2RAD( 45 + i * 90 ) );
		v[0] = ( right[0] * c + up[0] * s ) * scale * spanWidth;
		v[1] = ( right[1] * c + up[1] * s ) * scale * spanWidth;
		v[2] = ( right[2] * c + up[2] * s ) * scale * spanWidth;
		VectorAdd( start, v, pos[i] );

		if ( numSegs > 1 )
		{
			// offset by 1 segment if we're doing a long distance shot
			VectorAdd( pos[i], dir, pos[i] );
		}
	}

	if ( tess.vertexPtr1 ) {
		vbase = tess.numVertexes;
		for ( i = 0; i < numSegs; i++ )
		{
			int j;
			
			for ( j = 0; j < 4; j++ )
			{
				VectorCopy( pos[j], tess.vertexPtr2[vbase].xyz );
				tess.vertexPtr1[vbase].tc1[0] = ( j < 2 );
				tess.vertexPtr1[vbase].tc1[1] = ( j && j != 3 );
				tess.vertexPtr3[vbase].color[0] = backEnd.currentEntity->e.shaderRGBA[0];
				tess.vertexPtr3[vbase].color[1] = backEnd.currentEntity->e.shaderRGBA[1];
				tess.vertexPtr3[vbase].color[2] = backEnd.currentEntity->e.shaderRGBA[2];
				vbase++;
				
				VectorAdd( pos[j], dir, pos[j] );
			}
		}
		vbase = tess.numVertexes;
	} else {
		vbase = 0;
	}

	if ( tess.indexPtr.p16 ) {
		for ( i = 0; i < numSegs; i++ )
		{
			int iwrite = tess.numIndexes;
			tess.indexPtr.p16[iwrite++] = vbase + 0;
			tess.indexPtr.p16[iwrite++] = vbase + 1;
			tess.indexPtr.p16[iwrite++] = vbase + 3;
			tess.indexPtr.p16[iwrite++] = vbase + 3;
			tess.indexPtr.p16[iwrite++] = vbase + 1;
			tess.indexPtr.p16[iwrite++] = vbase + 2;
			vbase += 4;
		}
	}

	tess.numVertexes += numSegs * 4;
	tess.numIndexes += numSegs * 6;
}

/*
** RB_SurfaceRailRinges
*/
static void RB_SurfaceRailRings( void ) {
	refEntity_t *e;
	int			numSegs;
	int			len;
	vec3_t		vec;
	vec3_t		right, up;
	vec3_t		start, end;

	e = &backEnd.currentEntity->e;

	VectorCopy( e->oldorigin, start );
	VectorCopy( e->origin, end );

	// compute variables
	VectorSubtract( end, start, vec );
	len = VectorNormalize( vec );
	MakeNormalVectors( vec, right, up );
	numSegs = ( len ) / r_railSegmentLength->value;
	if ( numSegs <= 0 ) {
		numSegs = 1;
	}

	VectorScale( vec, r_railSegmentLength->value, vec );

	DoRailDiscs( numSegs, start, vec, right, up );
}

/*
** RB_SurfaceRailCore
*/
static void RB_SurfaceRailCore( void ) {
	refEntity_t *e;
	int			len;
	vec3_t		right;
	vec3_t		vec;
	vec3_t		start, end;
	vec3_t		v1, v2;

	e = &backEnd.currentEntity->e;

	VectorCopy( e->oldorigin, start );
	VectorCopy( e->origin, end );

	VectorSubtract( end, start, vec );
	len = VectorNormalize( vec );

	// compute side vector
	VectorSubtract( start, backEnd.viewParms.or.origin, v1 );
	VectorNormalize( v1 );
	VectorSubtract( end, backEnd.viewParms.or.origin, v2 );
	VectorNormalize( v2 );
	CrossProduct( v1, v2, right );
	VectorNormalize( right );

	DoRailCore( start, end, right, len, r_railCoreWidth->integer );
}

/*
** RB_SurfaceLightningBolt
*/
static void RB_SurfaceLightningBolt( void ) {
	refEntity_t *e;
	int			len;
	vec3_t		right;
	vec3_t		vec;
	vec3_t		start, end;
	vec3_t		v1, v2;
	int			i;

	e = &backEnd.currentEntity->e;

	VectorCopy( e->oldorigin, end );
	VectorCopy( e->origin, start );

	// compute variables
	VectorSubtract( end, start, vec );
	len = VectorNormalize( vec );

	// compute side vector
	VectorSubtract( start, backEnd.viewParms.or.origin, v1 );
	VectorNormalize( v1 );
	VectorSubtract( end, backEnd.viewParms.or.origin, v2 );
	VectorNormalize( v2 );
	CrossProduct( v1, v2, right );
	VectorNormalize( right );

	for ( i = 0 ; i < 4 ; i++ ) {
		vec3_t	temp;

		DoRailCore( start, end, right, len, 8 );
		RotatePointAroundVector( temp, vec, right, 45 );
		VectorCopy( temp, right );
	}
}



/*
** LerpMeshVertexes
*/
#if idppc_altivec
static void LerpMeshVertexes_altivec(md3Surface_t *surf, float backlerp)
{
	short	*oldXyz, *newXyz, *oldNormals, *newNormals;
	vaWord2_t	*vertexPtr2;
	vaWord3_t	*vertexPtr3;
	float	oldXyzScale ALIGN(16);
	float   newXyzScale ALIGN(16);
	float	oldNormalScale ALIGN(16);
	float newNormalScale ALIGN(16);
	int		vertNum;
	unsigned lat, lng;
	int		numVerts;

	vertexPtr2 = tess.vertexPtr2 + tess.numVertexes;
	vertexPtr3 = tess.vertexPtr3 + tess.numVertexes;

	newXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
		+ (backEnd.currentEntity->e.frame * surf->numVerts * 4);
	newNormals = newXyz + 3;

	newXyzScale = MD3_XYZ_SCALE * (1.0 - backlerp);
	newNormalScale = 1.0 - backlerp;

	numVerts = surf->numVerts;

	if ( backlerp == 0 ) {
		vector signed short newNormalsVec0;
		vector signed short newNormalsVec1;
		vector signed int newNormalsIntVec;
		vector float newNormalsFloatVec;
		vector float newXyzScaleVec;
		vector unsigned char newNormalsLoadPermute;
		vector unsigned char newNormalsStorePermute;
		vector float zero;
		
		newNormalsStorePermute = vec_lvsl(0,(float *)&newXyzScaleVec);
		newXyzScaleVec = *(vector float *)&newXyzScale;
		newXyzScaleVec = vec_perm(newXyzScaleVec,newXyzScaleVec,newNormalsStorePermute);
		newXyzScaleVec = vec_splat(newXyzScaleVec,0);		
		newNormalsLoadPermute = vec_lvsl(0,newXyz);
		newNormalsStorePermute = vec_lvsr(0,outXyz);
		zero = (vector float)vec_splat_s8(0);
		//
		// just copy the vertexes
		//
		for (vertNum=0 ; vertNum < numVerts ; vertNum++,
		       newXyz += 4, newNormals += 4,
		       vertexPtr2++, vertexPtr3++ )
		{
			newNormalsLoadPermute = vec_lvsl(0,newXyz);
			newNormalsStorePermute = vec_lvsr(0,outXyz);
			newNormalsVec0 = vec_ld(0,newXyz);
			newNormalsVec1 = vec_ld(16,newXyz);
			newNormalsVec0 = vec_perm(newNormalsVec0,newNormalsVec1,newNormalsLoadPermute);
			newNormalsIntVec = vec_unpackh(newNormalsVec0);
			newNormalsFloatVec = vec_ctf(newNormalsIntVec,0);
			newNormalsFloatVec = vec_madd(newNormalsFloatVec,newXyzScaleVec,zero);
			newNormalsFloatVec = vec_perm(newNormalsFloatVec,newNormalsFloatVec,newNormalsStorePermute);
			//outXyz[0] = newXyz[0] * newXyzScale;
			//outXyz[1] = newXyz[1] * newXyzScale;
			//outXyz[2] = newXyz[2] * newXyzScale;

			lat = ( newNormals[0] >> 8 ) & 0xff;
			lng = ( newNormals[0] & 0xff );
			lat *= (FUNCTABLE_SIZE/256);
			lng *= (FUNCTABLE_SIZE/256);

			// decode X as cos( lat ) * sin( long )
			// decode Y as sin( lat ) * sin( long )
			// decode Z as cos( long )

			vertexPtr3->normal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			vertexPtr3->normal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			vertexPtr3->normal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

			vec_ste(newNormalsFloatVec,0,vertexPtr2->xyz);
			vec_ste(newNormalsFloatVec,4,vertexPtr2->xyz);
			vec_ste(newNormalsFloatVec,8,vertexPtr2->xyz);
		}
	} else {
		//
		// interpolate and copy the vertex and normal
		//
		oldXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
			+ (backEnd.currentEntity->e.oldframe * surf->numVerts * 4);
		oldNormals = oldXyz + 3;

		oldXyzScale = MD3_XYZ_SCALE * backlerp;
		oldNormalScale = backlerp;

		for (vertNum=0 ; vertNum < numVerts ; vertNum++,
			     oldXyz += 4, newXyz += 4, oldNormals += 4, newNormals += 4,
			     vertexPtr2++, vertexPtr3++ )
		{
			vec3_t uncompressedOldNormal, uncompressedNewNormal;

			// interpolate the xyz
			vertexPtr2->xyz[0] = oldXyz[0] * oldXyzScale + newXyz[0] * newXyzScale;
			vertexPtr2->xyz[1] = oldXyz[1] * oldXyzScale + newXyz[1] * newXyzScale;
			vertexPtr2->xyz[2] = oldXyz[2] * oldXyzScale + newXyz[2] * newXyzScale;

			// FIXME: interpolate lat/long instead?
			lat = ( newNormals[0] >> 8 ) & 0xff;
			lng = ( newNormals[0] & 0xff );
			lat *= 4;
			lng *= 4;
			uncompressedNewNormal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			uncompressedNewNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			uncompressedNewNormal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

			lat = ( oldNormals[0] >> 8 ) & 0xff;
			lng = ( oldNormals[0] & 0xff );
			lat *= 4;
			lng *= 4;

			uncompressedOldNormal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			uncompressedOldNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			uncompressedOldNormal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

			vertexPtr3->normal[0] = uncompressedOldNormal[0] * oldNormalScale + uncompressedNewNormal[0] * newNormalScale;
			vertexPtr3->normal[1] = uncompressedOldNormal[1] * oldNormalScale + uncompressedNewNormal[1] * newNormalScale;
			vertexPtr3->normal[2] = uncompressedOldNormal[2] * oldNormalScale + uncompressedNewNormal[2] * newNormalScale;

			VectorNormalize (vertexPtr3->normal);
		}
   	}
}
#endif

static void LerpMeshVertexes_scalar(md3Surface_t *surf, float backlerp)
{
	short	*oldXyz, *newXyz, *oldNormals, *newNormals;
	vaWord2_t	*vertexPtr2;
	vaWord3_t	*vertexPtr3;
	float	oldXyzScale, newXyzScale;
	float	oldNormalScale, newNormalScale;
	int		vertNum;
	unsigned lat, lng;
	int		numVerts;

	vertexPtr2 = tess.vertexPtr2 + tess.numVertexes;
	vertexPtr3 = tess.vertexPtr3 + tess.numVertexes;

	newXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
		+ (backEnd.currentEntity->e.frame * surf->numVerts * 4);
	newNormals = newXyz + 3;

	newXyzScale = MD3_XYZ_SCALE * (1.0 - backlerp);
	newNormalScale = 1.0 - backlerp;

	numVerts = surf->numVerts;

	if ( backlerp == 0 ) {
		//
		// just copy the vertexes
		//
		for (vertNum=0 ; vertNum < numVerts ; vertNum++,
			     newXyz += 4, newNormals += 4,
			     vertexPtr2++, vertexPtr3++ )
		{

			vertexPtr2->xyz[0] = newXyz[0] * newXyzScale;
			vertexPtr2->xyz[1] = newXyz[1] * newXyzScale;
			vertexPtr2->xyz[2] = newXyz[2] * newXyzScale;

			lat = ( newNormals[0] >> 8 ) & 0xff;
			lng = ( newNormals[0] & 0xff );
			lat *= (FUNCTABLE_SIZE/256);
			lng *= (FUNCTABLE_SIZE/256);

			// decode X as cos( lat ) * sin( long )
			// decode Y as sin( lat ) * sin( long )
			// decode Z as cos( long )

			vertexPtr3->normal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			vertexPtr3->normal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			vertexPtr3->normal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];
		}
	} else {
		//
		// interpolate and copy the vertex and normal
		//
		oldXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
			+ (backEnd.currentEntity->e.oldframe * surf->numVerts * 4);
		oldNormals = oldXyz + 3;

		oldXyzScale = MD3_XYZ_SCALE * backlerp;
		oldNormalScale = backlerp;

		for (vertNum=0 ; vertNum < numVerts ; vertNum++,
			     oldXyz += 4, newXyz += 4, oldNormals += 4, newNormals += 4,
			     vertexPtr2++, vertexPtr3++ )
		{
			vec3_t uncompressedOldNormal, uncompressedNewNormal;

			// interpolate the xyz
			vertexPtr2->xyz[0] = oldXyz[0] * oldXyzScale + newXyz[0] * newXyzScale;
			vertexPtr2->xyz[1] = oldXyz[1] * oldXyzScale + newXyz[1] * newXyzScale;
			vertexPtr2->xyz[2] = oldXyz[2] * oldXyzScale + newXyz[2] * newXyzScale;

			// FIXME: interpolate lat/long instead?
			lat = ( newNormals[0] >> 8 ) & 0xff;
			lng = ( newNormals[0] & 0xff );
			lat *= 4;
			lng *= 4;
			uncompressedNewNormal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			uncompressedNewNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			uncompressedNewNormal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

			lat = ( oldNormals[0] >> 8 ) & 0xff;
			lng = ( oldNormals[0] & 0xff );
			lat *= 4;
			lng *= 4;

			uncompressedOldNormal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			uncompressedOldNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			uncompressedOldNormal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

			vertexPtr3->normal[0] = uncompressedOldNormal[0] * oldNormalScale + uncompressedNewNormal[0] * newNormalScale;
			vertexPtr3->normal[1] = uncompressedOldNormal[1] * oldNormalScale + uncompressedNewNormal[1] * newNormalScale;
			vertexPtr3->normal[2] = uncompressedOldNormal[2] * oldNormalScale + uncompressedNewNormal[2] * newNormalScale;

			VectorNormalize (vertexPtr3->normal);
		}
   	}
}

static void LerpMeshVertexes(md3Surface_t *surf, float backlerp)
{
#if idppc_altivec
	if (com_altivec->integer) {
		// must be in a seperate function or G3 systems will crash.
		LerpMeshVertexes_altivec( surf, backlerp );
		return;
	}
#endif // idppc_altivec
	LerpMeshVertexes_scalar( surf, backlerp );
}


/*
=============
RB_SurfaceMesh
=============
*/
void RB_SurfaceMesh( surfaceType_t *surf) {
	md3Surface_t *surface = (md3Surface_t *)surf;
	int				j;
	float			backlerp;
	int				*triangles;
	float			*texCoords;
	int				indexes;
	int				Doug;
	int				numVerts;
	vaWord1_t		*vertexPtr1;
	vaWord2_t		*vertexPtr2;

	if (  backEnd.currentEntity->e.oldframe == backEnd.currentEntity->e.frame ) {
		backlerp = 0;
	} else  {
		backlerp = backEnd.currentEntity->e.backlerp;
	}

	if ( tess.vertexPtr1 ) {
		LerpMeshVertexes (surface, backlerp);
		
		Doug = tess.numVertexes;
		texCoords = (float *) ((byte *)surface + surface->ofsSt);
		vertexPtr1 = tess.vertexPtr1 + Doug;
		vertexPtr2 = tess.vertexPtr2 + Doug;

		numVerts = surface->numVerts;
		for ( j = 0; j < numVerts; j++, vertexPtr1++, vertexPtr2++ ) {
			vertexPtr1->tc1[0] = vertexPtr1->tc2[0] = texCoords[j*2+0];
			vertexPtr1->tc1[1] = vertexPtr1->tc2[1] = texCoords[j*2+1];
			vertexPtr2->fogNum = tess.fogNum;
		}
	} else {
		Doug = 0;
	}
	
	if ( tess.indexPtr.p16 ) {
		if ( tess.minIndex > Doug )
			tess.minIndex = Doug;
		if ( tess.maxIndex < Doug + surface->numVerts - 1 )
			tess.maxIndex = Doug + surface->numVerts - 1;

		triangles = (int *) ((byte *)surface + surface->ofsTriangles);
		indexes = surface->numTriangles * 3;

		if ( tess.indexInc == sizeof(GLushort) ) {
			GLushort *indexPtr = tess.indexPtr.p16 + tess.numIndexes;
			for (j = 0 ; j < indexes ; j++) {
				*indexPtr++ = Doug + triangles[j];
			}
		} else {
			GLuint *indexPtr32 = tess.indexPtr.p32 + tess.numIndexes;
			for (j = 0 ; j < indexes ; j++) {
				*indexPtr32++ = Doug + triangles[j];
			}
		}
	}
	tess.numVertexes += surface->numVerts;
	tess.numIndexes  += 3*surface->numTriangles;
}


/*
==============
RB_SurfaceFace
==============
*/
static void RB_SurfaceFace( surfaceType_t *surface ) {
	srfSurfaceFace_t *surf = (srfSurfaceFace_t *)surface;
	int			i;
	unsigned	*indices;
	float		*v;
	int			Bob;
	int			numPoints;
	int			dlightBits;
	vaWord1_t	*vertexPtr1;
	vaWord2_t	*vertexPtr2;
	vaWord3_t	*vertexPtr3;

	dlightBits = surf->dlightBits[backEnd.smpFrame];
	tess.dlightBits |= dlightBits;

	if ( tess.vertexPtr1 ) {
		Bob = tess.numVertexes;

		v = surf->points[0];
		
		numPoints = surf->numPoints;
		
		vertexPtr1 = tess.vertexPtr1 + tess.numVertexes;
		vertexPtr2 = tess.vertexPtr2 + tess.numVertexes;
		vertexPtr3 = tess.vertexPtr3 + tess.numVertexes;
		
		for ( i = 0, v = surf->points[0]; i < numPoints; i++, v += VERTEXSIZE,
			      vertexPtr1++, vertexPtr2++, vertexPtr3++ ) {
			VectorCopy( surf->plane.normal, vertexPtr3->normal );
			
			VectorCopy ( v, *(vec3_t *)&vertexPtr2->xyz);
			Vector2Copy( v+3, vertexPtr1->tc1 );
			Vector2Copy( v+5, vertexPtr1->tc2 );
			*(unsigned int *)&vertexPtr3->color = *(unsigned int *)&v[7];
			vertexPtr2->fogNum = tess.fogNum;
		}
	} else {
		Bob = surf->vboStart;
	}

	if ( tess.indexPtr.p16 ) {
		if ( tess.minIndex > Bob )
			tess.minIndex = Bob;
		if ( tess.maxIndex < Bob + surf->numPoints - 1 )
			tess.maxIndex = Bob + surf->numPoints - 1;
		
		indices = ( unsigned * ) ( ( ( char  * ) surf ) + surf->ofsIndices );
		if ( tess.indexInc == sizeof(GLushort) ) {
			GLushort *indexPtr = tess.indexPtr.p16 + tess.numIndexes;
			for ( i = surf->numIndices-1 ; i >= 0  ; i-- ) {
				indexPtr[i] = (GLushort)(indices[i] + Bob);
			}
		} else {
			GLuint *indexPtr32 = tess.indexPtr.p32 + tess.numIndexes;
			for ( i = surf->numIndices-1 ; i >= 0  ; i-- ) {
				indexPtr32[i] = indices[i] + Bob;
			}
		}
	}

	tess.numVertexes += surf->numPoints;
	tess.numIndexes  += surf->numIndices;
}


static float	LodErrorForVolume( vec3_t local, float radius ) {
	vec3_t		world;
	float		d;

	// never let it go negative
	if ( r_lodCurveError->value < 0 ) {
		return 0;
	}

	world[0] = local[0] * backEnd.or.axis[0][0] + local[1] * backEnd.or.axis[1][0] + 
		local[2] * backEnd.or.axis[2][0] + backEnd.or.origin[0];
	world[1] = local[0] * backEnd.or.axis[0][1] + local[1] * backEnd.or.axis[1][1] + 
		local[2] * backEnd.or.axis[2][1] + backEnd.or.origin[1];
	world[2] = local[0] * backEnd.or.axis[0][2] + local[1] * backEnd.or.axis[1][2] + 
		local[2] * backEnd.or.axis[2][2] + backEnd.or.origin[2];

	VectorSubtract( world, backEnd.viewParms.or.origin, world );
	d = DotProduct( world, backEnd.viewParms.or.axis[0] );

	if ( d < 0 ) {
		d = -d;
	}
	d -= radius;
	if ( d < 1 ) {
		d = 1;
	}

	return r_lodCurveError->value / d;
}

/*
=============
RB_SurfaceGrid

Just copy the grid of points and triangulate
=============
*/
static void RB_SurfaceGrid( surfaceType_t *surface ) {
	srfGridMesh_t *cv = (srfGridMesh_t *)surface;
	int		i, j;
	vaWord1_t	*vertexPtr1;
	vaWord2_t	*vertexPtr2;
	vaWord3_t	*vertexPtr3;
	drawVert_t	*dv;
	int		widthTable[MAX_GRID_SIZE];
	int		heightTable[MAX_GRID_SIZE];
	float	lodError;
	int		lodWidth, lodHeight;
	int		baseVertex;
	int		dlightBits;

	dlightBits = cv->dlightBits[backEnd.smpFrame];
	tess.dlightBits |= dlightBits;

	// determine the allowable discrepance
	if ( r_ext_vertex_buffer_object->integer ) {
		// always render max res for VBOs
		lodError = r_lodCurveError->value;
	} else {
		lodError = LodErrorForVolume( cv->lodOrigin, cv->lodRadius );
	}

	// determine which rows and columns of the subdivision
	// we are actually going to use
	widthTable[0] = 0;
	lodWidth = 1;
	for ( i = 1 ; i < cv->width-1 ; i++ ) {
		if ( cv->widthLodError[i] <= lodError ) {
			widthTable[lodWidth] = i;
			lodWidth++;
		}
	}
	widthTable[lodWidth] = cv->width-1;
	lodWidth++;

	heightTable[0] = 0;
	lodHeight = 1;
	for ( i = 1 ; i < cv->height-1 ; i++ ) {
		if ( cv->heightLodError[i] <= lodError ) {
			heightTable[lodHeight] = i;
			lodHeight++;
		}
	}
	heightTable[lodHeight] = cv->height-1;
	lodHeight++;

	if ( tess.vertexPtr1 ) {
		baseVertex = tess.numVertexes;
		vertexPtr1 = tess.vertexPtr1 + baseVertex;
		vertexPtr2 = tess.vertexPtr2 + baseVertex;
		vertexPtr3 = tess.vertexPtr3 + baseVertex;
		
		for ( i = 0 ; i < lodHeight ; i++ ) {
			for ( j = 0 ; j < lodWidth ; j++,
				      vertexPtr1++, vertexPtr2++, vertexPtr3++ ) {
				dv = cv->verts + heightTable[ i ] * cv->width
					+ widthTable[ j ];
				
				VectorCopy( dv->xyz, vertexPtr2->xyz );
				Vector2Copy ( dv->st, vertexPtr1->tc1 );
				Vector2Copy ( dv->lightmap, vertexPtr1->tc2 );
				VectorCopy( dv->normal, vertexPtr3->normal );
				*(unsigned int *)&vertexPtr3->color = *(unsigned int *)dv->color;
				vertexPtr2->fogNum = tess.fogNum;
			}
		}
	} else {
		baseVertex = cv->vboStart;
	}
	
	if ( tess.indexPtr.p16 ) {
		if ( tess.minIndex > baseVertex )
			tess.minIndex = baseVertex;
		if ( tess.maxIndex < baseVertex + lodWidth*lodHeight - 1 )
			tess.maxIndex = baseVertex + lodWidth*lodHeight - 1;

		// add the indexes
		int		w, h;
		
		h = lodHeight - 1;
		w = lodWidth - 1;
		
		if ( tess.indexInc == sizeof(GLushort) ) {
			GLushort *indexPtr = tess.indexPtr.p16 + tess.numIndexes;
			for (i = 0 ; i < h ; i++) {
				for (j = 0 ; j < w ; j++) {
					int		v1, v2, v3, v4;
			
					// vertex order to be reckognized as tristrips
					v1 = baseVertex + i*lodWidth + j + 1;
					v2 = v1 - 1;
					v3 = v2 + lodWidth;
					v4 = v3 + 1;
					
					*indexPtr++ = v2;
					*indexPtr++ = v3;
					*indexPtr++ = v1;
					
					*indexPtr++ = v1;
					*indexPtr++ = v3;
					*indexPtr++ = v4;
				}
			}
		} else {
			GLuint *indexPtr32 = tess.indexPtr.p32 + tess.numIndexes;
			for (i = 0 ; i < h ; i++) {
				for (j = 0 ; j < w ; j++) {
					int		v1, v2, v3, v4;
					
					// vertex order to be reckognized as tristrips
					v1 = baseVertex + i*lodWidth + j + 1;
					v2 = v1 - 1;
					v3 = v2 + lodWidth;
					v4 = v3 + 1;
					
					*indexPtr32++ = v2;
					*indexPtr32++ = v3;
					*indexPtr32++ = v1;
					
					*indexPtr32++ = v1;
					*indexPtr32++ = v3;
					*indexPtr32++ = v4;
				}
			}
		}
	}
	
	tess.numVertexes += lodWidth * lodHeight;
	tess.numIndexes  += 6 * (lodWidth - 1) * (lodHeight - 1);
}


/*
===========================================================================

NULL MODEL

===========================================================================
*/

/*
===================
RB_SurfaceAxis

Draws x/y/z lines from the origin for orientation debugging
===================
*/
static void RB_SurfaceAxis( void ) {
	static vec3_t vertexes[6] = {
		{  0,  0,  0 },
		{ 16,  0,  0 },
		{  0,  0,  0 },
		{  0, 16,  0 },
		{  0,  0,  0 },
		{  0,  0, 16 }
	};
	static color4ub_t colors[6] = {
		{ 255,   0,   0, 255 },
		{ 255,   0,   0, 255 },
		{   0, 255,   0, 255 },
		{   0, 255,   0, 255 },
		{   0,   0, 255, 255 },
		{   0,   0, 255, 255 }
	};
	GL_Program( 0 );
	GL_VBO( 0, 0 );

	GL_BindImages( 1, &tr.whiteImage );
	qglLineWidth( 3 );
	qglVertexPointer( 3, GL_FLOAT, 0, vertexes );
	GL_ColorPtr( colors, sizeof(color4ub_t) );
	qglDrawArrays( GL_LINES, 0, 6 );
	qglLineWidth( 1 );
}

//===========================================================================

/*
====================
RB_SurfaceEntity

Entities that have a single procedurally generated surface
====================
*/
static void RB_SurfaceEntity( surfaceType_t *surfType ) {
	switch( backEnd.currentEntity->e.reType ) {
	case RT_SPRITE:
		RB_SurfaceSprite();
		break;
	case RT_BEAM:
		RB_SurfaceBeam();
		break;
	case RT_RAIL_CORE:
		RB_SurfaceRailCore();
		break;
	case RT_RAIL_RINGS:
		RB_SurfaceRailRings();
		break;
	case RT_LIGHTNING:
		RB_SurfaceLightningBolt();
		break;
	default:
		RB_SurfaceAxis();
		break;
	}
	return;
}

static void RB_SurfaceBad( surfaceType_t *surfType ) {
	ri.Printf( PRINT_ALL, "Bad surface tesselated.\n" );
}

static void RB_SurfaceFlare( surfaceType_t *surface )
{
	srfFlare_t *surf = (srfFlare_t *)surface;

	if (r_flares->integer)
		RB_AddFlare(surf, tess.fogNum, surf->origin, surf->color, surf->normal);
}

static void RB_SurfaceDisplayList( surfaceType_t *surface ) {
	srfDisplayList_t *surf = (srfDisplayList_t *)surface;
	// all apropriate state must be set in RB_BeginSurface
	// this isn't implemented yet...
	qglCallList( surf->listNum );
}

static void RB_SurfaceImpostor( surfaceType_t *surface ) {
	srfImpostor_t	*surf = (srfImpostor_t *)surface;

	if ( tess.vertexPtr1 ) {
		vaWord1_t	*vertexPtr1 = tess.vertexPtr1 + tess.numVertexes;
		vaWord2_t	*vertexPtr2 = tess.vertexPtr2 + tess.numVertexes;

		vertexPtr2->xyz[0] = surf->bounds[0][0];
		vertexPtr2->xyz[1] = surf->bounds[0][1];
		vertexPtr2->xyz[2] = surf->bounds[0][2];
		vertexPtr1->tc1[0] = 0.0f;
		vertexPtr1->tc1[1] = 0.0f;
		vertexPtr1++; vertexPtr2++;

		vertexPtr2->xyz[0] = surf->bounds[1][0];
		vertexPtr2->xyz[1] = surf->bounds[0][1];
		vertexPtr2->xyz[2] = surf->bounds[0][2];
		vertexPtr1->tc1[0] = 1.0f;
		vertexPtr1->tc1[1] = 0.0f;
		vertexPtr1++; vertexPtr2++;

		vertexPtr2->xyz[0] = surf->bounds[0][0];
		vertexPtr2->xyz[1] = surf->bounds[0][1];
		vertexPtr2->xyz[2] = surf->bounds[1][2];
		vertexPtr1->tc1[0] = 0.0f;
		vertexPtr1->tc1[1] = 1.0f;
		vertexPtr1++; vertexPtr2++;

		vertexPtr2->xyz[0] = surf->bounds[1][0];
		vertexPtr2->xyz[1] = surf->bounds[0][1];
		vertexPtr2->xyz[2] = surf->bounds[1][2];
		vertexPtr1->tc1[0] = 1.0f;
		vertexPtr1->tc1[1] = 1.0f;
		vertexPtr1++; vertexPtr2++;

		vertexPtr2->xyz[0] = surf->bounds[0][0];
		vertexPtr2->xyz[1] = surf->bounds[1][1];
		vertexPtr2->xyz[2] = surf->bounds[1][2];
		vertexPtr1->tc1[0] = 0.0f;
		vertexPtr1->tc1[1] = 0.0f;
		vertexPtr1++; vertexPtr2++;

		vertexPtr2->xyz[0] = surf->bounds[1][0];
		vertexPtr2->xyz[1] = surf->bounds[1][1];
		vertexPtr2->xyz[2] = surf->bounds[1][2];
		vertexPtr1->tc1[0] = 1.0f;
		vertexPtr1->tc1[1] = 0.0f;
		vertexPtr1++; vertexPtr2++;

		vertexPtr2->xyz[0] = surf->bounds[1][0];
		vertexPtr2->xyz[1] = surf->bounds[0][1];
		vertexPtr2->xyz[2] = surf->bounds[0][2];
		vertexPtr1->tc1[0] = 0.0f;
		vertexPtr1->tc1[1] = 1.0f;
		vertexPtr1++; vertexPtr2++;

		vertexPtr2->xyz[0] = surf->bounds[1][0];
		vertexPtr2->xyz[1] = surf->bounds[1][1];
		vertexPtr2->xyz[2] = surf->bounds[0][2];
		vertexPtr1->tc1[0] = 0.0f;
		vertexPtr1->tc1[1] = 0.0f;
		vertexPtr1++; vertexPtr2++;

		vertexPtr2->xyz[0] = surf->bounds[0][0];
		vertexPtr2->xyz[1] = surf->bounds[1][1];
		vertexPtr2->xyz[2] = surf->bounds[1][2];
		vertexPtr1->tc1[0] = 1.0f;
		vertexPtr1->tc1[1] = 1.0f;
		vertexPtr1++; vertexPtr2++;

		vertexPtr2->xyz[0] = surf->bounds[0][0];
		vertexPtr2->xyz[1] = surf->bounds[1][1];
		vertexPtr2->xyz[2] = surf->bounds[0][2];
		vertexPtr1->tc1[0] = 0.0f;
		vertexPtr1->tc1[1] = 1.0f;
		vertexPtr1++; vertexPtr2++;

		vertexPtr2->xyz[0] = surf->bounds[0][0];
		vertexPtr2->xyz[1] = surf->bounds[0][1];
		vertexPtr2->xyz[2] = surf->bounds[0][2];
		vertexPtr1->tc1[0] = 1.0f;
		vertexPtr1->tc1[1] = 1.0f;
		vertexPtr1++; vertexPtr2++;

		vertexPtr2->xyz[0] = surf->bounds[0][0];
		vertexPtr2->xyz[1] = surf->bounds[0][1];
		vertexPtr2->xyz[2] = surf->bounds[1][2];
		vertexPtr1->tc1[0] = 1.0f;
		vertexPtr1->tc1[1] = 0.0f;
		vertexPtr1++; vertexPtr2++;
	}

	if ( tess.indexPtr.p16 ) {
		if ( tess.minIndex > tess.numIndexes )
			tess.minIndex = tess.numIndexes;
		if ( tess.maxIndex < tess.numIndexes + 35 )
			tess.maxIndex = tess.numIndexes + 35;

		if ( tess.indexInc == sizeof(GLushort) ) {
			GLushort *indexPtr = tess.indexPtr.p16 + tess.numIndexes;
			indexPtr[ 0] = tess.numVertexes + 0;
			indexPtr[ 1] = tess.numVertexes + 2;
			indexPtr[ 2] = tess.numVertexes + 1;
			indexPtr[ 3] = tess.numVertexes + 1;
			indexPtr[ 4] = tess.numVertexes + 2;
			indexPtr[ 5] = tess.numVertexes + 3;
			indexPtr[ 6] = tess.numVertexes + 3;
			indexPtr[ 7] = tess.numVertexes + 2;
			indexPtr[ 8] = tess.numVertexes + 4;
			indexPtr[ 9] = tess.numVertexes + 3;
			indexPtr[10] = tess.numVertexes + 4;
			indexPtr[11] = tess.numVertexes + 5;
			indexPtr[12] = tess.numVertexes + 3;
			indexPtr[13] = tess.numVertexes + 5;
			indexPtr[14] = tess.numVertexes + 6;
			indexPtr[15] = tess.numVertexes + 6;
			indexPtr[16] = tess.numVertexes + 5;
			indexPtr[17] = tess.numVertexes + 7;
			indexPtr[18] = tess.numVertexes + 7;
			indexPtr[19] = tess.numVertexes + 5;
			indexPtr[20] = tess.numVertexes + 8;
			indexPtr[21] = tess.numVertexes + 7;
			indexPtr[22] = tess.numVertexes + 8;
			indexPtr[23] = tess.numVertexes + 9;
			indexPtr[24] = tess.numVertexes + 1;
			indexPtr[25] = tess.numVertexes + 7;
			indexPtr[26] = tess.numVertexes + 9;
			indexPtr[27] = tess.numVertexes + 1;
			indexPtr[28] = tess.numVertexes + 9;
			indexPtr[29] = tess.numVertexes + 10;
			indexPtr[30] = tess.numVertexes + 9;
			indexPtr[31] = tess.numVertexes + 4;
			indexPtr[32] = tess.numVertexes + 10;
			indexPtr[33] = tess.numVertexes + 10;
			indexPtr[34] = tess.numVertexes + 4;
			indexPtr[35] = tess.numVertexes + 11;
		} else {
			GLuint *indexPtr32 = tess.indexPtr.p32 + tess.numIndexes;
			indexPtr32[ 0] = tess.numVertexes + 0;
			indexPtr32[ 1] = tess.numVertexes + 2;
			indexPtr32[ 2] = tess.numVertexes + 1;
			indexPtr32[ 3] = tess.numVertexes + 1;
			indexPtr32[ 4] = tess.numVertexes + 2;
			indexPtr32[ 5] = tess.numVertexes + 3;
			indexPtr32[ 6] = tess.numVertexes + 3;
			indexPtr32[ 7] = tess.numVertexes + 2;
			indexPtr32[ 8] = tess.numVertexes + 4;
			indexPtr32[ 9] = tess.numVertexes + 3;
			indexPtr32[10] = tess.numVertexes + 4;
			indexPtr32[11] = tess.numVertexes + 5;
			indexPtr32[12] = tess.numVertexes + 3;
			indexPtr32[13] = tess.numVertexes + 5;
			indexPtr32[14] = tess.numVertexes + 6;
			indexPtr32[15] = tess.numVertexes + 6;
			indexPtr32[16] = tess.numVertexes + 5;
			indexPtr32[17] = tess.numVertexes + 7;
			indexPtr32[18] = tess.numVertexes + 7;
			indexPtr32[19] = tess.numVertexes + 5;
			indexPtr32[20] = tess.numVertexes + 8;
			indexPtr32[21] = tess.numVertexes + 7;
			indexPtr32[22] = tess.numVertexes + 8;
			indexPtr32[23] = tess.numVertexes + 9;
			indexPtr32[24] = tess.numVertexes + 1;
			indexPtr32[25] = tess.numVertexes + 7;
			indexPtr32[26] = tess.numVertexes + 9;
			indexPtr32[27] = tess.numVertexes + 1;
			indexPtr32[28] = tess.numVertexes + 9;
			indexPtr32[29] = tess.numVertexes + 10;
			indexPtr32[30] = tess.numVertexes + 9;
			indexPtr32[31] = tess.numVertexes + 4;
			indexPtr32[32] = tess.numVertexes + 10;
			indexPtr32[33] = tess.numVertexes + 10;
			indexPtr32[34] = tess.numVertexes + 4;
			indexPtr32[35] = tess.numVertexes + 11;
		}
	}
	tess.numVertexes += 12;
	tess.numIndexes += 36;
}

static void RB_SurfaceSkip( surfaceType_t *surf ) {
}


void (*rb_surfaceTable[SF_NUM_SURFACE_TYPES])( surfaceType_t * ) = {
	RB_SurfaceBad,			// SF_BAD, 
	RB_SurfaceSkip,			// SF_SKIP, 
	RB_SurfaceFace,			// SF_FACE,
	RB_SurfaceGrid,			// SF_GRID,
	RB_SurfaceTriangles,		// SF_TRIANGLES,
	RB_SurfacePolychain,		// SF_POLY,
	RB_SurfaceMesh,			// SF_MD3,
	RB_SurfaceAnim,			// SF_MD4,
#ifdef RAVENMD4
	RB_MDRSurfaceAnim,		// SF_MDR,
#endif
	RB_SurfaceFlare,		// SF_FLARE,
	RB_SurfaceEntity,		// SF_ENTITY
	RB_SurfaceDisplayList,		// SF_DISPLAY_LIST
	RB_SurfaceImpostor		// SF_IMPOSTOR
};
