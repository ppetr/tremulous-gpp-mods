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
// tr_shade.c

#include "tr_local.h" 
#if idppc_altivec && !defined(MACOS_X)
#include <altivec.h>
#endif

/*

  THIS ENTIRE FILE IS BACK END

  This file deals with applying shaders to surface data in the tess struct.
*/

// "pseudo pointers" that tell the shader to bind some static data
#define COLOR_BIND_NONE ((color4ub_t *)0)
#define COLOR_BIND_VERT ((color4ub_t *)1)

#define TC_BIND_NONE ((vec2_t *)0)
#define TC_BIND_TEXT ((vec2_t *)1)
#define TC_BIND_LMAP ((vec2_t *)2)


/*
==================
R_DrawElements

Optionally performs our own glDrawElements that looks for strip conditions
instead of using the single glDrawElements call that may be inefficient
without compiled vertex arrays.
==================
*/
static void R_DrawElements( int numIndexes, const void *indexes,
			    GLuint start, GLuint end, GLuint max ) {
	GLenum          type = max > 65535 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;

	if ( qglDrawRangeElementsEXT )
		qglDrawRangeElementsEXT( GL_TRIANGLES, 
					 start, end,
					 numIndexes,
					 type,
					 indexes );
	else
		qglDrawElements( GL_TRIANGLES,
				 numIndexes,
				 type,
				 indexes );
}


/*
=============================================================

SURFACE SHADERS

=============================================================
*/

shaderCommands_t	tess;

/*
=================
R_BindAnimatedImage

=================
*/
static void R_GetAnimatedImage( textureBundle_t *bundle, qboolean combined,
				image_t **pImage ) {
	int		index;

	if ( bundle->isVideoMap ) {
		ri.CIN_RunCinematic(bundle->videoMapHandle);
		ri.CIN_UploadCinematic(bundle->videoMapHandle);
		return;
	}

	if ( bundle->numImageAnimations <= 1 ) {
		*pImage = bundle->image[0];
		return;
	}

	if ( combined && bundle->combinedImage ) {
		*pImage = bundle->combinedImage;
		return;
	}

	// it is necessary to do this messy calc to make sure animations line up
	// exactly with waveforms of the same frequency
	index = myftol( tess.shaderTime * bundle->imageAnimationSpeed * FUNCTABLE_SIZE );
	index >>= FUNCTABLE_SIZE2;

	if ( index < 0 ) {
		index = 0;	// may happen with shader time offsets
	}
	index %= bundle->numImageAnimations;

	*pImage = bundle->image[ index ];
}

/*
================
DrawTris

Draws triangle outlines for debugging
This requires that all vertex pointers etc. are still bound from the StageIterator
================
*/
static void DrawTris ( int numIndexes, const void *indexes,
		       GLuint start, GLuint end, GLuint max,
		       float r, float g, float b ) {
	unsigned long oldState = glState.glStateBits;
	
	GL_BindImages( 1, &tr.whiteImage );

	GL_State( GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE );
	qglDepthRange( 0, 0 );

	if ( tess.currentStageIteratorFunc == RB_StageIteratorGLSL ) {
		shader_t *shader = tess.shader->depthShader;
		if( !shader ) shader = tr.defaultShader->depthShader;
		if( !shader ) shader = tr.defaultShader;
		GL_Program( shader->GLSLprogram );
		GL_Color4f( r, g, b, 1.0f );
	} else {
		GL_Program( 0 );
		GL_Color4f( r, g, b, 1.0f );
	}
	
	R_DrawElements( numIndexes, indexes, start, end, max );

	GL_State( oldState );
	qglDepthRange( 0, 1 );
}


/*
================
DrawNormals

Draws vertex normals for debugging
================
*/
static void DrawNormals ( void ) {
	int		i;
	vec3_t	*temp;

	if( backEnd.projection2D )
		return;

	qglDepthRange( 0, 0 );	// never occluded
	GL_State( GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE );

	if( tess.currentStageIteratorFunc == RB_StageIteratorGLSL ) {
		// TODO: use geometry shaders or render-to-vbo
	} else {
		if( tess.numVertexes > 0 ) {
			temp = ri.Hunk_AllocateTempMemory( tess.numVertexes * 2 * sizeof(vec3_t) );
			GL_Program( 0 );
			GL_BindImages( 0, NULL );
			GL_Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
			GL_State( GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE );
			
			for (i = 0 ; i < tess.numVertexes ; i++) {
				VectorCopy( tess.vertexPtr2[i].xyz, temp[2*i] );
				VectorMA( tess.vertexPtr2[i].xyz, 2, tess.vertexPtr3[i].normal, temp[2*i+1] );
			}
			qglVertexPointer( 3, GL_FLOAT, 0, temp );
			qglDrawArrays( GL_LINES, 0, 2 * tess.numVertexes );
			ri.Hunk_FreeTempMemory( temp );
		}
 	}
	qglDepthRange( 0, 1 );
}

/*
==============
RB_BeginSurface

We must set some things up before beginning any tesselation,
because a surface may be forced to perform a RB_End due
to overflow.
==============
*/
void RB_BeginSurface( shader_t *shader, int fogNum ) {

	shader_t *state = (shader->remappedShader) ? shader->remappedShader : shader;

	tess.numIndexes = 0;
	tess.minIndex = backEnd.worldVBO.maxIndex;
	tess.maxIndex = 0;
	tess.numVertexes = 0;
	tess.firstVBO = NULL;
	tess.shader = state;
	tess.fogNum = fogNum;
	tess.dlightBits = 0;		// will be OR'd in by surface functions
	tess.xstages = state->stages;
	tess.numPasses = state->numUnfoggedPasses;
	tess.currentStageIteratorFunc = state->optimalStageIteratorFunc;

	tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;
	if (tess.shader->clampTime && tess.shaderTime >= tess.shader->clampTime) {
		tess.shaderTime = tess.shader->clampTime;
	}


}

/*
===================
DrawMultitextured

output = t0 * t1 or t0 + t1

t0 = most upstream according to spec
t1 = most downstream according to spec
===================
*/
static void DrawMultitextured( shaderCommands_t *input, int stage, GLuint max ) {
	shaderStage_t	*pStage;
	int		bundle;
	image_t		*images[NUM_TEXTURE_BUNDLES];

	pStage = tess.xstages[stage];

	GL_Program( 0 );
	GL_State( pStage->stateBits );

	// this is an ugly hack to work around a GeForce driver
	// bug with multitexture and clip planes
	if ( backEnd.viewParms.isPortal ) {
		qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	}

	//
	// base
	//
	if ( input->svars.texcoords[0] == TC_BIND_NONE ) {
		GL_TexCoordPtr( 0, 2, GL_FLOAT, sizeof(vaWord1_t), NULL );
	} else if ( input->svars.texcoords[0] == TC_BIND_TEXT ) {
		GL_TexCoordPtr( 0, 2, GL_FLOAT, sizeof(vaWord1_t), &input->vertexPtr1[0].tc1[0] );
	} else if ( input->svars.texcoords[0] == TC_BIND_LMAP ) {
		GL_TexCoordPtr( 0, 2, GL_FLOAT, sizeof(vaWord1_t), &input->vertexPtr1[0].tc2[0] );
	} else {
		GL_TexCoordPtr( 0, 2, GL_FLOAT, 0, input->svars.texcoords[0][0] );
	}
	R_GetAnimatedImage( &pStage->bundle[0], qfalse, &images[0] );

	//
	// lightmap/secondary passes
	//
	for ( bundle = 1; bundle < glConfig.numTextureUnits; bundle++ ) {
		if ( !pStage->bundle[bundle].multitextureEnv )
			break;
		
		if ( input->svars.texcoords[bundle] == TC_BIND_NONE ) {
			GL_TexCoordPtr( bundle, 2, GL_FLOAT, sizeof(vaWord1_t), NULL );
		} else if ( input->svars.texcoords[bundle] == TC_BIND_TEXT ) {
			GL_TexCoordPtr( bundle, 2, GL_FLOAT, sizeof(vaWord1_t), &input->vertexPtr1[0].tc1[0] );
		} else if ( input->svars.texcoords[bundle] == TC_BIND_LMAP ) {
			GL_TexCoordPtr( bundle, 2, GL_FLOAT, sizeof(vaWord1_t), &input->vertexPtr1[0].tc2[0] );
		} else {
			GL_TexCoordPtr( bundle, 2, GL_FLOAT, 0, input->svars.texcoords[bundle][0] );
		}
		
		if ( r_lightmap->integer ) {
			GL_TexEnv( bundle, GL_REPLACE );
		} else {
			GL_TexEnv( bundle, pStage->bundle[bundle].multitextureEnv );
		}

		R_GetAnimatedImage( &pStage->bundle[bundle], qfalse, &images[bundle] );
	}

	GL_BindImages( bundle, images );
	R_DrawElements( input->numIndexes, input->indexPtr.p16,
			input->minIndex, input->maxIndex, max );

	//
	// disable texturing on TEXTURE>=1, then select TEXTURE0
	//
	--bundle;
	while ( bundle > 0 ) {
		if ( input->svars.texcoords[bundle] != TC_BIND_NONE &&
		     input->svars.texcoords[bundle] != TC_BIND_TEXT &&
		     input->svars.texcoords[bundle] != TC_BIND_LMAP ) {
			ri.Hunk_FreeTempMemory( input->svars.texcoords[bundle] );
		}
		bundle--;
	}
}



/*
===================
ProjectDlightTexture

Perform dynamic lighting with another rendering pass
===================
*/
#if idppc_altivec
static void ProjectDlightTexture_altivec( void ) {
	int		i, l;
	vec_t	origin0, origin1, origin2;
	float   texCoords0, texCoords1;
	vector float floatColorVec0, floatColorVec1;
	vector float modulateVec, colorVec, zero;
	vector short colorShort;
	vector signed int colorInt;
	vector unsigned char floatColorVecPerm, modulatePerm, colorChar;
	vector unsigned char vSel = VECCONST_UINT8(0x00, 0x00, 0x00, 0xff,
                                               0x00, 0x00, 0x00, 0xff,
                                               0x00, 0x00, 0x00, 0xff,
                                               0x00, 0x00, 0x00, 0xff);
	vec2_t	*texCoords;
	color4ub_t	*colors;
	byte	*clipBits;
	vec2_t	*texCoordsArray;
	color4ub_t	*colorArray;
	GLushort	*hitIndexes;
	int		numIndexes;
	float	scale;
	float	radius;
	vec3_t	floatColor;
	float	modulate = 0.0f;

	if ( !backEnd.refdef.num_dlights ||
	     tess.numVertexes > 65535 ) { // to avoid GLushort overflow
		return;
	}

	clipBits = ri.Hunk_AllocateTempMemory( sizeof(byte) * tess.numVertexes );
	texCoordsArray = ri.Hunk_AllocateTempMemory( sizeof(vec2_t) * tess.numVertexes );
	colorArray = ri.Hunk_AllocateTempMemory( sizeof(color4ub_t) * tess.numVertexes );
	hitIndexes = ri.Hunk_AllocateTempMemory( sizeof(GLushort) * tess.numIndexes );

	// There has to be a better way to do this so that floatColor
	// and/or modulate are already 16-byte aligned.
	floatColorVecPerm = vec_lvsl(0,(float *)floatColor);
	modulatePerm = vec_lvsl(0,(float *)&modulate);
	modulatePerm = (vector unsigned char)vec_splat((vector unsigned int)modulatePerm,0);
	zero = (vector float)vec_splat_s8(0);

	for ( l = 0 ; l < backEnd.refdef.num_dlights ; l++ ) {
		dlight_t	*dl;

		if ( !( tess.dlightBits & ( 1 << l ) ) ) {
			continue;	// this surface definately doesn't have any of this light
		}
		texCoords = texCoordsArray;
		colors = colorArray;

		dl = &backEnd.refdef.dlights[l];
		origin0 = dl->transformed[0];
		origin1 = dl->transformed[1];
		origin2 = dl->transformed[2];
		radius = dl->radius;
		scale = 1.0f / radius;

		if(r_greyscale->integer)
		{
			float luminance;
			
			luminance = (dl->color[0] * 255.0f + dl->color[1] * 255.0f + dl->color[2] * 255.0f) / 3;
			floatColor[0] = floatColor[1] = floatColor[2] = luminance;
		}
		else
		{
			floatColor[0] = dl->color[0] * 255.0f;
			floatColor[1] = dl->color[1] * 255.0f;
			floatColor[2] = dl->color[2] * 255.0f;
		}
		floatColorVec0 = vec_ld(0, floatColor);
		floatColorVec1 = vec_ld(11, floatColor);
		floatColorVec0 = vec_perm(floatColorVec0,floatColorVec0,floatColorVecPerm);
		for ( i = 0 ; i < tess.numVertexes ; i++, texCoords++, colors++ ) {
			int		clip = 0;
			vec_t dist0, dist1, dist2;
			
			dist0 = origin0 - tess.vertexPtr2[i].xyz[0];
			dist1 = origin1 - tess.vertexPtr2[i].xyz[1];
			dist2 = origin2 - tess.vertexPtr2[i].xyz[2];

			backEnd.pc.c_dlightVertexes++;

			texCoords0 = 0.5f + dist0 * scale;
			texCoords1 = 0.5f + dist1 * scale;

			ptr = ptrPlusOffset(tess.normalPtr, i * tess.normalInc);
			if( !r_dlightBacks->integer &&
			    // dist . tess.normal[i]
			    ( dist0 * tess.vertexPtr3[i].normal[0] +
			      dist1 * tess.vertexPtr3[i].normal[1] +
			      dist2 * tess.vertexPtr3[i].normal[2] ) < 0.0f ) {
				clip = 63;
			} else {
				if ( texCoords0 < 0.0f ) {
					clip |= 1;
				} else if ( texCoords0 > 1.0f ) {
					clip |= 2;
				}
				if ( texCoords1 < 0.0f ) {
					clip |= 4;
				} else if ( texCoords1 > 1.0f ) {
					clip |= 8;
				}
				(*texCoords)[0] = texCoords0;
				(*texCoords)[1] = texCoords1;

				// modulate the strength based on the height and color
				if ( dist2 > radius ) {
					clip |= 16;
					modulate = 0.0f;
				} else if ( dist2 < -radius ) {
					clip |= 32;
					modulate = 0.0f;
				} else {
					dist2 = Q_fabs(dist2);
					if ( dist2 < radius * 0.5f ) {
						modulate = 1.0f;
					} else {
						modulate = 2.0f * (radius - dist2) * scale;
					}
				}
			}
			clipBits[i] = clip;

			modulateVec = vec_ld(0,(float *)&modulate);
			modulateVec = vec_perm(modulateVec,modulateVec,modulatePerm);
			colorVec = vec_madd(floatColorVec0,modulateVec,zero);
			colorInt = vec_cts(colorVec,0);	// RGBx
			colorShort = vec_pack(colorInt,colorInt);		// RGBxRGBx
			colorChar = vec_packsu(colorShort,colorShort);	// RGBxRGBxRGBxRGBx
			colorChar = vec_sel(colorChar,vSel,vSel);		// RGBARGBARGBARGBA replace alpha with 255
			vec_ste((vector unsigned int)colorChar,0,(unsigned int *)colors);	// store color
		}

		// build a list of triangles that need light
		numIndexes = 0;
		for ( i = 0 ; i < tess.numIndexes ; i += 3 ) {
			GLushort	a, b, c;

			a = tess.indexPtr.p16[i];
			b = tess.indexPtr.p16[i+1];
			c = tess.indexPtr.p16[i+2];
			if ( clipBits[a] & clipBits[b] & clipBits[c] ) {
				continue;	// not lighted
			}
			hitIndexes[numIndexes] = a;
			hitIndexes[numIndexes+1] = b;
			hitIndexes[numIndexes+2] = c;
			numIndexes += 3;
		}

		if ( !numIndexes ) {
			continue;
		}

		GL_VBO( 0, 0 );
		GL_Program( 0 );
		
		GL_TexCoordPtr( 0, 2, GL_FLOAT, 0, texCoordsArray );
		GL_ColorPtr( colorArray, sizeof(color4ub_t) );

		GL_BindImages( 1, &tr.dlightImage );
		// include GLS_DEPTHFUNC_EQUAL so alpha tested surfaces don't add light
		// where they aren't rendered
		if ( dl->additive ) {
			GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
		}
		else {
			GL_State( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
		}
		R_DrawElements( numIndexes, hitIndexes, 0, tess.numVertexes-1, tess.numVertexes-1 );
		backEnd.pc.c_totalIndexes += numIndexes;
		backEnd.pc.c_dlightIndexes += numIndexes;
	}

	ri.Hunk_FreeTempMemory( hitIndexes );
	ri.Hunk_FreeTempMemory( colorArray );
	ri.Hunk_FreeTempMemory( texCoordsArray );
	ri.Hunk_FreeTempMemory( clipBits );
}
#endif


static void ProjectDlightTexture_scalar( void ) {
	int		i, l;
	vec3_t	origin;
	vec2_t	*texCoords;
	color4ub_t	*colors;
	byte	*clipBits;
	vec2_t	*texCoordsArray;
	color4ub_t	*colorArray;
	GLushort	*hitIndexes;
	int		numIndexes;
	float	scale;
	float	radius;
	vec3_t	floatColor;
	float	modulate = 0.0f;
	int     state;

	if ( !backEnd.refdef.num_dlights ||
	     tess.numVertexes > 65535 ) { // to avoid GLushort overflow
		return;
	}

	clipBits = ri.Hunk_AllocateTempMemory( sizeof(byte) * tess.numVertexes );
	texCoordsArray = ri.Hunk_AllocateTempMemory( sizeof(vec2_t) * tess.numVertexes );
	colorArray = ri.Hunk_AllocateTempMemory( sizeof(color4ub_t) * tess.numVertexes );
	hitIndexes = ri.Hunk_AllocateTempMemory( sizeof(GLushort) * tess.numIndexes );

	for ( l = 0 ; l < backEnd.refdef.num_dlights ; l++ ) {
		dlight_t	*dl;

		if ( !( tess.dlightBits & ( 1 << l ) ) ) {
			continue;	// this surface definately doesn't have any of this light
		}
		texCoords = texCoordsArray;
		colors = colorArray;

		dl = &backEnd.refdef.dlights[l];
		VectorCopy( dl->transformed, origin );
		radius = dl->radius;
		scale = 1.0f / radius;

		if(r_greyscale->integer)
		{
			float luminance;
			
			luminance = (dl->color[0] * 255.0f + dl->color[1] * 255.0f + dl->color[2] * 255.0f) / 3;
			floatColor[0] = floatColor[1] = floatColor[2] = luminance;
		}
		else
		{
			floatColor[0] = dl->color[0] * 255.0f;
			floatColor[1] = dl->color[1] * 255.0f;
			floatColor[2] = dl->color[2] * 255.0f;
		}

		for ( i = 0 ; i < tess.numVertexes ; i++, texCoords++, colors++ ) {
			int		clip = 0;
			vec3_t	dist;
			
			VectorSubtract( origin, tess.vertexPtr2[i].xyz, dist );

			backEnd.pc.c_dlightVertexes++;

			(*texCoords)[0] = 0.5f + dist[0] * scale;
			(*texCoords)[1] = 0.5f + dist[1] * scale;

			if( !r_dlightBacks->integer &&
			    // dist . tess.normal[i]
			    ( dist[0] * tess.vertexPtr3[i].normal[0] +
			      dist[1] * tess.vertexPtr3[i].normal[1] +
			      dist[2] * tess.vertexPtr3[i].normal[2] ) < 0.0f ) {
				clip = 63;
			} else {
				if ( (*texCoords)[0] < 0.0f ) {
					clip |= 1;
				} else if ( (*texCoords)[0] > 1.0f ) {
					clip |= 2;
				}
				if ( (*texCoords)[1] < 0.0f ) {
					clip |= 4;
				} else if ( (*texCoords)[1] > 1.0f ) {
					clip |= 8;
				}

				// modulate the strength based on the height and color
				if ( dist[2] > radius ) {
					clip |= 16;
					modulate = 0.0f;
				} else if ( dist[2] < -radius ) {
					clip |= 32;
					modulate = 0.0f;
				} else {
					dist[2] = Q_fabs(dist[2]);
					if ( dist[2] < radius * 0.5f ) {
						modulate = 1.0f;
					} else {
						modulate = 2.0f * (radius - dist[2]) * scale;
					}
				}
			}
			clipBits[i] = clip;
			(*colors)[0] = myftol(floatColor[0] * modulate);
			(*colors)[1] = myftol(floatColor[1] * modulate);
			(*colors)[2] = myftol(floatColor[2] * modulate);
			(*colors)[3] = 255;
		}

		// build a list of triangles that need light
		numIndexes = 0;
		for ( i = 0 ; i < tess.numIndexes ; i += 3 ) {
			GLushort		a, b, c;

			a = tess.indexPtr.p16[i];
			b = tess.indexPtr.p16[i+1];
			c = tess.indexPtr.p16[i+2];
			if ( clipBits[a] & clipBits[b] & clipBits[c] ) {
				continue;	// not lighted
			}
			hitIndexes[numIndexes] = a;
			hitIndexes[numIndexes+1] = b;
			hitIndexes[numIndexes+2] = c;
			numIndexes += 3;
		}

		if ( !numIndexes ) {
			continue;
		}
		
		GL_Program( tr.defaultShader->GLSLprogram );
		GL_VBO( 0, 0 );
		
		GL_TexCoordPtr( 0, 2, GL_FLOAT, 0, texCoordsArray[0] );
		GL_ColorPtr( colorArray, sizeof(color4ub_t) );

		GL_BindImages( 1, &tr.dlightImage );
		state = 0;
		if ( tess.xstages[0] )
			state |= tess.xstages[0]->stateBits & GLS_POLYGON_OFFSET;
		// include GLS_DEPTHFUNC_EQUAL so alpha tested surfaces don't add light
		// where they aren't rendered
		if ( dl->additive ) {
			state |= GLS_DEPTHFUNC_EQUAL | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		} else {
			state |= GLS_DEPTHFUNC_EQUAL | GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE;
		}
		GL_State( state );

		R_DrawElements( numIndexes, hitIndexes, 0, tess.numVertexes-1, tess.numVertexes-1 );
		backEnd.pc.c_totalIndexes += numIndexes;
		backEnd.pc.c_dlightIndexes += numIndexes;
	}

	ri.Hunk_FreeTempMemory( hitIndexes );
	ri.Hunk_FreeTempMemory( colorArray );
	ri.Hunk_FreeTempMemory( texCoordsArray );
	ri.Hunk_FreeTempMemory( clipBits );
}

static void ProjectDlightTexture( void ) {
#if idppc_altivec
	if (com_altivec->integer) {
		// must be in a seperate function or G3 systems will crash.
		ProjectDlightTexture_altivec();
		return;
	}
#endif
	ProjectDlightTexture_scalar();
}


/*
===================
RB_FogPass

Blends a fog texture on top of everything else
===================
*/
static void RB_FogPass( void ) {
	fog_t		*fog;
	int		state = GLS_DEFAULT;

	tess.svars.texcoords[0] = ri.Hunk_AllocateTempMemory( tess.numVertexes * sizeof(vec2_t) );

	if ( tess.xstages[0] )
		state |= tess.xstages[0]->stateBits & GLS_POLYGON_OFFSET;
	if ( tess.shader->fogPass == FP_EQUAL )
		state |= GLS_DEPTHFUNC_EQUAL;

	if ( tr.fogShader->GLSLprogram ) {
		GL_Program( tr.fogShader->GLSLprogram );
		state |= GLS_SRCBLEND_ONE | GLS_DSTBLEND_SRC_ALPHA;
	} else {
		GL_Program( 0 );
		state |= GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	}
	GL_State( state );

	fog = tr.world->fogs + tess.fogNum;

	RB_CalcFogTexCoords( tess.svars.texcoords[0], tess.numVertexes );

	GL_TexCoordPtr( 0, 2, GL_FLOAT, 0, tess.svars.texcoords[0][0] );
	GL_Color4f( fog->parms.color[0], fog->parms.color[1],
		    fog->parms.color[2], 1.0f );

	GL_BindImages( 1, &tr.fogImage );

	R_DrawElements( tess.numIndexes, tess.indexPtr.p16, 0, tess.numVertexes-1, tess.numVertexes-1 );

	ri.Hunk_FreeTempMemory( tess.svars.texcoords[0] );
}

/*
===============
ComputeColors
===============
*/
static void ComputeColors( shaderStage_t *pStage, qboolean skipFog )
{
	int		i;
	alphaGen_t	aGen = pStage->alphaGen;
	colorGen_t	rgbGen = pStage->rgbGen;
	color4ub_t	color;
	fog_t		*fog;
	qboolean	constRGB, constA;
	
	// get rid of AGEN_SKIP
	if ( aGen == AGEN_SKIP ) {
		if ( rgbGen == CGEN_EXACT_VERTEX ||
		     rgbGen == CGEN_VERTEX ) {
			aGen = AGEN_VERTEX;
		} else {
			aGen = AGEN_IDENTITY;
		}
	}
	
	// no need to multiply by 1
	if ( tr.identityLight == 1 && rgbGen == CGEN_VERTEX ) {
		rgbGen = CGEN_EXACT_VERTEX;
	}

	// check for constant RGB
	switch( rgbGen ) {
	case CGEN_IDENTITY_LIGHTING:
		color[0] = color[1] = color[2] = tr.identityLightByte;
		constRGB = qtrue;
		break;
	case CGEN_IDENTITY:
		color[0] = color[1] = color[2] = 255;
		constRGB = qtrue;
		break;
	case CGEN_ENTITY:
		RB_CalcColorFromEntity( &color, 1 );
		constRGB = qtrue;
		break;
	case CGEN_ONE_MINUS_ENTITY:
		RB_CalcColorFromOneMinusEntity( &color, 1 );
		constRGB = qtrue;
		break;
	case CGEN_WAVEFORM:
		RB_CalcWaveColor( &pStage->rgbWave, &color, 1 );
		constRGB = qtrue;
		break;
	case CGEN_FOG:
		fog = tr.world->fogs + tess.fogNum;
		*(int *)(&color) = fog->colorInt;
		constRGB = qtrue;
		break;
	case CGEN_CONST:
		*(int *)(&color) = *(int *)pStage->constantColor;		
		constRGB = qtrue;
		break;
	default:
		constRGB = qfalse;
		break;
	}

	// check for constant ALPHA
	switch( aGen ) {
	case AGEN_IDENTITY:
		color[3] = 255;
		constA = qtrue;
		break;
	case AGEN_ENTITY:
		RB_CalcAlphaFromEntity( &color, 1 );
		constA = qtrue;
		break;
	case AGEN_ONE_MINUS_ENTITY:
		RB_CalcAlphaFromOneMinusEntity( &color, 1 );
		constA = qtrue;
		break;
	case AGEN_WAVEFORM:
		RB_CalcWaveAlpha( &pStage->alphaWave, &color, 1 );
		constA = qtrue;
		break;
	case AGEN_CONST:
		color[3] = pStage->constantColor[3];
		constA = qtrue;
		break;
	default:
		constA = qfalse;
		break;
	}

	if ( !r_greyscale->integer &&
	     (skipFog || !tess.fogNum || pStage->adjustColorsForFog == ACFF_NONE) ) {
		// if RGB and ALPHA are constant, just set the GL color
		if ( constRGB && constA ) {
			GL_Color4ub( color[0], color[1], color[2], color[3] );
			tess.svars.colors = COLOR_BIND_NONE;
			return;
		}
		
		// if RGB and ALPHA are identical to vertex data, bind that
		if ( aGen == AGEN_VERTEX && rgbGen == CGEN_EXACT_VERTEX ) {
			tess.svars.colors = COLOR_BIND_VERT;
			return;
		}
	}
	
	// we have to allocate a per-Vertex color value
	tess.svars.colors = ri.Hunk_AllocateTempMemory( tess.numVertexes * sizeof(color4ub_t) );

	//
	// rgbGen
	//
	switch ( rgbGen )
	{
	case CGEN_BAD:
	case CGEN_IDENTITY_LIGHTING:
	case CGEN_IDENTITY:
	case CGEN_ENTITY:
	case CGEN_ONE_MINUS_ENTITY:
	case CGEN_WAVEFORM:
	case CGEN_FOG:
	case CGEN_CONST:
		for ( i = 0; i < tess.numVertexes; i++ ) {
			*(int *)tess.svars.colors[i] = *(int *)(&color);
		}
		break;
	case CGEN_LIGHTING_DIFFUSE:
		RB_CalcDiffuseColor( tess.svars.colors, tess.numVertexes );
		break;
	case CGEN_EXACT_VERTEX:
		for ( i = 0; i < tess.numVertexes; i++ )
		{
			tess.svars.colors[i][0] = tess.vertexPtr3[i].color[0];
			tess.svars.colors[i][1] = tess.vertexPtr3[i].color[1];
			tess.svars.colors[i][2] = tess.vertexPtr3[i].color[2];
			tess.svars.colors[i][3] = tess.vertexPtr3[i].color[3];
		}
		break;
	case CGEN_VERTEX:
		for ( i = 0; i < tess.numVertexes; i++ )
		{
			tess.svars.colors[i][0] = tess.vertexPtr3[i].color[0] * tr.identityLight;
			tess.svars.colors[i][1] = tess.vertexPtr3[i].color[1] * tr.identityLight;
			tess.svars.colors[i][2] = tess.vertexPtr3[i].color[2] * tr.identityLight;
			tess.svars.colors[i][3] = tess.vertexPtr3[i].color[3];
		}
		break;
	case CGEN_ONE_MINUS_VERTEX:
		if ( tr.identityLight == 1 )
		{
			for ( i = 0; i < tess.numVertexes; i++ )
			{
				tess.svars.colors[i][0] = 255 - tess.vertexPtr3[i].color[0];
				tess.svars.colors[i][1] = 255 - tess.vertexPtr3[i].color[1];
				tess.svars.colors[i][2] = 255 - tess.vertexPtr3[i].color[2];
			}
		}
		else
		{
			for ( i = 0; i < tess.numVertexes; i++ )
			{
				tess.svars.colors[i][0] = ( 255 - tess.vertexPtr3[i].color[0] ) * tr.identityLight;
				tess.svars.colors[i][1] = ( 255 - tess.vertexPtr3[i].color[1] ) * tr.identityLight;
				tess.svars.colors[i][2] = ( 255 - tess.vertexPtr3[i].color[2] ) * tr.identityLight;
			}
		}
		break;
	}

	//
	// alphaGen
	//
	switch ( aGen )
	{
	case AGEN_SKIP:
	case AGEN_IDENTITY:
	case AGEN_ENTITY:
	case AGEN_ONE_MINUS_ENTITY:
	case AGEN_WAVEFORM:
	case AGEN_CONST:
		for ( i = 0; i < tess.numVertexes; i++ ) {
			tess.svars.colors[i][3] = color[3];
		}
		break;
	case AGEN_LIGHTING_SPECULAR:
		RB_CalcSpecularAlpha( tess.svars.colors, tess.numVertexes );
		break;
	case AGEN_VERTEX:
		if ( pStage->rgbGen != CGEN_VERTEX ) {
			for ( i = 0; i < tess.numVertexes; i++ ) {
				tess.svars.colors[i][3] = tess.vertexPtr3[i].color[3];
			}
		}
		break;
	case AGEN_ONE_MINUS_VERTEX:
		for ( i = 0; i < tess.numVertexes; i++ )
		{
			tess.svars.colors[i][3] = 255 - tess.vertexPtr3[i].color[3];
		}
		break;
	case AGEN_PORTAL:
		{
			unsigned char alpha;

			for ( i = 0; i < tess.numVertexes; i++ )
			{
				float len;
				vec3_t v;

				VectorSubtract( tess.vertexPtr2[i].xyz, backEnd.viewParms.or.origin, v );
				len = VectorLength( v );

				len /= tess.shader->portalRange;

				if ( len < 0 )
				{
					alpha = 0;
				}
				else if ( len > 1 )
				{
					alpha = 0xff;
				}
				else
				{
					alpha = len * 0xff;
				}

				tess.svars.colors[i][3] = alpha;
			}
		}
		break;
	}

	//
	// fog adjustment for colors to fade out as fog increases
	//
	if ( tess.fogNum )
	{
		switch ( pStage->adjustColorsForFog )
		{
		case ACFF_MODULATE_RGB:
			RB_CalcModulateColorsByFog( tess.svars.colors, tess.numVertexes );
			break;
		case ACFF_MODULATE_ALPHA:
			RB_CalcModulateAlphasByFog( tess.svars.colors, tess.numVertexes );
			break;
		case ACFF_MODULATE_RGBA:
			RB_CalcModulateRGBAsByFog( tess.svars.colors, tess.numVertexes );
			break;
		case ACFF_NONE:
			break;
		}
	}
	
	// if in greyscale rendering mode turn all color values into greyscale.
	if(r_greyscale->integer)
	{
		int scale;
		
		for(i = 0; i < tess.numVertexes; i++)
		{
			scale = (tess.svars.colors[i][0] + tess.svars.colors[i][1] + tess.svars.colors[i][2]) / 3;
			tess.svars.colors[i][0] = tess.svars.colors[i][1] = tess.svars.colors[i][2] = scale;
		}
	}
}

/*
===============
ComputeTexCoords
===============
*/
static void ComputeTexCoords( shaderStage_t *pStage ) {
	int		i;
	int		b;
	qboolean	noTexMods;
	
	for ( b = 0; b < NUM_TEXTURE_BUNDLES; b++ ) {
		int tm;

		noTexMods = (pStage->bundle[b].numTexMods == 0) ||
			(pStage->bundle[b].texMods[0].type == TMOD_NONE);
		
		if ( noTexMods && pStage->bundle[b].tcGen == TCGEN_BAD ) {
			tess.svars.texcoords[b] = TC_BIND_NONE;
			continue;
		}

		if ( noTexMods && pStage->bundle[b].tcGen == TCGEN_TEXTURE ) {
			tess.svars.texcoords[b] = TC_BIND_TEXT;
			continue;
		}

		if ( noTexMods && pStage->bundle[b].tcGen == TCGEN_LIGHTMAP ) {
			tess.svars.texcoords[b] = TC_BIND_LMAP;
			continue;
		}

		tess.svars.texcoords[b] = ri.Hunk_AllocateTempMemory( tess.numVertexes * sizeof(vec2_t) );
		//
		// generate the texture coordinates
		//
		switch ( pStage->bundle[b].tcGen )
		{
		case TCGEN_IDENTITY:
			Com_Memset( tess.svars.texcoords[b], 0, sizeof( vec2_t ) * tess.numVertexes );
			break;
		case TCGEN_TEXTURE:
			for ( i = 0 ; i < tess.numVertexes ; i++ ) {
				tess.svars.texcoords[b][i][0] = tess.vertexPtr1[i].tc1[0];
				tess.svars.texcoords[b][i][1] = tess.vertexPtr1[i].tc1[1];
			}
			break;
		case TCGEN_LIGHTMAP:
			for ( i = 0 ; i < tess.numVertexes ; i++ ) {
				tess.svars.texcoords[b][i][0] = tess.vertexPtr1[i].tc2[0];
				tess.svars.texcoords[b][i][1] = tess.vertexPtr1[i].tc2[1];
			}
			break;
		case TCGEN_VECTOR:
			for ( i = 0 ; i < tess.numVertexes ; i++ ) {
				tess.svars.texcoords[b][i][0] = DotProduct( tess.vertexPtr2[i].xyz, pStage->bundle[b].tcGenVectors[0] );
				tess.svars.texcoords[b][i][1] = DotProduct( tess.vertexPtr2[i].xyz, pStage->bundle[b].tcGenVectors[1] );
			}
			break;
		case TCGEN_FOG:
			RB_CalcFogTexCoords( tess.svars.texcoords[b], tess.numVertexes );
			break;
		case TCGEN_ENVIRONMENT_MAPPED:
			RB_CalcEnvironmentTexCoords( tess.svars.texcoords[b], tess.numVertexes );
			break;
		case TCGEN_BAD:
			return;
		}

		//
		// alter texture coordinates
		//
		for ( tm = 0; tm < pStage->bundle[b].numTexMods ; tm++ ) {
			switch ( pStage->bundle[b].texMods[tm].type )
			{
			case TMOD_NONE:
				tm = TR_MAX_TEXMODS;		// break out of for loop
				break;

			case TMOD_TURBULENT:
				RB_CalcTurbulentTexCoords( &pStage->bundle[b].texMods[tm].wave, 
							   tess.svars.texcoords[b], tess.numVertexes );
				break;

			case TMOD_ENTITY_TRANSLATE:
				RB_CalcScrollTexCoords( backEnd.currentEntity->e.shaderTexCoord,
							tess.svars.texcoords[b], tess.numVertexes );
				break;

			case TMOD_SCROLL:
				RB_CalcScrollTexCoords( pStage->bundle[b].texMods[tm].scroll,
							tess.svars.texcoords[b], tess.numVertexes );
				break;

			case TMOD_SCALE:
				RB_CalcScaleTexCoords( pStage->bundle[b].texMods[tm].scale,
						       tess.svars.texcoords[b], tess.numVertexes );
				break;
			
			case TMOD_STRETCH:
				RB_CalcStretchTexCoords( &pStage->bundle[b].texMods[tm].wave, 
							 tess.svars.texcoords[b], tess.numVertexes );
				break;

			case TMOD_TRANSFORM:
				RB_CalcTransformTexCoords( &pStage->bundle[b].texMods[tm],
							   tess.svars.texcoords[b], tess.numVertexes );
				break;

			case TMOD_ROTATE:
				RB_CalcRotateTexCoords( pStage->bundle[b].texMods[tm].rotateSpeed,
							tess.svars.texcoords[b], tess.numVertexes );
				break;

			default:
				ri.Error( ERR_DROP, "ERROR: unknown texmod '%d' in shader '%s'\n", pStage->bundle[b].texMods[tm].type, tess.shader->name );
				break;
			}
		}
	}
}

/*
** RB_IterateStagesGeneric
*/
static void RB_IterateStagesGenericVBO( vboInfo_t *VBO, GLuint max )
{
	int stage;
	int bundle;
	image_t *images[NUM_TEXTURE_BUNDLES];
	GLint    glQueryID = 0;
	GLuint   glQueryResult;

	if( qglBeginQueryARB &&
	    !backEnd.viewParms.isPortal ) {
		if ( backEnd.currentEntity == &tr.worldEntity ) {
			glQueryID = tess.shader->QueryID;
			glQueryResult = tess.shader->QueryResult;
		} else if ( tess.shader == tr.occlusionTestShader ) {
			glQueryID = backEnd.currentEntity->GLQueryID;
			glQueryResult = backEnd.currentEntity->GLQueryResult;
		}
	}
	
	for ( stage = 0; stage < MAX_SHADER_STAGES; stage++ )
	{
		shaderStage_t *pStage = tess.xstages[stage];

		if ( !pStage )
		{
			break;
		}

		// if the surface was invisible in the last frame, skip
		// all detail stages
		if ( glQueryID && glQueryResult == 0 && pStage->isDetail )
			continue;

		GL_State( pStage->stateBits );

		ComputeColors( pStage, qtrue );
		
		if ( tess.svars.colors == COLOR_BIND_NONE) {
			// constant color is already set
		} else if ( tess.svars.colors == COLOR_BIND_VERT ) {
			GL_ColorPtr( &VBO->offs3->color, sizeof(vaWord3_t) );
		} else {
			// per vertex color should not happen with VBO
			ri.Printf( PRINT_WARNING, "ERROR: per vertex color in VBO shader '%s'\n", tess.shader->name );
			ri.Hunk_FreeTempMemory( tess.svars.colors );
			return;
		}

		ComputeTexCoords( pStage );
		if ( tess.svars.texcoords[0] == TC_BIND_TEXT ) {
			GL_TexCoordPtr( 0, 2, GL_FLOAT, sizeof(vaWord1_t),
					&VBO->offs1->tc1[0] );
		} else if ( tess.svars.texcoords[0] == TC_BIND_LMAP ) {
			GL_TexCoordPtr( 0, 2, GL_FLOAT, sizeof(vaWord1_t),
					&VBO->offs1->tc2[0] );
		} else {
			// per vertex texcoords should not happen with VBO
			ri.Printf( PRINT_WARNING, "ERROR: per vertex texture coords in VBO shader '%s'\n", tess.shader->name );
			ri.Hunk_FreeTempMemory( tess.svars.texcoords[0] );
			return;
		}

		//
		// set state
		//
		if ( pStage->bundle[0].vertexLightmap && ( (r_vertexLight->integer && !r_uiFullScreen->integer) || glConfig.hardwareType == GLHW_PERMEDIA2 ) && r_lightmap->integer )
		{
			images[0] = tr.whiteImage;
		}
		else 
			R_GetAnimatedImage( &pStage->bundle[0], qfalse, &images[0] );
		
		//
		// do multitexture
		//
		for ( bundle = 1; bundle < glConfig.numTextureUnits; bundle++ ) {
			if ( !pStage->bundle[bundle].multitextureEnv )
				break;

			// this is an ugly hack to work around a GeForce driver
			// bug with multitexture and clip planes
			if ( backEnd.viewParms.isPortal ) {
				qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
			}
			
			//
			// lightmap/secondary pass
			//
			if ( tess.svars.texcoords[bundle] == TC_BIND_TEXT ) {
				GL_TexCoordPtr( bundle, 2, GL_FLOAT, sizeof(vaWord1_t),
						&VBO->offs1->tc1[0] );
			} else if ( tess.svars.texcoords[bundle] == TC_BIND_LMAP ) {
				GL_TexCoordPtr( bundle, 2, GL_FLOAT, sizeof(vaWord1_t),
						&VBO->offs1->tc2[0] );
			} else {
				// per vertex texcoords should not happen with VBO
				ri.Error( ERR_DROP, "ERROR: per vertex texture coords in VBO shader '%s'\n", tess.shader->name );			
			}
			
			if ( r_lightmap->integer ) {
				GL_TexEnv( bundle, GL_REPLACE );
			} else {
				GL_TexEnv( bundle, pStage->bundle[bundle].multitextureEnv );
			}
			R_GetAnimatedImage( &pStage->bundle[bundle], qfalse, &images[bundle] );
		}
		
		GL_BindImages( bundle, images );
		
		if ( stage == 0 && glQueryID ) {
			qglBeginQueryARB( GL_SAMPLES_PASSED_ARB, glQueryID );
		}
		
		if ( qglDrawRangeElementsEXT )
			qglDrawRangeElementsEXT( GL_TRIANGLES,
						 VBO->minIndex, VBO->maxIndex,
						 VBO->numIndexes,
						 max > 65535 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT,
						 NULL );
		else
			qglDrawElements( GL_TRIANGLES, VBO->numIndexes,
					 max > 65535 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, NULL );
		
		if ( stage == 0 && glQueryID ) {
			qglEndQueryARB( GL_SAMPLES_PASSED_ARB );
		}

		// allow skipping out to show just lightmaps during development
		if ( r_lightmap->integer && ( pStage->bundle[0].isLightmap || pStage->bundle[1].isLightmap || pStage->bundle[0].vertexLightmap ) )
		{
			break;
		}
	}
}
static void RB_IterateStagesGeneric( shaderCommands_t *input, GLuint max )
{
	int stage;
	image_t *image;
	GLint    glQueryID = 0;
	GLuint   glQueryResult = 0;

	if( qglBeginQueryARB &&
	    !backEnd.viewParms.isPortal ) {
		if( backEnd.currentEntity == &tr.worldEntity ) {
			glQueryID = tess.shader->QueryID;
			glQueryResult = tess.shader->QueryResult;
		} else if ( tess.shader == tr.occlusionTestShader ) {
			glQueryID = backEnd.currentEntity->GLQueryID;
			glQueryResult = backEnd.currentEntity->GLQueryResult;
		}
	}

	for ( stage = 0; stage < MAX_SHADER_STAGES; stage++ )
	{
		shaderStage_t *pStage = tess.xstages[stage];

		if ( !pStage )
		{
			break;
		}

		// if the surface was invisible in the last frame, skip
		// all detail stages
		if ( glQueryID && glQueryResult == 0 && pStage->isDetail )
			continue;

		ComputeColors( pStage, (tess.vertexPtr1 == NULL) );
		ComputeTexCoords( pStage );

		if ( input->svars.colors == COLOR_BIND_NONE ) {
		} else if (input->svars.colors == COLOR_BIND_VERT ) {
			GL_ColorPtr( &input->vertexPtr3[0].color, sizeof(vaWord3_t) );
		} else {
			GL_ColorPtr( input->svars.colors, sizeof(color4ub_t) );
		}

		if ( stage == 0 && glQueryID ) {
			qglBeginQueryARB( GL_SAMPLES_PASSED_ARB, glQueryID );
		}
		
		//
		// do multitexture
		//
		if ( pStage->bundle[1].image[0] != 0 )
		{
			DrawMultitextured( input, stage, max );
		}
		else
		{
			if ( input->svars.texcoords[0] == TC_BIND_NONE ) {
				GL_TexCoordPtr( 0, 2, GL_FLOAT, sizeof(vaWord1_t),
						NULL );
			} else if ( input->svars.texcoords[0] == TC_BIND_TEXT ) {
				GL_TexCoordPtr( 0, 2, GL_FLOAT, sizeof(vaWord1_t),
						&input->vertexPtr1[0].tc1[0] );
			} else if ( input->svars.texcoords[0] == TC_BIND_LMAP ) {
				GL_TexCoordPtr( 0, 2, GL_FLOAT, sizeof(vaWord1_t),
						&input->vertexPtr1[0].tc2[0] );
			} else {
				GL_TexCoordPtr( 0, 2, GL_FLOAT, 0,
						input->svars.texcoords[0][0] );
			}

			//
			// set state
			//
			if ( pStage->bundle[0].vertexLightmap && ( (r_vertexLight->integer && !r_uiFullScreen->integer) || glConfig.hardwareType == GLHW_PERMEDIA2 ) && r_lightmap->integer )
			{
				image = tr.whiteImage;
			}
			else 
				R_GetAnimatedImage( &pStage->bundle[0], qfalse, &image );
			
			GL_BindImages( 1, &image );
			GL_State( pStage->stateBits );

			//
			// draw
			//
			R_DrawElements( input->numIndexes, input->indexPtr.p16,
					input->minIndex, input->maxIndex, max );
		}
		if ( stage == 0 && glQueryID ) {
			qglEndQueryARB( GL_SAMPLES_PASSED_ARB );
		}

		if ( input->svars.texcoords[0] != TC_BIND_NONE &&
		     input->svars.texcoords[0] != TC_BIND_TEXT &&
		     input->svars.texcoords[0] != TC_BIND_LMAP ) {
			ri.Hunk_FreeTempMemory( input->svars.texcoords[0] );
		}
		if ( input->svars.colors == COLOR_BIND_NONE ) {
		} else if (input->svars.colors == COLOR_BIND_VERT ) {
		} else {
			ri.Hunk_FreeTempMemory( tess.svars.colors );
		}

		// allow skipping out to show just lightmaps during development
		if ( r_lightmap->integer && ( pStage->bundle[0].isLightmap || pStage->bundle[1].isLightmap || pStage->bundle[0].vertexLightmap ) )
		{
			break;
		}
	}
}


/*
** RB_StageIteratorGeneric
*/
void RB_StageIteratorGeneric( void )
{
	shaderCommands_t *input;
	GLuint            max;
	vboInfo_t        *VBO;

	input = &tess;

	RB_DeformTessGeometry();

	//
	// log this call
	//
	if ( r_logFile->integer ) 
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment( va("--- RB_StageIteratorGeneric( %s ) ---\n", tess.shader->name) );
	}

	//
	// set face culling appropriately
	//
	GL_Cull( input->shader->cullType );
	GL_Program( 0 );

	for( VBO = tess.firstVBO; VBO; VBO = VBO->next ) {
		vec3_t	*vertexes;
		
		if ( VBO->vbo ) {
			GL_VBO( VBO->vbo, VBO->ibo );
			max = VBO->maxIndex;
			vertexes = &VBO->offs2->xyz;
		} else {
			GL_VBO( backEnd.worldVBO.vbo, VBO->ibo );
			max = backEnd.worldVBO.maxIndex;
			vertexes = &backEnd.worldVBO.offs2->xyz;
		}

		qglVertexPointer (3, GL_FLOAT, sizeof(vaWord2_t),
				  vertexes);

		RB_IterateStagesGenericVBO( VBO, max );

		if ( r_showtris->integer && !input->shader->isDepth ) {
			DrawTris( VBO->numIndexes, NULL,
				  VBO->minIndex,
				  VBO->maxIndex, max,
				  1.0f, 1.0f, 0.0f );
		}
	}

	if ( tess.numIndexes > 0 ) {
		vec3_t	*vertexes;

		if ( input->vertexPtr1 ) {
			GL_VBO( 0, 0 );
			max = tess.numIndexes-1;
			vertexes = &input->vertexPtr2[0].xyz;
		} else {
			GL_VBO( backEnd.worldVBO.vbo, 0 );
			max = backEnd.worldVBO.maxIndex;
			vertexes = &backEnd.worldVBO.offs2->xyz;
		}
		//
		// lock XYZ
		//
		qglVertexPointer (3, GL_FLOAT, sizeof(vaWord2_t),
				  vertexes);
		if (qglLockArraysEXT)
		{
			qglLockArraysEXT(0, input->numVertexes);
			GLimp_LogComment( "glLockArraysEXT\n" );
		}
		
		//
		// call shader function
		//
		if ( !tess.firstVBO )
			RB_IterateStagesGeneric( input, max );
		
		if ( r_showtris->integer && !input->shader->isDepth ) {
			DrawTris( tess.numIndexes, tess.indexPtr.p16,
				  tess.minIndex, tess.maxIndex, max,
				  1.0f, 0.0f, 0.0f );
		}

		// 
		// unlock arrays
		//
		if (qglUnlockArraysEXT)
		{
			qglUnlockArraysEXT();
			GLimp_LogComment( "glUnlockArraysEXT\n" );
		}
	}
}


/*
** RB_StageIteratorVertexLitTexture
*/
void RB_StageIteratorVertexLitTexture( void )
{
	shaderCommands_t *input;
	shader_t		*shader;
	image_t		*image;
	GLuint            max;
	vboInfo_t        *VBO;

	input = &tess;

	shader = input->shader;

	//
	// compute colors
	//
	RB_CalcDiffuseColor( tess.svars.colors, tess.numVertexes );

	//
	// log this call
	//
	if ( r_logFile->integer ) 
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment( va("--- RB_StageIteratorVertexLitTexturedUnfogged( %s ) ---\n", tess.shader->name) );
	}

	//
	// set face culling appropriately
	//
	GL_Cull( input->shader->cullType );
	GL_Program( 0 );

	R_GetAnimatedImage( &tess.xstages[0]->bundle[0], qfalse, &image );
	GL_BindImages( 1, &image );
	GL_State( tess.xstages[0]->stateBits );

	//
	// set arrays and lock
	//
	for( VBO = input->firstVBO; VBO; VBO = VBO->next ) {
		vec3_t		*vertexes;
		color4ub_t	*colors;
		vec2_t		*tcs;
		
		if ( VBO->vbo ) {
			GL_VBO( VBO->vbo, VBO->ibo );
			max = VBO->maxIndex;
			vertexes = &VBO->offs2->xyz;
			colors = &VBO->offs3->color;
			tcs = &VBO->offs1->tc1;
		} else {
			GL_VBO( backEnd.worldVBO.vbo, VBO->ibo );
			max = backEnd.worldVBO.maxIndex;
			vertexes = &backEnd.worldVBO.offs2->xyz;
			colors = &backEnd.worldVBO.offs3->color;
			tcs = &backEnd.worldVBO.offs1->tc1;
		}

		GL_ColorPtr( colors, sizeof(vboWord3_t) );
		GL_TexCoordPtr( 0, 2, GL_FLOAT, sizeof(vaWord1_t), tcs[0] );
		qglVertexPointer (3, GL_FLOAT, sizeof(vaWord2_t),
				  vertexes );
		
		//
		// call special shade routine
		//
		R_DrawElements( VBO->numIndexes, NULL, 
				VBO->minIndex, VBO->maxIndex, max );
		
		if ( r_showtris->integer && !input->shader->isDepth ) {
			DrawTris( VBO->numIndexes, NULL,
				  VBO->minIndex,
				  VBO->maxIndex, max,
				  1.0f, 1.0f, 0.0f );
		}
	}
	if ( input->numIndexes > 0 ) {
		vec3_t		*vertexes;
		vec2_t		*tcs;

		if ( input->vertexPtr1 ) {
			GL_VBO( 0, 0 );
			max = tess.numVertexes-1;
			vertexes = &input->vertexPtr2[0].xyz;
			tcs = &input->vertexPtr1[0].tc1;
		} else {
			GL_VBO( backEnd.worldVBO.vbo, 0 );
			max = backEnd.worldVBO.maxIndex;
			vertexes = &backEnd.worldVBO.offs2->xyz;
			tcs = &backEnd.worldVBO.offs1->tc1;
		}
		
		GL_ColorPtr( tess.svars.colors, sizeof(color4ub_t) );
		GL_TexCoordPtr( 0, 2, GL_FLOAT, sizeof(vaWord1_t), tcs[0] );
		qglVertexPointer( 3, GL_FLOAT, sizeof(vaWord2_t), vertexes );
		
		//
		// lock arrays
		//
		if ( qglLockArraysEXT ) {
			qglLockArraysEXT(0, input->numVertexes);
			GLimp_LogComment( "glLockArraysEXT\n" );
		}
		
		//
		// call special shade routine
		//
		R_DrawElements( input->numIndexes, input->indexPtr.p16,
				input->minIndex, input->maxIndex, max );

		if ( r_showtris->integer && !input->shader->isDepth ) {
			DrawTris( input->numIndexes, input->indexPtr.p16,
				  input->minIndex, input->maxIndex, max,
				  1.0f, 0.0f, 0.0f );
		}

		//
		// unlock arrays
		//
		if ( qglUnlockArraysEXT ) {
			qglUnlockArraysEXT();
			GLimp_LogComment( "glUnlockArraysEXT\n" );
		}
	}
}

void RB_StageIteratorLightmappedMultitexture( void ) {
	shaderCommands_t *input;
	GLuint            max;
	vboInfo_t        *VBO;
	image_t          *images[2];

	input = &tess;

	//
	// log this call
	//
	if ( r_logFile->integer ) {
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment( va("--- RB_StageIteratorLightmappedMultitexture( %s ) ---\n", tess.shader->name) );
	}

	//
	// set face culling appropriately
	//
	GL_Cull( input->shader->cullType );
	GL_Program( 0 );

	GL_State( GLS_DEFAULT );

	GL_Color4f( 1.0f, 1.0f, 1.0f, 1.0f );

	R_GetAnimatedImage( &tess.xstages[0]->bundle[0], qfalse, &images[0] );
	R_GetAnimatedImage( &tess.xstages[0]->bundle[1], qfalse, &images[1] );

	if ( r_lightmap->integer ) {
		GL_TexEnv( 1, GL_REPLACE );
	} else {
		GL_TexEnv( 1, GL_MODULATE );
	}
	GL_BindImages( 2, images );

	for( VBO = tess.firstVBO; VBO; VBO = VBO->next ) {
		vec3_t	*vertexes;
		vec2_t	*tcs1, *tcs2;
		
		if ( VBO->vbo ) {
			GL_VBO( VBO->vbo, VBO->ibo );
			max = VBO->maxIndex;
			vertexes = &VBO->offs2->xyz;
			tcs1 = &VBO->offs1->tc1;
			tcs2 = &VBO->offs1->tc2;
		} else {
			GL_VBO( backEnd.worldVBO.vbo, VBO->ibo );
			max = backEnd.worldVBO.maxIndex;
			vertexes = &backEnd.worldVBO.offs2->xyz;
			tcs1 = &backEnd.worldVBO.offs1->tc1;
			tcs2 = &backEnd.worldVBO.offs1->tc2;
		}
		//
		// set color, pointers, and lock
		//
		qglVertexPointer( 3, GL_FLOAT, sizeof(vaWord2_t), vertexes );
		
		//
		// select base stage
		//
		GL_TexCoordPtr( 0, 2, GL_FLOAT, sizeof(vaWord1_t), tcs1[0] );
		
		//
		// configure second stage
		//
		GL_TexCoordPtr( 1, 2, GL_FLOAT, sizeof(vaWord1_t), tcs2[0] );
		
		R_DrawElements( VBO->numIndexes, NULL,
				VBO->minIndex, VBO->maxIndex, max );
		
		if ( r_showtris->integer && !input->shader->isDepth ) {
			DrawTris( VBO->numIndexes, NULL,
				  VBO->minIndex,
				  VBO->maxIndex, max,
				  1.0f, 1.0f, 0.0f );
		}
	}
	if ( tess.numIndexes > 0 ) {
		vec3_t	*vertexes;
		vec2_t	*tcs1, *tcs2;
		
		if ( tess.vertexPtr1 ) {
			GL_VBO( 0, 0 );
			max = tess.numVertexes-1;
			vertexes = &input->vertexPtr2[0].xyz;
			tcs1 = &input->vertexPtr1[0].tc1;
			tcs2 = &input->vertexPtr1[0].tc2;
		} else {
			GL_VBO( backEnd.worldVBO.vbo, 0 );
			max = backEnd.worldVBO.maxIndex;
			vertexes = &backEnd.worldVBO.offs2->xyz;
			tcs1 = &backEnd.worldVBO.offs1->tc1;
			tcs2 = &backEnd.worldVBO.offs1->tc2;
		}

		//
		// set color, pointers, and lock
		//
		qglVertexPointer( 3, GL_FLOAT, sizeof(vaWord2_t), vertexes );
		
		//
		// select base stage
		//
		GL_TexCoordPtr( 0, 2, GL_FLOAT, sizeof(vaWord1_t), tcs1[0] );
		
		//
		// configure second stage
		//
		GL_TexCoordPtr( 1, 2, GL_FLOAT, sizeof(vaWord1_t), tcs2[0] );
		
		//
		// lock arrays
		//
		if ( qglLockArraysEXT ) {
			qglLockArraysEXT(0, input->numVertexes);
			GLimp_LogComment( "glLockArraysEXT\n" );
		}
		
		R_DrawElements( input->numIndexes, input->indexPtr.p16,
				input->minIndex, input->maxIndex, max );

		if ( r_showtris->integer && !input->shader->isDepth ) {
			DrawTris( input->numIndexes, input->indexPtr.p16,
				  input->minIndex, input->maxIndex, max,
				  1.0f, 0.0f, 0.0f );
		}
		
		//
		// unlock arrays
		//
		if ( qglUnlockArraysEXT ) {
			qglUnlockArraysEXT();
			GLimp_LogComment( "glUnlockArraysEXT\n" );
		}
	}
}

/*
** RB_StageIteratorGLSL
*/
void RB_StageIteratorGLSL( void ) {
	shaderCommands_t	*input = &tess;
	shader_t		*shader = input->shader;
	GLuint			max;
	int			i;
	vboInfo_t		*VBO;
	image_t			*images[MAX_SHADER_STAGES];
	GLint			glQueryID = 0;
	GLint			*glQueryResult = NULL;

	//
	// log this call
	//
	if ( r_logFile->integer ) 
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment( va("--- RB_StageIteratorGLSL( %s ) ---\n", shader->name) );
	}

	if( qglBeginQueryARB &&
	    !backEnd.viewParms.isPortal ) {
		if( backEnd.currentEntity == &tr.worldEntity ) {
			glQueryID = tess.shader->QueryID;
			glQueryResult = &tess.shader->QueryResult;
		} else if ( shader == tr.occlusionTestShader ) {
			glQueryID = backEnd.currentEntity->GLQueryID;
			glQueryResult = &backEnd.currentEntity->GLQueryResult;
		}
	}
	
	//
	// set face culling appropriately
	//
	GL_Cull( shader->cullType );
	if( glQueryID && *glQueryResult == (GLuint)(-1) &&
	    qglIsQueryARB( glQueryID) ) {
		GLuint available;
		
		// last attempt to get the query result
		qglGetQueryObjectuivARB( glQueryID,
					 GL_QUERY_RESULT_AVAILABLE_ARB,
					 &available);
		if ( available ) {
			qglGetQueryObjectivARB( glQueryID,
						GL_QUERY_RESULT_ARB,
						glQueryResult);
		}
	}
	if( glQueryID && *glQueryResult == 0 && shader->depthShader ) {
		GL_Program( shader->depthShader->GLSLprogram );
		GL_State( shader->depthShader->stages[0]->stateBits &
			  ~GLS_DEPTHMASK_TRUE );
		
		// bind only first texture
		if( shader->stages[0] ) {
			R_GetAnimatedImage( &shader->stages[0]->bundle[0], qtrue, &images[0] );
			GL_BindImages( 1, images );
		} else {
			GL_BindImages( 0, NULL );
		}
	} else {
		GL_Program( shader->GLSLprogram );
		if( glQueryID && *glQueryResult == 0 ) {
			GL_State( shader->stages[0]->stateBits |
				  GLS_COLORMASK_FALSE );
		} else {
			GL_State( shader->stages[0]->stateBits );
		}
		
		// bind all required textures
		for( i = 0; i < shader->numUnfoggedPasses; i++ ) {
			if( !shader->stages[i] )
				break;
			
			R_GetAnimatedImage( &shader->stages[i]->bundle[0], qtrue, &images[i] );
		}
		GL_BindImages( i, images );
	}
	
	// bind attributes
	GL_VertexAttrib3f( AL_CAMERAPOS,
			   backEnd.viewParms.or.origin[0],
			   backEnd.viewParms.or.origin[1],
			   backEnd.viewParms.or.origin[2] );
	if( backEnd.currentEntity->e.reType == RT_SPRITE ) {
		GL_VBO( 0, 0 );
		GL_VertexAttribPtr( AL_TIMES, 
				    3, GL_FLOAT, GL_FALSE,
				    sizeof(vaWord3_t),
				    tess.vertexPtr3[0].normal );
	} else {
		GL_VertexAttrib3f( AL_TIMES,
				   tess.shaderTime,
				   backEnd.currentEntity->e.backlerp,
				   backEnd.currentEntity == &tr.worldEntity
				   ? 0.0f
				   : tess.fogNum );
	}

	if( backEnd.currentEntity->e.reType != RT_MODEL ) {
		GL_VertexAttrib4f( AL_TRANSX,
				   1.0f, 0.0f, 0.0f, 0.0f );
		GL_VertexAttrib4f( AL_TRANSY,
				   0.0f, 1.0f, 0.0f, 0.0f );
		GL_VertexAttrib4f( AL_TRANSZ,
				   0.0f, 0.0f, 1.0f, 0.0f );
	} else {
		GL_VertexAttrib4f( AL_TRANSX,
				   backEnd.currentEntity->e.axis[0][0],
				   backEnd.currentEntity->e.axis[1][0],
				   backEnd.currentEntity->e.axis[2][0],
				   backEnd.currentEntity->e.origin[0] );
		GL_VertexAttrib4f( AL_TRANSY,
				   backEnd.currentEntity->e.axis[0][1],
				   backEnd.currentEntity->e.axis[1][1],
				   backEnd.currentEntity->e.axis[2][1],
				   backEnd.currentEntity->e.origin[1] );
		GL_VertexAttrib4f( AL_TRANSZ,
				   backEnd.currentEntity->e.axis[0][2],
				   backEnd.currentEntity->e.axis[1][2],
				   backEnd.currentEntity->e.axis[2][2],
				   backEnd.currentEntity->e.origin[2] );
	}
	GL_VertexAttrib4f( AL_AMBIENTLIGHT,
			   backEnd.currentEntity->ambientLight[0] / 255.0f,
			   backEnd.currentEntity->ambientLight[1] / 255.0f,
			   backEnd.currentEntity->ambientLight[2] / 255.0f,
			   1.0f );
	GL_VertexAttrib4f( AL_DIRECTEDLIGHT,
			   backEnd.currentEntity->directedLight[0] / 255.0f,
			   backEnd.currentEntity->directedLight[1] / 255.0f,
			   backEnd.currentEntity->directedLight[2] / 255.0f,
			   1.0f );
	if( backEnd.currentEntity == &tr.worldEntity ) {
		GL_VertexAttrib3f( AL_LIGHTDIR,
				   tr.sunDirection[0],
				   tr.sunDirection[1],
				   tr.sunDirection[2] );
	} else {
		GL_VertexAttrib3f( AL_LIGHTDIR,
				   backEnd.currentEntity->lightDir[0],
				   backEnd.currentEntity->lightDir[1],
				   backEnd.currentEntity->lightDir[2] );
	}
	
	if ( glQueryID && *glQueryResult != -1 ) {
		qglBeginQueryARB( GL_SAMPLES_PASSED_ARB, glQueryID );
	}

	for( VBO = input->firstVBO; VBO; VBO = VBO->next ) {
		int		minIndex, maxIndex;
		vaWord1_t	*word1;
		vaWord2_t	*word2, *oldWord2;
		vboWord3_t	*word3, *oldWord3;
		
		if ( VBO->vao ) {
			GL_VAO( VBO->vao );
			if ( VBO->vbo ) {
				max = VBO->maxIndex;
			} else {
				max = backEnd.worldVBO.maxIndex;
			}
			R_DrawElements( VBO->numIndexes, NULL,
					VBO->minIndex, VBO->maxIndex, max );
			GL_VAO( 0 );
		} else {
			vboInfo_t *dataVBO;
			
			if ( VBO->vbo ) {
				dataVBO = VBO;
			} else {
				dataVBO = &backEnd.worldVBO;
			}
			if( qglBindVertexArray &&
			    shader->lightmapIndex != LIGHTMAP_MD3 ) {
				GL_CreateVAO( &VBO->vao, dataVBO->vbo, VBO->ibo );
			} else {
				GL_VAO( 0 );
				GL_VBO( dataVBO->vbo, VBO->ibo );
			}

			max = dataVBO->maxIndex;
			word1 = dataVBO->offs1;
			word2 = dataVBO->offs2;
			word3 = dataVBO->offs3;
			
			if( shader->lightmapIndex == LIGHTMAP_MD3 ) {
				size_t  numVerts = (max + 4) & -4;
				
				minIndex = 0;
				maxIndex = max;
				
				oldWord2 = word2 + numVerts * backEnd.currentEntity->e.oldframe;
				word2 = word2 + numVerts * backEnd.currentEntity->e.frame;
				qglVertexPointer( 4, GL_FLOAT, sizeof(vaWord2_t), word2->xyz );
				GL_VertexAttribPtr( AL_OLDVERTEX, 3, GL_FLOAT,
						    GL_FALSE, sizeof(vaWord2_t),
						    &oldWord2->xyz );
				
				oldWord3 = word3 + numVerts * backEnd.currentEntity->e.oldframe;
				word3 = word3 + numVerts * backEnd.currentEntity->e.frame;
				
				GL_VertexAttribPtr( AL_NORMAL, 2, GL_SHORT,
						    GL_FALSE, sizeof(vaWord3_t),
						    &word3->normal );
				GL_VertexAttribPtr( AL_OLDNORMAL, 2, GL_SHORT,
						    GL_FALSE, sizeof(vaWord3_t),
						    &oldWord3->normal );
				
				GL_Color4ub( 255, 255, 255, 255 );
			} else {
				minIndex = VBO->minIndex;
				maxIndex = VBO->maxIndex;
				
				qglVertexPointer( 4, GL_FLOAT, sizeof(vaWord2_t), &word2->xyz );
				GL_VertexAttribPtr( AL_NORMAL, 2, GL_SHORT,
						    GL_FALSE, sizeof(vaWord3_t),
						    &word3->normal );
				
				if( !backEnd.currentEntity->e.shaderRGBA[3] ) {
					GL_ColorPtr( &word3->color, sizeof(vaWord3_t) );
				} else {
					GL_Color4ub(backEnd.currentEntity->e.shaderRGBA[0],
						    backEnd.currentEntity->e.shaderRGBA[1],
						    backEnd.currentEntity->e.shaderRGBA[2],
						    backEnd.currentEntity->e.shaderRGBA[3]);
				}
			}
			
			GL_TexCoordPtr( 0, 4, GL_FLOAT, sizeof(vaWord1_t),
					&word1->tc1[0] );
			
			R_DrawElements( VBO->numIndexes, NULL,
					minIndex, maxIndex, max );
			
			GL_VAO( 0 );
		}
	}

	if ( input->numIndexes > 0 ) {
		vaWord1_t	*word1;
		vaWord2_t	*word2;
		vboWord3_t	*word3;
		
		if ( input->vertexPtr1 ) {
			GL_VBO( 0, 0 );
			max = input->numIndexes-1;
			word1 = input->vertexPtr1;
			word2 = input->vertexPtr2;
			word3 = (vboWord3_t *)(input->vertexPtr3);
		} else {
			GL_VBO( backEnd.worldVBO.vbo, 0 );
			max = backEnd.worldVBO.maxIndex;
			word1 = backEnd.worldVBO.offs1;
			word2 = backEnd.worldVBO.offs2;
			word3 = backEnd.worldVBO.offs3;
		}
		//
		// lock XYZ
		//
		GL_VertexAttribPtr( AL_NORMAL,
				    2, GL_SHORT, GL_FALSE,
				    sizeof(vboWord3_t),
				    &word3->normal );
		
		qglVertexPointer (4, GL_FLOAT, sizeof(vaWord2_t), &word2->xyz );

		if( !backEnd.currentEntity->e.shaderRGBA[3] ) {
			GL_ColorPtr( &word3->color, sizeof(vboWord3_t) );
		} else {
			GL_Color4ub(backEnd.currentEntity->e.shaderRGBA[0],
				    backEnd.currentEntity->e.shaderRGBA[1],
				    backEnd.currentEntity->e.shaderRGBA[2],
				    backEnd.currentEntity->e.shaderRGBA[3]);
		}
		GL_TexCoordPtr( 0, 4, GL_FLOAT, sizeof(vaWord1_t), &word1->tc1[0] );

		if (qglLockArraysEXT)
		{
			qglLockArraysEXT(0, input->numVertexes);
			GLimp_LogComment( "glLockArraysEXT\n" );
		}
		
		//
		// call shader function
		//
		R_DrawElements( input->numIndexes, input->indexPtr.p16,
				input->minIndex, input->maxIndex, max );
		
		// 
		// unlock arrays
		//
		if (qglUnlockArraysEXT)
		{
			qglUnlockArraysEXT();
			GLimp_LogComment( "glUnlockArraysEXT\n" );
		}
	}
	if ( glQueryID && *glQueryResult != -1 ) {
		qglEndQueryARB( GL_SAMPLES_PASSED_ARB );
		*glQueryResult = -1;
	}
	
	if ( r_showtris->integer && !shader->isDepth ) {
		for( VBO = input->firstVBO; VBO; VBO = VBO->next ) {
			int		minIndex, maxIndex;
			vaWord2_t	*word2, *oldWord2;
			
			if ( VBO->vbo ) {
				GL_VBO( VBO->vbo, VBO->ibo );
				max = VBO->maxIndex;
				word2 = VBO->offs2;
			} else {
				GL_VBO( backEnd.worldVBO.vbo, VBO->ibo );
				max = backEnd.worldVBO.maxIndex;
				word2 = backEnd.worldVBO.offs2;
			}
			
			if( shader->lightmapIndex == LIGHTMAP_MD3 ) {
				size_t  numVerts = (max + 4) & -4;
				
				minIndex = 0;
				maxIndex = max;
				
				oldWord2 = word2 + numVerts * backEnd.currentEntity->e.oldframe;
				word2 = word2 + numVerts * backEnd.currentEntity->e.frame;
				qglVertexPointer( 4, GL_FLOAT, sizeof(vaWord2_t), word2->xyz );
				GL_VertexAttribPtr( AL_OLDVERTEX, 3, GL_FLOAT,
						    GL_FALSE, sizeof(vaWord2_t),
						    &oldWord2->xyz );
			} else {
				minIndex = VBO->minIndex;
				maxIndex = VBO->maxIndex;
				
				qglVertexPointer( 4, GL_FLOAT, sizeof(vaWord2_t), &word2->xyz );
			}
			
			if( glQueryID && *glQueryResult == 0 )
				DrawTris( VBO->numIndexes, NULL,
					  VBO->minIndex,
					  VBO->maxIndex, max,
					  0.3f, 0.3f, 1.0f );
			else
				DrawTris( VBO->numIndexes, NULL,
					  VBO->minIndex,
					  VBO->maxIndex, max,
					  0.0f, 0.0f, 1.0f );
		}
		
		if ( input->numIndexes > 0 ) {
			vaWord2_t	*word2;
			
			if ( input->vertexPtr1 ) {
				GL_VBO( 0, 0 );
				max = input->numIndexes-1;
				word2 = input->vertexPtr2;
			} else {
				GL_VBO( backEnd.worldVBO.vbo, 0 );
				max = backEnd.worldVBO.maxIndex;
				word2 = backEnd.worldVBO.offs2;
			}
			//
			// lock XYZ
			//
			qglVertexPointer (4, GL_FLOAT, sizeof(vaWord2_t), &word2->xyz );
			
			if (qglLockArraysEXT)
			{
				qglLockArraysEXT(0, input->numVertexes);
				GLimp_LogComment( "glLockArraysEXT\n" );
			}
			
			if( glQueryID && *glQueryResult == 0)
				DrawTris( input->numIndexes, input->indexPtr.p16,
					  input->minIndex, input->maxIndex, max,
					  0.3f, 1.0f, 0.3f );
			else
				DrawTris( input->numIndexes, input->indexPtr.p16,
					  input->minIndex, input->maxIndex, max,
					  0.0f, 1.0f, 0.0f );
			
			// 
			// unlock arrays
			//
			if (qglUnlockArraysEXT)
			{
				qglUnlockArraysEXT();
				GLimp_LogComment( "glUnlockArraysEXT\n" );
			}
		}
	}
}

/*
** RB_EndSurface
*/
void RB_EndSurface( void ) {
	shaderCommands_t *input;

	input = &tess;

	if (input->numIndexes == 0 && !input->firstVBO ) {
		return;
	}
	
	if ( !tess.shader ) {
		return;
	}

	if ( tess.shader == tr.shadowShader ) {
		RB_ShadowTessEnd();
		return;
	}

	// for debugging of sort order issues, stop rendering after a given sort value
	if ( r_debugSort->integer && r_debugSort->integer < tess.shader->sort ) {
		return;
	}

	//
	// update performance counters
	//
	backEnd.pc.c_shaders++;
	backEnd.pc.c_vertexes += tess.numVertexes;
	backEnd.pc.c_indexes += tess.numIndexes;
	backEnd.pc.c_totalIndexes += tess.numIndexes * tess.numPasses;

	//
	// call off to shader specific tess end function
	//
	tess.currentStageIteratorFunc();

	//
	// draw debugging stuff
	//
	if ( r_shownormals->integer ) {
		DrawNormals ();
	}
	if ( r_flush->integer ) {
		qglFlush();
	}

	// clear shader so we can tell we don't have any unclosed surfaces
	tess.numIndexes = 0;
	tess.firstVBO = NULL;

	GLimp_LogComment( "----------\n" );
}

/*
** RB_LightSurface
*/
void RB_LightSurface( void ) {
	shaderCommands_t *input;

	input = &tess;

	if (input->numIndexes == 0) {
		return;
	}

	// for debugging of sort order issues, stop rendering after a given sort value
	if ( r_debugSort->integer && r_debugSort->integer < tess.shader->sort ) {
		return;
	}

	RB_DeformTessGeometry();

	//
	// log this call
	//
	if ( r_logFile->integer ) 
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment( va("--- RB_LightSurface( %s ) ---\n", tess.shader->name) );
	}

	//
	// set face culling appropriately
	//
	GL_Cull( input->shader->cullType );

	GL_VBO( 0, 0 );
	GL_Program( 0 );

	// if there is only a single pass then we can enable color
	// and texture arrays before we compile, otherwise we need
	// to avoid compiling those arrays since they will change
	// during multipass rendering
	//
	qglDisableClientState (GL_TEXTURE_COORD_ARRAY);
	
	//
	// lock XYZ
	//
	qglVertexPointer (3, GL_FLOAT, sizeof(vaWord2_t),
			  input->vertexPtr2[0].xyz);
	if (qglLockArraysEXT)
	{
		qglLockArraysEXT(0, input->numVertexes);
		GLimp_LogComment( "glLockArraysEXT\n" );
	}
	
	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	
	// 
	// now do any dynamic lighting needed
	//
	if ( tess.dlightBits && tess.shader->sort <= SS_OPAQUE
	     && !(tess.shader->surfaceFlags & (SURF_NODLIGHT | SURF_SKY) ) ) {
		ProjectDlightTexture();
	}
	
	//
	// now do fog
	//
	if ( tess.fogNum && tess.shader->fogPass ) {
		RB_FogPass();
	}
	
	// 
	// unlock arrays
	//
	if (qglUnlockArraysEXT) 
	{
		qglUnlockArraysEXT();
		GLimp_LogComment( "glUnlockArraysEXT\n" );
	}
	
	if ( r_flush->integer ) {
		qglFlush();
	}

	// clear shader so we can tell we don't have any unclosed surfaces
	tess.numIndexes = 0;
	tess.firstVBO = NULL;

	GLimp_LogComment( "----------\n" );
}
