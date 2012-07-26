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
#include "tr_local.h"

backEndData_t	*backEndData[SMP_FRAMES];
backEndState_t	backEnd;


static float	s_flipMatrix[16] = {
	// convert from our coordinate system (looking down X)
	// to OpenGL's coordinate system (looking down -Z)
	0, 0, -1, 0,
	-1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
};


/*
** GL_BindTexture
*/
void GL_BindTexture( int texnum ) {
	if ( glState.currenttextures[0] != texnum ) {
		glState.currenttextures[0] = texnum;
		qglBindTexture (GL_TEXTURE_2D, texnum);
	}
}

/*
** GL_Bind
*/
void GL_BindImages( int count, image_t **images ) {
	int i, texnum;
	
	if( !qglActiveTextureARB && count > 1 ) {
		ri.Printf( PRINT_WARNING, "GL_BindImages: Multitexturing not enabled\n" );
		count = 1;
	}
	
	for( i = 0; i < count; i++ ) {
		if ( !images[i] ) {
			ri.Printf( PRINT_WARNING, "GL_BindImages: NULL image\n" );
			texnum = tr.defaultImage->texnum;
		} else {
			texnum = images[i]->texnum;
		}
		
		if ( r_nobind->integer && tr.dlightImage ) {		// performance evaluation option
			texnum = tr.dlightImage->texnum;
		}
		
		if ( glState.currenttextures[i] != texnum ) {
			images[i]->frameUsed = tr.frameCount;
			glState.currenttextures[i] = texnum;
			if( qglActiveTextureARB )
				qglActiveTextureARB( GL_TEXTURE0_ARB + i );
			qglBindTexture (GL_TEXTURE_2D, texnum);
			qglEnable( GL_TEXTURE_2D );
		}
	}
	for( ; i < NUM_TEXTURE_BUNDLES; i++ ) {
		if( i >= glGlobals.maxTextureImageUnits )
			break;
		
		if( glState.currenttextures[i] ) {
			glState.currenttextures[i] = 0;
			if( qglActiveTextureARB )
				qglActiveTextureARB( GL_TEXTURE0_ARB + i );
			qglBindTexture (GL_TEXTURE_2D, 0);
			qglDisable( GL_TEXTURE_2D );
		}
	}
	if( qglActiveTextureARB )
		qglActiveTextureARB( GL_TEXTURE0_ARB );
}

/*
** GL_TexCoord
*/
void GL_TexCoordPtr( int unit, int size, GLenum type, int stride, GLfloat *data ) {
	if( data == NULL && !glState.currentVBO ) {
		qglClientActiveTextureARB( GL_TEXTURE0_ARB + unit );
		qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	} else {
		qglClientActiveTextureARB( GL_TEXTURE0_ARB + unit );
		qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		qglTexCoordPointer( size, type, stride, data );
	}
}


/*
** GL_Color
*/
void GL_Color4f( GLfloat r, GLfloat g, GLfloat b, GLfloat a ) {
	if( glState.currentVAO ) {
		qglDisableClientState( GL_COLOR_ARRAY );
	} else if( backEnd.glAttribute[AL_COLOR].isPointer ) {
		backEnd.glAttribute[AL_COLOR].isPointer = qfalse;
		qglDisableClientState( GL_COLOR_ARRAY );
	}
	qglColor4f( r, g, b, a );
}
void GL_Color4ub( GLubyte r, GLubyte g, GLubyte b, GLubyte a ) {
	if( glState.currentVAO ) {
		qglDisableClientState( GL_COLOR_ARRAY );
	} else if( backEnd.glAttribute[AL_COLOR].isPointer ) {
		backEnd.glAttribute[AL_COLOR].isPointer = qfalse;
		qglDisableClientState( GL_COLOR_ARRAY );
	}
	qglColor4ub( r, g, b, a );
}
void GL_ColorPtr( color4ub_t *ptr, GLsizei stride ) {
	if( glState.currentVAO ) {
		qglEnableClientState( GL_COLOR_ARRAY );
	} else if( !backEnd.glAttribute[AL_COLOR].isPointer ) {
		backEnd.glAttribute[AL_COLOR].isPointer = qtrue;
		qglEnableClientState( GL_COLOR_ARRAY );
	}
	qglColorPointer( 4, GL_UNSIGNED_BYTE, stride, ptr );
}

void GL_VertexAttrib4f( int index, float x, float y, float z, float w ) {
	if( glState.currentVAO ) {
		qglDisableVertexAttribArrayARB( index );
	} else if( backEnd.glAttribute[index].isPointer ) {
		backEnd.glAttribute[index].isPointer = qfalse;
		qglDisableVertexAttribArrayARB( index );
	}
	qglVertexAttrib4fARB( index, x, y, z, w );
}
void GL_VertexAttrib4Nub( int index, GLubyte x, GLubyte y, GLubyte z, GLubyte w ) {
	if( glState.currentVAO ) {
		qglDisableVertexAttribArrayARB( index );
	} else if( backEnd.glAttribute[index].isPointer ) {
		backEnd.glAttribute[index].isPointer = qfalse;
		qglDisableVertexAttribArrayARB( index );
	}
	qglVertexAttrib4NubARB( index, x, y, z, w );
}
void GL_VertexAttrib3f( int index, float x, float y, float z ) {
	if( glState.currentVAO ) {
		qglDisableVertexAttribArrayARB( index );
	} else if( backEnd.glAttribute[index].isPointer ) {
		backEnd.glAttribute[index].isPointer = qfalse;
		qglDisableVertexAttribArrayARB( index );
	}
	qglVertexAttrib3fARB( index, x, y, z );
}
void GL_VertexAttribPtr( int index, int size, int type, int scale,
			 size_t stride, void *ptr ) {
	if( glState.currentVAO ) {
		qglEnableVertexAttribArrayARB( index );
	} else if( !backEnd.glAttribute[index].isPointer ) {
		backEnd.glAttribute[index].isPointer = qtrue;
		qglEnableVertexAttribArrayARB( index );
	}
	qglVertexAttribPointerARB( index, size, type, scale, stride, ptr );
}

/*
** GL_Cull
*/
void GL_Cull( int cullType ) {
	if ( glState.faceCulling == cullType ) {
		return;
	}

	glState.faceCulling = cullType;

	if ( cullType == CT_TWO_SIDED ) 
	{
		qglDisable( GL_CULL_FACE );
	} 
	else 
	{
		qglEnable( GL_CULL_FACE );

		if ( cullType == CT_BACK_SIDED )
		{
			if ( backEnd.viewParms.isMirror )
			{
				qglCullFace( GL_FRONT );
			}
			else
			{
				qglCullFace( GL_BACK );
			}
		}
		else
		{
			if ( backEnd.viewParms.isMirror )
			{
				qglCullFace( GL_BACK );
			}
			else
			{
				qglCullFace( GL_FRONT );
			}
		}
	}
}

/*
** GL_TexEnv
*/
void GL_TexEnv( int tmu, int env )
{
	if ( env == glState.texEnv[tmu] )
	{
		return;
	}

	glState.texEnv[tmu] = env;
	qglActiveTextureARB( GL_TEXTURE0_ARB + tmu );

	switch ( env )
	{
	case GL_MODULATE:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		break;
	case GL_REPLACE:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
		break;
	case GL_DECAL:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL );
		break;
	case GL_ADD:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD );
		break;
	default:
		ri.Error( ERR_DROP, "GL_TexEnv: invalid env '%d' passed\n", env );
		break;
	}
}

/*
** GL_State
**
** This routine is responsible for setting the most commonly changed state
** in Q3.
*/
void GL_State( unsigned long stateBits )
{
	unsigned long diff = stateBits ^ glState.glStateBits;

	if ( !diff )
	{
		return;
	}

	//
	// check depthFunc bits
	//
	if ( diff & GLS_DEPTHFUNC_EQUAL )
	{
		if ( stateBits & GLS_DEPTHFUNC_EQUAL )
		{
			qglDepthFunc( GL_EQUAL );
		}
		else
		{
			qglDepthFunc( GL_LEQUAL );
		}
	}

	//
	// check blend bits
	//
	if ( diff & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) )
	{
		GLenum srcFactor, dstFactor;

		if ( stateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) )
		{
			switch ( stateBits & GLS_SRCBLEND_BITS )
			{
			case GLS_SRCBLEND_ZERO:
				srcFactor = GL_ZERO;
				break;
			case GLS_SRCBLEND_ONE:
				srcFactor = GL_ONE;
				break;
			case GLS_SRCBLEND_DST_COLOR:
				srcFactor = GL_DST_COLOR;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
				srcFactor = GL_ONE_MINUS_DST_COLOR;
				break;
			case GLS_SRCBLEND_SRC_ALPHA:
				srcFactor = GL_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
				srcFactor = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_DST_ALPHA:
				srcFactor = GL_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
				srcFactor = GL_ONE_MINUS_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ALPHA_SATURATE:
				srcFactor = GL_SRC_ALPHA_SATURATE;
				break;
			default:
				srcFactor = GL_ONE;		// to get warning to shut up
				ri.Error( ERR_DROP, "GL_State: invalid src blend state bits\n" );
				break;
			}

			switch ( stateBits & GLS_DSTBLEND_BITS )
			{
			case GLS_DSTBLEND_ZERO:
				dstFactor = GL_ZERO;
				break;
			case GLS_DSTBLEND_ONE:
				dstFactor = GL_ONE;
				break;
			case GLS_DSTBLEND_SRC_COLOR:
				dstFactor = GL_SRC_COLOR;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
				dstFactor = GL_ONE_MINUS_SRC_COLOR;
				break;
			case GLS_DSTBLEND_SRC_ALPHA:
				dstFactor = GL_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
				dstFactor = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_DST_ALPHA:
				dstFactor = GL_DST_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
				dstFactor = GL_ONE_MINUS_DST_ALPHA;
				break;
			default:
				dstFactor = GL_ONE;		// to get warning to shut up
				ri.Error( ERR_DROP, "GL_State: invalid dst blend state bits\n" );
				break;
			}

			qglEnable( GL_BLEND );
			qglBlendFunc( srcFactor, dstFactor );
		}
		else
		{
			qglDisable( GL_BLEND );
		}
	}

	//
	// check depthmask
	//
	if ( diff & GLS_DEPTHMASK_TRUE )
	{
		if ( stateBits & GLS_DEPTHMASK_TRUE )
		{
			qglDepthMask( GL_TRUE );
		}
		else
		{
			qglDepthMask( GL_FALSE );
		}
	}

	//
	// check colormask
	//
	if ( diff & GLS_COLORMASK_FALSE )
	{
		if ( stateBits & GLS_COLORMASK_FALSE )
		{
		  qglColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
		}
		else
		{
		  qglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		}
	}

	// check polygon offset
	if ( diff & GLS_POLYGON_OFFSET )
	{
		if ( stateBits & GLS_POLYGON_OFFSET )
		{
			qglEnable( GL_POLYGON_OFFSET_FILL );
			qglPolygonOffset( r_offsetFactor->value, r_offsetUnits->value );
		}
		else
		{
			qglDisable( GL_POLYGON_OFFSET_FILL );
		}
	}

	//
	// fill/line mode
	//
	if ( diff & GLS_POLYMODE_LINE )
	{
		if ( stateBits & GLS_POLYMODE_LINE )
		{
			qglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		}
		else
		{
			qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		}
	}

	//
	// depthtest
	//
	if ( diff & GLS_DEPTHTEST_DISABLE )
	{
		if ( stateBits & GLS_DEPTHTEST_DISABLE )
		{
			qglDisable( GL_DEPTH_TEST );
		}
		else
		{
			qglEnable( GL_DEPTH_TEST );
		}
	}

	//
	// alpha test
	//
	if ( diff & GLS_ATEST_BITS )
	{
		switch ( stateBits & GLS_ATEST_BITS )
		{
		case 0:
			qglDisable( GL_ALPHA_TEST );
			break;
		case GLS_ATEST_GT_0:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GREATER, 0.0f );
			break;
		case GLS_ATEST_LT_80:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_LESS, 0.5f );
			break;
		case GLS_ATEST_GE_80:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GEQUAL, 0.5f );
			break;
		default:
			assert( 0 );
			break;
		}
	}

	glState.glStateBits = stateBits;
}



/*
** GL_State
**
** This routine is responsible for setting the most commonly changed state
** in Q3.
*/
void GL_CreateVAO( GLuint *vao, GLuint vbo, GLuint ibo ) {
	qglGenVertexArrays( 1, vao );
	GL_VAO( *vao );
	glState.currentVBO = vbo;
	qglBindBufferARB (GL_ARRAY_BUFFER_ARB, vbo );
	glState.currentIBO = ibo;
	qglBindBufferARB (GL_ELEMENT_ARRAY_BUFFER_ARB, ibo );

	qglEnableClientState( GL_VERTEX_ARRAY );
}
void GL_VAO( GLuint vao ){
	if ( glState.currentVAO != vao ) {
		glState.currentVAO = vao;
		qglBindVertexArray( vao );
	}
}
void GL_VBO( GLuint vbo, GLuint ibo )
{
	if ( glState.currentVAO )
		return;
	
	if ( glState.currentVBO != vbo ) {
		glState.currentVBO = vbo;
		qglBindBufferARB (GL_ARRAY_BUFFER_ARB, vbo );
	}

	if ( glState.currentIBO != ibo ) {
		glState.currentIBO = ibo;
		qglBindBufferARB (GL_ELEMENT_ARRAY_BUFFER_ARB, ibo );
	}
}

GLint RB_CompileShader( GLenum type, const char **code, int parts ) {
	GLint shader;
	GLint status;
	
	shader = qglCreateShader( type );
	qglShaderSource( shader, parts, code, NULL );
	qglCompileShader( shader );
	qglGetShaderiv( shader, GL_OBJECT_COMPILE_STATUS_ARB, &status );
	if( !status ) {
		char *log;
		GLint len;
		qglGetShaderiv( shader, GL_OBJECT_INFO_LOG_LENGTH_ARB, &len );
		log = ri.Hunk_AllocateTempMemory( len + 1 );
		qglGetShaderInfoLog( shader, len + 1, &len, log );
		
		ri.Printf( PRINT_DEVELOPER, "compile shader error: %s\n", log );
		
		ri.Hunk_FreeTempMemory( log );
		qglDeleteShader( shader );
		return 0;
	}
	return shader;
}

GLint RB_CompileProgram( const char *name,
			 const char **VScode, int VSparts,
			 const char **FScode, int FSparts,
			 unsigned int *hashPtr ) {
	GLint		VertexShader;
	GLint		FragmentShader;
	GLint		Program;
	GLint		i, j;
	unsigned int	hash;
	const char	*ptr;

	// try to reuse existing program
	for( i = 0, hash = 0; i < VSparts; i++ ) {
		for( ptr = VScode[i]; *ptr; ptr++ ) {
			hash = *ptr + hash * 65599;
		}
	}
	for( i = 0; i < FSparts; i++ ) {
		for( ptr = FScode[i]; *ptr; ptr++ ) {
			hash = *ptr + hash * 65599;
		}
	}

	for( i = 0; i < tr.numShaders; i++ ) {
		if( tr.shaders[i]->GLSLprogram &&
		    tr.shaders[i]->programHash == hash ) {
			GLuint  shaders[4];
			GLsizei count;
			GLint   shader, type, length;
			char    *source, *sourcePtr;
			qboolean same = qtrue;

			qglGetAttachedShaders( tr.shaders[i]->GLSLprogram,
					       4, &count, shaders );
			for( shader = 0; shader < count; shader++ ) {
				qglGetShaderiv( shaders[shader],
						GL_SHADER_SOURCE_LENGTH,
						&length );
				qglGetShaderiv( shaders[shader],
						GL_SHADER_TYPE,
						&type );
				sourcePtr = source = ri.Hunk_AllocateTempMemory( length + 1);
				qglGetShaderSource( shaders[shader], length + 1,
						    NULL, source );
				
				if( type == GL_VERTEX_SHADER ) {
					for( j = 0; j < VSparts; j++ ) {
						for( ptr = VScode[j]; *ptr; ptr++, sourcePtr++ ) {
							if( *ptr != *sourcePtr )
								break;
						}
					}
					same = same && *sourcePtr == '\0';
				} else if ( type == GL_FRAGMENT_SHADER ) {
					for( j = 0; j < FSparts; j++ ) {
						for( ptr = FScode[j]; *ptr; ptr++, sourcePtr++ ) {
							if( *ptr != *sourcePtr )
								break;
						}
					}
					same = same && *sourcePtr == '\0';
				}
				ri.Hunk_FreeTempMemory( source );
			}

			if( same ) {
				*hashPtr = hash;
				return tr.shaders[i]->GLSLprogram;
			}
		}
	}
	VertexShader = RB_CompileShader( GL_VERTEX_SHADER_ARB, VScode, VSparts );
	if( !VertexShader ) {
		return 0;
	}

	FragmentShader = RB_CompileShader( GL_FRAGMENT_SHADER_ARB, FScode, FSparts );
	if ( !FragmentShader ) {
		qglDeleteShader( VertexShader );
		return 0;
	}
	
	Program = qglCreateProgram();
	qglAttachShader( Program, VertexShader );
	qglAttachShader( Program, FragmentShader );
	
	qglBindAttribLocationARB( Program, AL_CAMERAPOS,     "aCameraPos" );
	qglBindAttribLocationARB( Program, AL_OLDVERTEX,     "aOldVertex" );
	qglBindAttribLocationARB( Program, AL_OLDNORMAL,     "aOldNormal" );
	qglBindAttribLocationARB( Program, AL_TIMES,         "aTimes" );
	qglBindAttribLocationARB( Program, AL_TANGENTS,      "aTangents" );
	qglBindAttribLocationARB( Program, AL_TRANSX,        "aTransX" );
	qglBindAttribLocationARB( Program, AL_TRANSY,        "aTransY" );
	qglBindAttribLocationARB( Program, AL_TRANSZ,        "aTransZ" );
	qglBindAttribLocationARB( Program, AL_AMBIENTLIGHT,  "aAmbientLight" );
	qglBindAttribLocationARB( Program, AL_DIRECTEDLIGHT, "aDirectedLight" );
	qglBindAttribLocationARB( Program, AL_LIGHTDIR,      "aLightDir" );
	qglBindAttribLocationARB( Program, AL_OLDTANGENTS,   "aOldTangents" );
	
	qglLinkProgram( Program );
	qglGetProgramiv( Program, GL_OBJECT_LINK_STATUS_ARB, &i );
	if ( !i ) {
		char *log;
		qglGetProgramiv( Program, GL_OBJECT_INFO_LOG_LENGTH_ARB, &i );
		log = ri.Hunk_AllocateTempMemory( i + 1 );
		qglGetProgramInfoLog( Program, i + 1, &i, log );
		
		ri.Printf( PRINT_DEVELOPER, "link shader %s error: %s\n", name, log );

		ri.Hunk_FreeTempMemory( log );
		qglDeleteProgram( Program );
		qglDeleteShader( FragmentShader );
		qglDeleteShader( VertexShader );
		return 0;
	}
	// shader object handles are no longer needed, they are bound
	// until the program object is deleted
	qglDeleteShader( FragmentShader );
	qglDeleteShader( VertexShader );

	*hashPtr = hash;
	return Program;
}

void GL_Program( GLint program )
{
	if ( glState.currentProgram != program ) {
		glState.currentProgram = program;
		qglUseProgram( program );
	}
}

/*
================
RB_Hyperspace

A player has predicted a teleport, but hasn't arrived yet
================
*/
static void RB_Hyperspace( void ) {
	float		c;

	if ( !backEnd.isHyperspace ) {
		// do initialization shit
	}

	c = ( backEnd.refdef.time & 255 ) / 255.0f;
	qglClearColor( c, c, c, 1 );
	qglClear( GL_COLOR_BUFFER_BIT );

	backEnd.isHyperspace = qtrue;
}


static void SetViewportAndScissor( void ) {
	qglMatrixMode(GL_PROJECTION);
	qglLoadMatrixf( backEnd.viewParms.projectionMatrix );
	qglMatrixMode(GL_MODELVIEW);

	// set the window clipping
	qglViewport( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY, 
		backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
	qglScissor( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY, 
		backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
}

/*
=================
RB_BeginDrawingView

Any mirrored or portaled views have already been drawn, so prepare
to actually render the visible surfaces for this view
=================
*/
void RB_BeginDrawingView (void) {
	int clearBits = 0;

	// sync with gl if needed
	if ( r_finish->integer == 1 && !glState.finishCalled ) {
		qglFinish ();
		glState.finishCalled = qtrue;
	}
	if ( r_finish->integer == 0 ) {
		glState.finishCalled = qtrue;
	}

	// we will need to change the projection matrix before drawing
	// 2D images again
	backEnd.projection2D = qfalse;

	//
	// set the modelview matrix for the viewer
	//
	SetViewportAndScissor();

	// ensures that depth writes are enabled for the depth clear
	GL_State( GLS_DEFAULT );
	// clear relevant buffers
	clearBits = GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;

	if ( r_measureOverdraw->integer || r_shadows->integer == 2 )
	{
		clearBits |= GL_STENCIL_BUFFER_BIT;
	}
	if ( r_fastsky->integer && !( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) )
	{
		clearBits |= GL_COLOR_BUFFER_BIT;	// FIXME: only if sky shaders have been used
#ifdef _DEBUG
		qglClearColor( 0.8f, 0.7f, 0.4f, 1.0f );	// FIXME: get color of sky
#else
		qglClearColor( 0.0f, 0.0f, 0.0f, 1.0f );	// FIXME: get color of sky
#endif
	}
	qglClear( clearBits );

	if ( ( backEnd.refdef.rdflags & RDF_HYPERSPACE ) )
	{
		RB_Hyperspace();
		return;
	}
	else
	{
		backEnd.isHyperspace = qfalse;
	}

	glState.faceCulling = -1;		// force face culling to set next time

	// we will only draw a sun if there was sky rendered in this view
	backEnd.skyRenderedThisView = qfalse;

	// clip to the plane of the portal
	if ( backEnd.viewParms.isPortal ) {
		float	plane[4];
		double	plane2[4];

		plane[0] = backEnd.viewParms.portalPlane.normal[0];
		plane[1] = backEnd.viewParms.portalPlane.normal[1];
		plane[2] = backEnd.viewParms.portalPlane.normal[2];
		plane[3] = backEnd.viewParms.portalPlane.dist;

		plane2[0] = DotProduct (backEnd.viewParms.or.axis[0], plane);
		plane2[1] = DotProduct (backEnd.viewParms.or.axis[1], plane);
		plane2[2] = DotProduct (backEnd.viewParms.or.axis[2], plane);
		plane2[3] = DotProduct (plane, backEnd.viewParms.or.origin) - plane[3];

		qglLoadMatrixf( s_flipMatrix );
		qglClipPlane (GL_CLIP_PLANE0, plane2);
		qglEnable (GL_CLIP_PLANE0);
	} else {
		qglDisable (GL_CLIP_PLANE0);
	}
}


/*
==================
RB_findShaderVBO
==================
*/
static int
findShaderVBO( vboInfo_t **root, int VBOkey ) {
	vboInfo_t node, *left, *right, *tmp;
	qboolean  found = qfalse;

	if ( !*root )
		return qfalse;

	node.left = node.right = NULL;
	left = right = &node;
	
	while( 1 ) {
		if( VBOkey < (*root)->key ) {
			tmp = (*root)->left;
			if( !tmp )
				break;
			if( VBOkey < tmp->key ) {
				(*root)->left = tmp->right;
				tmp->right = *root;
				*root = tmp;
				if( !(*root)->left )
					break;
			}
			right->left = *root;
			right = *root;
			*root = (*root)->left;
		} else if( VBOkey > (*root)->key ) {
			tmp = (*root)->right;
			if( !tmp )
				break;
			if( VBOkey > tmp->key ) {
				(*root)->right = tmp->left;
				tmp->left = *root;
				*root = tmp;
				if( !(*root)->right )
					break;
			}
			left->right = *root;
			left = *root;
			*root = (*root)->right;
		} else {
			found = qtrue;
			break;
		}
	}
        left->right = (*root)->left;
	right->left = (*root)->right;
        (*root)->left = node.right;
        (*root)->right = node.left;
	return found;
}

static void newShaderVBO( vboInfo_t **root, int VBOkey )
{
	vboInfo_t *new;

	if ( !backEnd.vboReserveCount ) {
		backEnd.vboReserve = ri.Hunk_Alloc( sizeof(vboInfo_t) *
						    (backEnd.vboReserveCount = 1000),
						    h_dontcare );
	}
	
	new = backEnd.vboReserve++;
	backEnd.vboReserveCount--;
	
	// requires that the tree has been splayed with findShaderVBO
	// before
	if( !*root ) {
		new->left = new->right = NULL;
	} else {
		if( VBOkey < (*root)->key ) {
			new->left = (*root)->left;
			(*root)->left = NULL;
			new->right = *root;
		} else if ( VBOkey > (*root)->key ) {
			new->right = (*root)->right;
			(*root)->right = NULL;
			new->left = *root;
		} else
			return; // should not happen
	}
	*root = new;
}

vboInfo_t *RB_CreateShaderVBO( vboInfo_t **root, int VBOkey ) {
	if( !findShaderVBO( root, VBOkey ) ) {
		newShaderVBO( root, VBOkey );
		(*root)->vbo = 0;
		(*root)->ibo = 0;
	}
	(*root)->key = VBOkey;
	
	return *root;
}
void RB_CopyVBO( vboInfo_t **root, int VBOkeyNew, int VBOkeyOld ) {
	vboInfo_t *oldVBO;
	if( !findShaderVBO( root, VBOkeyOld ) )
		return;

	oldVBO = *root;
	
	if( !findShaderVBO( root, VBOkeyNew ) ) {
		newShaderVBO( root, VBOkeyNew );
		(*root)->vbo = oldVBO->vbo;
		(*root)->ibo = oldVBO->ibo;
		(*root)->numIndexes = oldVBO->numIndexes;
		(*root)->minIndex = oldVBO->minIndex;
		(*root)->maxIndex = oldVBO->maxIndex;
		(*root)->offs1 = oldVBO->offs1;
		(*root)->offs2 = oldVBO->offs2;
		(*root)->offs3 = oldVBO->offs3;
	}
	(*root)->key = VBOkeyNew;
}

#define	MAC_EVENT_PUMP_MSEC		5


void RB_ClearVertexBuffer( void ) {
	tess.indexPtr.p16 = NULL;
	tess.indexInc = 0;
	tess.vertexPtr1 = NULL;
	tess.vertexPtr2 = NULL;
	tess.vertexPtr3 = NULL;

	tess.numVertexes = tess.numIndexes = tess.maxIndex = 0;
}
void RB_SetupVertexBuffer(shader_t *shader) {
	size_t	requiredSize;

	if ( r_shadows->integer == 2 && backEnd.currentEntity != &tr.worldEntity ) {
		// need more room for stencil shadows
		tess.numVertexes *= 2;
		tess.numIndexes *= 6;
	}

	if ( tess.numVertexes > 65536 ) {
		tess.indexInc = sizeof(GLuint);
	} else {
		tess.indexInc = sizeof(GLushort);
	}
	
	// round to next multiple of 4 vertexes for alignment
	tess.numVertexes = (tess.numVertexes + 3) & -4;

	requiredSize = 16 +
		(sizeof(vaWord1_t) + sizeof(vaWord2_t) + sizeof(vaWord3_t)) * tess.numVertexes +
		tess.indexInc * tess.numIndexes +
		sizeof(int);

	if ( tess.vertexBuffer ) {
		if ( *(int *)tess.vertexBufferEnd != 0xDEADBEEF) {
			ri.Error( ERR_DROP, "VertexBuffer overflow" );
		}
		if ( tess.vertexBufferEnd - tess.vertexBuffer < requiredSize - sizeof(int) ) {
			ri.Free( tess.vertexBuffer );
			tess.vertexBuffer = tess.vertexBufferEnd = NULL;
		}
	}

	if ( !tess.vertexBuffer ) {
		tess.vertexBuffer = ri.Malloc( requiredSize );
		tess.vertexBufferEnd = tess.vertexBuffer + requiredSize	- sizeof(int);
		*(int *)tess.vertexBufferEnd = 0xDEADBEEF;
	}

	tess.vertexPtr1 = (vaWord1_t *)(((intptr_t)tess.vertexBuffer + 15) & -16);
	tess.vertexPtr2 = (vaWord2_t *)(tess.vertexPtr1 + tess.numVertexes);
	tess.vertexPtr3 = (vaWord3_t *)(tess.vertexPtr2 + tess.numVertexes);
	tess.indexPtr.p16 = (GLushort *)(tess.vertexPtr3 + tess.numVertexes);

	tess.numVertexes = tess.numIndexes = tess.maxIndex = 0;
}
void RB_SetupVBOBuffer(GLuint *vbo, GLuint **indexes) {
	size_t	requiredSize;
	
	// store no indexes in the buffer
	if ( tess.numVertexes > 65536 ) {
		tess.indexInc = sizeof(GLuint);
	} else {
		tess.indexInc = sizeof(GLushort);
	}
	*indexes = ri.Hunk_AllocateTempMemory(tess.numIndexes * tess.indexInc);
	tess.indexPtr.p32 = *indexes;
	
	// round to next multiple of 4 vertexes for alignment
	tess.numVertexes = (tess.numVertexes + 3) & -4;
	requiredSize = (sizeof(vaWord1_t) + sizeof(vaWord2_t) + sizeof(vaWord3_t)) * tess.numVertexes;
	
	// create and map VBO
	qglGenBuffersARB(1, vbo);
	GL_VBO(*vbo, 0);
	qglBufferDataARB(GL_ARRAY_BUFFER_ARB, requiredSize,
			 NULL, GL_STATIC_DRAW_ARB);
	
	tess.vertexPtr1 = (vaWord1_t *)qglMapBufferARB(GL_ARRAY_BUFFER,
						       GL_WRITE_ONLY);
	tess.vertexPtr2 = (vaWord2_t *)(tess.vertexPtr1 + tess.numVertexes);
	tess.vertexPtr3 = (vaWord3_t *)(tess.vertexPtr2 + tess.numVertexes);
	tess.numVertexes = tess.numIndexes = tess.maxIndex = 0;
}
void RB_SetupIndexBuffer( void ) {
	size_t	requiredSize;

	// store only indexes, vertexes are already in world VBO
	if ( backEnd.worldVBO.maxIndex > 65536 ) {
		tess.indexInc = sizeof(GLuint);
	} else {
		tess.indexInc = sizeof(GLushort);
	}
	
	requiredSize = 16 +
		tess.indexInc * tess.numIndexes +
		sizeof(int);

	if ( tess.vertexBuffer ) {
		if ( *(int *)tess.vertexBufferEnd != 0xDEADBEEF) {
			ri.Error( ERR_DROP, "VertexBuffer overflow" );
		}
		if ( tess.vertexBufferEnd - tess.vertexBuffer < requiredSize -sizeof(int) ) {
			ri.Free( tess.vertexBuffer );
			tess.vertexBuffer = tess.vertexBufferEnd = NULL;
		}
	}

	if ( !tess.vertexBuffer ) {
		tess.vertexBuffer = ri.Malloc( requiredSize );
		tess.vertexBufferEnd = tess.vertexBuffer + requiredSize	- sizeof(int);
		*(int *)tess.vertexBufferEnd = 0xDEADBEEF;
	}
	
	tess.vertexPtr1 = NULL;
	tess.indexPtr.p16 = (GLushort *)(((intptr_t)tess.vertexBuffer + 15) & -16);
	
	tess.numVertexes = tess.numIndexes = tess.maxIndex = 0;
}


void RB_NormalToOctDir( vec4_t normal, octDir_t *octDir ) {
	float  scale = Q_fabs(normal[0]) + Q_fabs(normal[1]) + Q_fabs(normal[2]);
	vec3_t octCoords;
	
	VectorScale( normal, 1.0f / scale, octCoords );
	if( octCoords[2] < 0.0f ) {
		if( octCoords[0] < 0.0f )
			octCoords[0] = -1.0f - octCoords[0];
		else
			octCoords[0] =  1.0f - octCoords[0];

		if( octCoords[1] < 0.0f )
			octCoords[1] = -1.0f - octCoords[1];
		else
			octCoords[1] =  1.0f - octCoords[1];
	}
	octDir->x = (short)(32767.0f * octCoords[0]);
	octDir->y = (short)(32767.0f * octCoords[1]);
}

void RB_OctDirToNormal( octDir_t *octDir, vec3_t normal ) {
	normal[0] = octDir->x / 32767.0f;
	normal[1] = octDir->y / 32767.0f;
	normal[2] = 1.0 - Q_fabs(normal[0]) - Q_fabs(normal[1]);

	if( normal[2] < 0.0f ) {
		if( normal[0] < 0.0f )
			normal[0] = -1.0f - normal[0];
		else
			normal[0] =  1.0f - normal[0];

		if( normal[1] < 0.0f )
			normal[1] = -1.0f - normal[1];
		else
			normal[1] =  1.0f - normal[1];
	}		
	VectorNormalizeFast( normal );
}

/*
** RB_VertexArrayToVBO
*
* Compute tangent space vectors and stuff them into the vboVertex.
*/
void RB_VertexArrayToVBO( void )
{
	int      i;
	vec4_t	*accum;
	
	accum = ri.Hunk_AllocateTempMemory( tess.numVertexes * 2 * sizeof(vec4_t) );
	Com_Memset( accum, 0, tess.numVertexes * 2 * sizeof(vec4_t) );
	
	// for every edge sum up the delta xyz / delta st
	for( i = 0; i < tess.numIndexes; i += 3 ) {
		GLuint i1, i2, i3;
		vec4_t	deltaXYZ;
		
		if( tess.indexInc == sizeof(GLushort) ) {
			i1 = tess.indexPtr.p16[i+0];
			i2 = tess.indexPtr.p16[i+1];
			i3 = tess.indexPtr.p16[i+2];
		} else {
			i1 = tess.indexPtr.p32[i+0];
			i2 = tess.indexPtr.p32[i+1];
			i3 = tess.indexPtr.p32[i+2];
		}
		
		VectorSubtract( tess.vertexPtr2[i2].xyz, tess.vertexPtr2[i1].xyz, deltaXYZ );
		if( tess.vertexPtr1[i2].tc1[0] != tess.vertexPtr1[i1].tc1[0] ) {
			deltaXYZ[3] = (tess.vertexPtr1[i2].tc1[0] - tess.vertexPtr1[i1].tc1[0]);
			Vector4Add( deltaXYZ, accum[2*i1+0], accum[2*i1+0] );
			Vector4Add( deltaXYZ, accum[2*i2+0], accum[2*i2+0] );
		}
		if( tess.vertexPtr1[i2].tc1[1] != tess.vertexPtr1[i1].tc1[1] ) {
			deltaXYZ[3] = (tess.vertexPtr1[i2].tc1[1] - tess.vertexPtr1[i1].tc1[1]);
			Vector4Add( deltaXYZ, accum[2*i1+1], accum[2*i1+1] );
			Vector4Add( deltaXYZ, accum[2*i2+1], accum[2*i2+1] );
		}
		
		VectorSubtract( tess.vertexPtr2[i3].xyz, tess.vertexPtr2[i2].xyz, deltaXYZ );
		if( tess.vertexPtr1[i3].tc1[0] != tess.vertexPtr1[i2].tc1[0] ) {
			deltaXYZ[3] = (tess.vertexPtr1[i3].tc1[0] - tess.vertexPtr1[i2].tc1[0]);
			Vector4Add( deltaXYZ, accum[2*i2+0], accum[2*i2+0] );
			Vector4Add( deltaXYZ, accum[2*i3+0], accum[2*i3+0] );
		}
		if( tess.vertexPtr1[i3].tc1[1] != tess.vertexPtr1[i2].tc1[1] ) {
			deltaXYZ[3] = (tess.vertexPtr1[i3].tc1[1] - tess.vertexPtr1[i2].tc1[1]);
			Vector4Add( deltaXYZ, accum[2*i2+1], accum[2*i2+1] );
			Vector4Add( deltaXYZ, accum[2*i3+1], accum[2*i3+1] );
		}
		
		VectorSubtract( tess.vertexPtr2[i1].xyz, tess.vertexPtr2[i3].xyz, deltaXYZ );
		if( tess.vertexPtr1[i1].tc1[0] != tess.vertexPtr1[i3].tc1[0] ) {
			deltaXYZ[3] = (tess.vertexPtr1[i1].tc1[0] - tess.vertexPtr1[i3].tc1[0]);
			Vector4Add( deltaXYZ, accum[2*i3+0], accum[2*i3+0] );
			Vector4Add( deltaXYZ, accum[2*i1+0], accum[2*i1+0] );
		}
		if( tess.vertexPtr1[i1].tc1[1] != tess.vertexPtr1[i3].tc1[1] ) {
			deltaXYZ[3] = (tess.vertexPtr1[i1].tc1[1] - tess.vertexPtr1[i3].tc1[1]);
			Vector4Add( deltaXYZ, accum[2*i3+1], accum[2*i3+1] );
			Vector4Add( deltaXYZ, accum[2*i1+1], accum[2*i1+1] );
		}
	}

	// put the compressed normal and tangents back into the vboVertex
	for( i = 0; i < tess.numVertexes; i++ ) {
		vec4_t	vec;
		vboWord3_t *out = (vboWord3_t *)&tess.vertexPtr3[i];
		
		VectorCopy( tess.vertexPtr3[i].normal, vec );
		// in-place copy
		//out->color[0] = tess.vertexPtr3[i].color[0];
		//out->color[1] = tess.vertexPtr3[i].color[1];
		//out->color[2] = tess.vertexPtr3[i].color[2];
		//out->color[3] = tess.vertexPtr3[i].color[3];

		RB_NormalToOctDir( vec, &out->normal );
		RB_NormalToOctDir( accum[2*i+0], &out->uTangent );
		RB_NormalToOctDir( accum[2*i+1], &out->vTangent );
	}

	ri.Hunk_FreeTempMemory( accum );
}

/*
==================
RB_RenderDrawSurfList
==================
*/
void RB_RenderDrawSurfList( drawSurf_t *drawSurfs, int numDrawSurfs ) {
	shader_t		*shader, *oldShader;
	int				fogNum, oldFogNum;
	int				entityNum, oldEntityNum;
	int				dlighted, oldDlighted;
	qboolean		depthRange, oldDepthRange, isCrosshair, wasCrosshair, worldMatrix;
	int				i, j, k;
	drawSurf_t		*drawSurf;
	int				oldSort, endSort;
	float			originalTime;
	qboolean		isGLSL = qfalse;
	int sortMask = -1 << QSORT_ENTITYNUM_SHIFT; // mask out fog and dlight bits
	
	// save original time for entity shader offsets
	originalTime = backEnd.refdef.floatTime;

	// clear the z buffer, set the modelview, etc
	RB_BeginDrawingView ();

	// draw everything
	oldEntityNum = -1;
	backEnd.currentEntity = &tr.worldEntity;
	oldShader = NULL;
	oldFogNum = -1;
	oldDepthRange = qfalse;
	wasCrosshair = qfalse;
	oldDlighted = qfalse;
	oldSort = -1;
	depthRange = qfalse;

	backEnd.pc.c_surfaces += numDrawSurfs;

	RB_ClearVertexBuffer ();
	
	qglLoadMatrixf( backEnd.viewParms.world.modelMatrix );
	worldMatrix = qtrue;
	
	for (i = 0, drawSurf = drawSurfs ; i < numDrawSurfs ; ) {
		oldSort = drawSurf->sort;
		R_DecomposeSort( drawSurf->sort, &entityNum, &shader, &fogNum, &dlighted );

		//
		// change the tess parameters if needed
		// a "entityMergable" shader is a shader that can have surfaces from seperate
		// entities merged into a single batch, like smoke and blood puff sprites
		if (shader != oldShader || fogNum != oldFogNum || dlighted != oldDlighted 
			|| ( entityNum != oldEntityNum && !shader->entityMergable ) ) {
			RB_BeginSurface( shader, fogNum );
			oldShader = shader;
			oldFogNum = fogNum;
			oldDlighted = dlighted;
			
			// combine sprite entities if possible, they don't use
			// VBOs anyway
			if (//shader->entityMergable &&
			    backEnd.refdef.entities[entityNum].e.reType == RT_SPRITE) {
				sortMask = -1 << QSORT_SHADERNUM_SHIFT;
			} else {
				sortMask = -1 << QSORT_ENTITYNUM_SHIFT;
			}
			isGLSL = (shader->optimalStageIteratorFunc == RB_StageIteratorGLSL);
			oldEntityNum = -1;
		}

		//
		// change the modelview matrix if needed
		//
		if ( entityNum != oldEntityNum ) {
			depthRange = isCrosshair = qfalse;

			if ( entityNum != ENTITYNUM_WORLD ) {
				backEnd.currentEntity = &backEnd.refdef.entities[entityNum];
				backEnd.refdef.floatTime = originalTime - backEnd.currentEntity->e.shaderTime;
				// we have to reset the shaderTime as well otherwise image animations start
				// from the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

				if( !isGLSL ) {
					// set up the transformation matrix
					R_RotateForEntity( backEnd.currentEntity, &backEnd.viewParms, &backEnd.or );
					qglLoadMatrixf( backEnd.or.modelMatrix );
					worldMatrix = qfalse;
					
					// set up the dynamic lighting if needed
					if ( backEnd.currentEntity->needDlights ) {
						R_TransformDlights( backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.or );
					}
				} else if( !worldMatrix ) {
					qglLoadMatrixf( backEnd.viewParms.world.modelMatrix );
					worldMatrix = qtrue;
				}

				if(backEnd.currentEntity->e.renderfx & RF_DEPTHHACK)
				{
					// hack the depth range to prevent view model from poking into walls
					depthRange = qtrue;
					
					if(backEnd.currentEntity->e.renderfx & RF_CROSSHAIR)
						isCrosshair = qtrue;
				}
			} else {
				backEnd.currentEntity = &tr.worldEntity;
				backEnd.refdef.floatTime = originalTime;
				backEnd.or = backEnd.viewParms.world;
				// we have to reset the shaderTime as well otherwise image animations on
				// the world (like water) continue with the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;
				if( !isGLSL ) {
					qglLoadMatrixf( backEnd.or.modelMatrix );
					R_TransformDlights( backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.or );
				}
			}

			//
			// change depthrange. Also change projection matrix so first person weapon does not look like coming
			// out of the screen.
			//
			if (oldDepthRange != depthRange || wasCrosshair != isCrosshair)
			{
				if (depthRange)
				{
					if(backEnd.viewParms.stereoFrame != STEREO_CENTER)
					{
						if(isCrosshair)
						{
							if(oldDepthRange)
							{
								// was not a crosshair but now is, change back proj matrix
								qglMatrixMode(GL_PROJECTION);
								qglLoadMatrixf(backEnd.viewParms.projectionMatrix);
								qglMatrixMode(GL_MODELVIEW);
							}
						}
						else
						{
							viewParms_t temp = backEnd.viewParms;

							R_SetupProjection(&temp, r_znear->value, qfalse);

							qglMatrixMode(GL_PROJECTION);
							qglLoadMatrixf(temp.projectionMatrix);
							qglMatrixMode(GL_MODELVIEW);
						}
					}

					if(!oldDepthRange)
						qglDepthRange (0, 0.3);
				}
				else
				{
					if(!wasCrosshair && backEnd.viewParms.stereoFrame != STEREO_CENTER)
					{
						qglMatrixMode(GL_PROJECTION);
						qglLoadMatrixf(backEnd.viewParms.projectionMatrix);
						qglMatrixMode(GL_MODELVIEW);
					}

					qglDepthRange (0, 1);
				}

				oldDepthRange = depthRange;
				wasCrosshair = isCrosshair;
			}

			oldEntityNum = entityNum;
		}

		// look if we can use a VBO for this shader/entity
		int VBOkey = 0;
		vboInfo_t **VBOtree = &shader->VBOs;
		
		if ( shader->useVBO ) {
			if (entityNum == ENTITYNUM_WORLD) {
				if ( backEnd.viewParms.viewCluster >= 0 ) {
					VBOkey = VBOKEY_VIS | ((backEnd.viewParms.frustType << 24 | backEnd.viewParms.viewCluster) & VBOKEY_IDXMASK);
				}
			} else {
				trRefEntity_t	*ent = backEnd.currentEntity;
				if ( ent->e.reType == RT_MODEL ) {
					model_t *model = R_GetModelByHandle( ent->e.hModel );
					if ( model->type == MOD_BRUSH ) {
						VBOkey = VBOKEY_MODEL | (ent->e.hModel & VBOKEY_IDXMASK);
					} else if ( model->type == MOD_MESH &&
						    shader == tr.occlusionTestShader ) {
						VBOkey = VBOKEY_IMPOSTOR | (ent->e.hModel << 8);
					} else if ( model->type == MOD_MESH &&
						    shader->lightmapIndex == LIGHTMAP_MD3 &&
						    isGLSL ) {
						VBOkey = ((md3Surface_t *)drawSurf->surface)->flags;
						VBOtree = &model->VBOs[0];
					} else if ( model->type == MOD_MESH &&
						ent->e.frame == ent->e.oldframe ) {
						// combine hModel and frame number into key
						// allows 65536 models with 256 frames
						VBOkey = VBOKEY_MD3 | (ent->e.hModel << 8) | ent->e.frame;
					}
				}
			}
		}

		if ( (VBOkey & VBOKEY_TYPEMASK) == VBOKEY_MD3_KEYS ) {
			// add one VBO per surface
			endSort = (oldSort - sortMask) & sortMask;

			k = i;
			for ( j = 0; j < numDrawSurfs - i ; j++ ) {
				if ((drawSurf+j)->sort >= endSort)
					break;
				
				findShaderVBO( VBOtree, ((md3Surface_t *)(drawSurf+j)->surface)->flags );
				(*VBOtree)->next = tess.firstVBO;
				tess.firstVBO = *VBOtree;
				
				if ( ((drawSurf+j)->sort & ~sortMask) == 0 )
					k++;
			}
			i += j;
			drawSurf += j;
			VBOkey = 0;
		} else if ( VBOkey == 0 || !findShaderVBO( VBOtree, VBOkey) ) {
			// build a vertex buffer
			RB_ClearVertexBuffer( );

			// we also want to collect dynamically lit
			// vertexes, even if we have to collect them twice
			endSort = (oldSort - sortMask) & sortMask;
			
			k = i;
			for ( j = 0; j < numDrawSurfs - i ; j++ ) {
				if ((drawSurf+j)->sort >= endSort)
					break;
				if ( ((drawSurf+j)->sort & ~sortMask) == 0 )
					k++;
				rb_surfaceTable[ *(drawSurf+j)->surface ]( (drawSurf+j)->surface );
			}
			
			if ( tess.numVertexes == 0 || tess.numIndexes == 0 ) {
				i += j;
				drawSurf += j;
				continue;
			}

			if ( shader->useVBO &&
			     entityNum == ENTITYNUM_WORLD &&
			     backEnd.worldVBO.vbo ) {
				// we can use vertex data from the world VBO
				RB_SetupIndexBuffer( );
			} else {
				RB_SetupVertexBuffer(shader);
			}
			for ( ; j > 0; j--, i++, drawSurf++ ) {
				R_DecomposeSort( drawSurf->sort, &entityNum, &shader, &fogNum, &dlighted );
				tess.fogNum = fogNum;
				if ( oldEntityNum != entityNum ) {
					backEnd.currentEntity = &backEnd.refdef.entities[entityNum];
					backEnd.refdef.floatTime = originalTime - backEnd.currentEntity->e.shaderTime;
					// we have to reset the shaderTime as well otherwise image animations start
					// from the wrong frame
					tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;
					
					// set up the transformation matrix
					R_RotateForEntity( backEnd.currentEntity, &backEnd.viewParms, &backEnd.or );
					
					// set up the dynamic lighting if needed
					if ( backEnd.currentEntity->needDlights ) {
						R_TransformDlights( backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.or );
					}
					
					oldEntityNum = entityNum;
					
				}
				rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
			}

			if( tess.vertexPtr1 && isGLSL &&
			    !backEnd.currentEntity->e.reType == RT_SPRITE ) {
				RB_VertexArrayToVBO( );
			}
			
			if ( VBOkey > 0 ) {
				// create new VBO
				
				vboInfo_t *vbo;
				
				newShaderVBO( VBOtree, VBOkey );
				vbo = *VBOtree;

				// allocate VBOs
				qglGenBuffersARB(1, &vbo->ibo);
				if ( entityNum == ENTITYNUM_WORLD ) {
					vbo->vbo = 0;
					GL_VBO( backEnd.worldVBO.vbo, vbo->ibo );
					vbo->minIndex = tess.minIndex;
					vbo->maxIndex = tess.maxIndex;
				} else {
					qglGenBuffersARB(1, &vbo->vbo);
					GL_VBO( vbo->vbo, vbo->ibo );
					
					tess.numVertexes = (tess.numVertexes + 3) & -4;
					qglBufferDataARB(GL_ARRAY_BUFFER_ARB,
							 tess.numVertexes * (sizeof(vaWord1_t) + sizeof(vaWord2_t) + sizeof(vboWord3_t)),
							 tess.vertexPtr1, GL_STATIC_DRAW_ARB);
					vbo->minIndex = tess.minIndex;
					vbo->maxIndex = tess.maxIndex;
					vbo->offs1 = (vaWord1_t *)NULL;
					vbo->offs2 = (vaWord2_t *)(vbo->offs1 + tess.numVertexes);
					vbo->offs3 = (vboWord3_t *)(vbo->offs2 + tess.numVertexes);
				}
								
				qglBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB,
						 tess.indexInc * tess.numIndexes,
						 tess.indexPtr.p16, GL_STATIC_DRAW_ARB);
			
				vbo->key = VBOkey;
				vbo->numIndexes = tess.numIndexes;

				RB_ClearVertexBuffer ();
			}
		} else {
			// find surfaces requiring fog or dlight
			// and skip surfaces contained in VBO
			endSort = (oldSort - sortMask) & sortMask;

			k = i;
			for ( j = 0; j < numDrawSurfs - i ; j++ ) {
				if ((drawSurf+j)->sort >= endSort)
					break;
				if ( ((drawSurf+j)->sort & ~sortMask) == 0 )
					k++;
			}
			i += j;
			drawSurf += j;
		}

		if (VBOkey > 0) {
			// add VBO
			(*VBOtree)->next = tess.firstVBO;
			tess.firstVBO = *VBOtree;
		}
		
		RB_EndSurface();
		RB_ClearVertexBuffer ();
		
		if( 1 || !isGLSL ) {
			// remaining surfaces have to be dlighted or fogged
			while ( k < i ) {
				drawSurf = drawSurfs + k;
				
				oldSort = drawSurf->sort;
				R_DecomposeSort( drawSurf->sort, &entityNum, &shader, &fogNum, &dlighted );
				RB_BeginSurface( shader, fogNum );
				for ( j = 0; j < numDrawSurfs - k ; j++ ) {
					if ((drawSurf+j)->sort != oldSort)
						break;
					rb_surfaceTable[ *(drawSurf+j)->surface ]( (drawSurf+j)->surface );
				}
				
				if ( tess.numVertexes > 0 && tess.numIndexes > 0 ) {
					RB_SetupVertexBuffer(shader);
					for ( ; j > 0 ; j--, k++, drawSurf++ ) {
						rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
					}
					RB_LightSurface();
				} else {
					k += j;
				}
				RB_ClearVertexBuffer ();
			}
		}
	}

	backEnd.refdef.floatTime = originalTime;

	GL_VBO( 0, 0 );

	// go back to the world modelview matrix
	qglLoadMatrixf( backEnd.viewParms.world.modelMatrix );
	if ( depthRange ) {
		qglDepthRange (0, 1);
	}

#if 0
	RB_DrawSun();
#endif
	// darken down any stencil shadows
	RB_ShadowFinish();

	// add light flares on lights that aren't obscured
	RB_RenderFlares();
}


/*
============================================================================

RENDER BACK END THREAD FUNCTIONS

============================================================================
*/

/*
================
RB_SetGL2D

================
*/
void	RB_SetGL2D (void) {
	backEnd.projection2D = qtrue;

	// set 2D virtual screen size
	qglViewport( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	qglScissor( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	qglMatrixMode(GL_PROJECTION);
    qglLoadIdentity ();
	qglOrtho (0, glConfig.vidWidth, glConfig.vidHeight, 0, 0, 1);
	qglMatrixMode(GL_MODELVIEW);
    qglLoadIdentity ();

	GL_State( GLS_DEPTHTEST_DISABLE |
			  GLS_SRCBLEND_SRC_ALPHA |
			  GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );

	qglDisable( GL_CULL_FACE );
	qglDisable( GL_CLIP_PLANE0 );

	// set time for 2D shaders
	backEnd.refdef.time = ri.Milliseconds();
	backEnd.refdef.floatTime = backEnd.refdef.time * 0.001f;
}


/*
=============
RE_StretchRaw

FIXME: not exactly backend
Stretches a raw 32 bit power of 2 bitmap image over the given screen rectangle.
Used for cinematics.
=============
*/
void RE_StretchRaw (int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty) {
	int			i, j;
	int			start, end;
	vec2_t			texCoords[4], vertexes[4];

	if ( !tr.registered ) {
		return;
	}
	R_SyncRenderThread();

	// we definately want to sync every frame for the cinematics
	qglFinish();

	start = end = 0;
	if ( r_speeds->integer ) {
		start = ri.Milliseconds();
	}

	// make sure rows and cols are powers of 2
	for ( i = 0 ; ( 1 << i ) < cols ; i++ ) {
	}
	for ( j = 0 ; ( 1 << j ) < rows ; j++ ) {
	}
	if ( ( 1 << i ) != cols || ( 1 << j ) != rows) {
		ri.Error (ERR_DROP, "Draw_StretchRaw: size not a power of 2: %i by %i", cols, rows);
	}

	GL_BindImages( 1, &tr.scratchImage[client] );

	// if the scratchImage isn't in the format we want, specify it as a new texture
	if ( cols != tr.scratchImage[client]->width || rows != tr.scratchImage[client]->height ) {
		tr.scratchImage[client]->width = tr.scratchImage[client]->uploadWidth = cols;
		tr.scratchImage[client]->height = tr.scratchImage[client]->uploadHeight = rows;
		qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGB8, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );	
	} else {
		if (dirty) {
			// otherwise, just subimage upload it so that drivers can tell we are going to be changing
			// it and don't try and do a texture compression
			qglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data );
		}
	}

	if ( r_speeds->integer ) {
		end = ri.Milliseconds();
		ri.Printf( PRINT_ALL, "qglTexSubImage2D %i, %i: %i msec\n", cols, rows, end - start );
	}

	RB_SetGL2D();
	GL_Program( 0 );
	GL_VBO( 0, 0 );

	GL_Color4f( tr.identityLight, tr.identityLight, tr.identityLight, 1.0f );

	texCoords[0][0] = 0.5f / cols;        texCoords[0][1] = 0.5f / rows;
	vertexes[0][0] = x;                   vertexes[0][1] = y;
	texCoords[1][0] = 1.0f - 0.5f / cols; texCoords[1][1] = 0.5f / rows;
	vertexes[1][0] = x+w;                 vertexes[1][1] = y;
	texCoords[2][0] = 1.0f - 0.5f / cols; texCoords[2][1] = 1.0f - 0.5f / rows;
	vertexes[2][0] = x+w;                 vertexes[2][1] = y+h;
	texCoords[3][0] = 0.5f / cols;        texCoords[3][1] = 1.0f - 0.5f / rows;
	vertexes[3][0] = x;                   vertexes[3][1] = y+h;
	qglVertexPointer( 2, GL_FLOAT, 0, vertexes );
	GL_TexCoordPtr( 0, 2, GL_FLOAT, 0, texCoords[0] );
	qglDrawArrays( GL_QUADS, 0, 4 );
}

void RE_UploadCinematic (int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty) {

	GL_BindImages( 1, &tr.scratchImage[client] );

	// if the scratchImage isn't in the format we want, specify it as a new texture
	if ( cols != tr.scratchImage[client]->width || rows != tr.scratchImage[client]->height ) {
		tr.scratchImage[client]->width = tr.scratchImage[client]->uploadWidth = cols;
		tr.scratchImage[client]->height = tr.scratchImage[client]->uploadHeight = rows;
		qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGB8, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );	
	} else {
		if (dirty) {
			// otherwise, just subimage upload it so that drivers can tell we are going to be changing
			// it and don't try and do a texture compression
			qglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data );
		}
	}
}


/*
=============
RB_SetColor

=============
*/
const void	*RB_SetColor( const void *data ) {
	const setColorCommand_t	*cmd;

	cmd = (const setColorCommand_t *)data;

	backEnd.color2D[0] = cmd->color[0] * 255;
	backEnd.color2D[1] = cmd->color[1] * 255;
	backEnd.color2D[2] = cmd->color[2] * 255;
	backEnd.color2D[3] = cmd->color[3] * 255;

	return (const void *)(cmd + 1);
}

/*
=============
RB_StretchPic
=============
*/
const void *RB_StretchPic ( const void *data ) {
	const stretchPicCommand_t	*cmd;
	shader_t	*shader;
	GLushort	indexes[6] = { 3, 0, 2, 2, 0, 1 };
	int		i, n;

	cmd = (const stretchPicCommand_t *)data;

	if ( !backEnd.projection2D ) {
		RB_SetGL2D();
	}

	shader = cmd->shader;
	backEnd.currentEntity = &backEnd.entity2D;
	RB_BeginSurface( shader, 0 );

	// check if the following StretchPic commands use the same shader
	n = 1;
	data = cmd+1;
	for(;;) {
		if ( *(int *)data == RC_STRETCH_PIC &&
		     ((stretchPicCommand_t *)data)->shader == shader ) {
			n++;
			data = data + sizeof(stretchPicCommand_t);
		} else if ( *(int *)data == RC_SET_COLOR ) {
			data = data + sizeof(setColorCommand_t);
		} else
			break;
 	}
	tess.numVertexes = 4 * n;
	tess.numIndexes  = 6 * n;

	RB_SetupVertexBuffer( shader );

	tess.numVertexes = 4 * n;
	tess.numIndexes  = 6 * n;
	tess.minIndex = 0;
	tess.maxIndex = 4 * n - 1;

	for ( i = 0; ; ) {
		if ( cmd->commandId == RC_STRETCH_PIC &&
		     cmd->shader == shader ) {
			tess.indexPtr.p16[6*i+0] = indexes[0] + 4*i;
			tess.indexPtr.p16[6*i+1] = indexes[1] + 4*i;
			tess.indexPtr.p16[6*i+2] = indexes[2] + 4*i;
			tess.indexPtr.p16[6*i+3] = indexes[3] + 4*i;
			tess.indexPtr.p16[6*i+4] = indexes[4] + 4*i;
			tess.indexPtr.p16[6*i+5] = indexes[5] + 4*i;
			
			*(int *)(&tess.vertexPtr3[4*i+0].color) =
			*(int *)(&tess.vertexPtr3[4*i+1].color) =
			*(int *)(&tess.vertexPtr3[4*i+2].color) =
			*(int *)(&tess.vertexPtr3[4*i+3].color) = *(int *)backEnd.color2D;
			
			tess.vertexPtr2[4*i+0].xyz[0] = cmd->x;
			tess.vertexPtr2[4*i+0].xyz[1] = cmd->y;
			tess.vertexPtr2[4*i+0].xyz[2] = 0.0f;
			tess.vertexPtr2[4*i+0].fogNum = 1.0f;
			tess.vertexPtr1[4*i+0].tc1[0] = cmd->s1;
			tess.vertexPtr1[4*i+0].tc1[1] = cmd->t1;
			
			tess.vertexPtr2[4*i+1].xyz[0] = cmd->x + cmd->w;
			tess.vertexPtr2[4*i+1].xyz[1] = cmd->y;
			tess.vertexPtr2[4*i+1].xyz[2] = 0.0f;
			tess.vertexPtr2[4*i+1].fogNum = 1.0f;
			tess.vertexPtr1[4*i+1].tc1[0] = cmd->s2;
			tess.vertexPtr1[4*i+1].tc1[1] = cmd->t1;
			
			tess.vertexPtr2[4*i+2].xyz[0] = cmd->x + cmd->w;
			tess.vertexPtr2[4*i+2].xyz[1] = cmd->y + cmd->h;
			tess.vertexPtr2[4*i+2].xyz[2] = 0.0f;
			tess.vertexPtr2[4*i+2].fogNum = 1.0f;
			tess.vertexPtr1[4*i+2].tc1[0] = cmd->s2;
			tess.vertexPtr1[4*i+2].tc1[1] = cmd->t2;
			
			tess.vertexPtr2[4*i+3].xyz[0] = cmd->x;
			tess.vertexPtr2[4*i+3].xyz[1] = cmd->y + cmd->h;
			tess.vertexPtr2[4*i+3].xyz[2] = 0.0f;
			tess.vertexPtr2[4*i+3].fogNum = 1.0f;
			tess.vertexPtr1[4*i+3].tc1[0] = cmd->s1;
			tess.vertexPtr1[4*i+3].tc1[1] = cmd->t2;
			
			cmd++;
			i++;
		} else if ( cmd->commandId == RC_SET_COLOR ) {
			cmd = RB_SetColor( cmd );
		} else
			break;
	}
	RB_EndSurface ();
	RB_ClearVertexBuffer ();

	return (const void *)cmd;
}


/*
=============
RB_DrawSurfs

=============
*/
const void	*RB_DrawSurfs( const void *data ) {
	const drawSurfsCommand_t	*cmd;

	// finish any 2D drawing if needed
	if ( tess.numIndexes || tess.firstVBO ) {
		RB_EndSurface();
	}

	cmd = (const drawSurfsCommand_t *)data;

	backEnd.refdef = cmd->refdef;
	backEnd.viewParms = cmd->viewParms;

	RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );

	return (const void *)(cmd + 1);
}


/*
=============
RB_DrawBuffer

=============
*/
const void	*RB_DrawBuffer( const void *data ) {
	const drawBufferCommand_t	*cmd;

	cmd = (const drawBufferCommand_t *)data;

	qglDrawBuffer( cmd->buffer );

	// clear screen for debugging
	if ( r_clear->integer ) {
		qglClearColor( 1, 0, 0.5, 1 );
		qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	}

	return (const void *)(cmd + 1);
}

/*
===============
RB_ShowImages

Draw all the images to the screen, on top of whatever
was there.  This is used to test for texture thrashing.

Also called by RE_EndRegistration
===============
*/
void RB_ShowImages( void ) {
	int		i;
	image_t	*image;
	float	x, y, w, h;
	int		start, end;
	vec2_t	vertexes[4], texCoords[4];

	if ( !backEnd.projection2D ) {
		RB_SetGL2D();
	}

	qglClear( GL_COLOR_BUFFER_BIT );

	qglFinish();

	start = ri.Milliseconds();

	texCoords[0][0] = 0.0f; texCoords[0][1] = 0.0f;
	texCoords[1][0] = 1.0f; texCoords[1][1] = 0.0f;
	texCoords[2][0] = 1.0f; texCoords[2][1] = 1.0f;
	texCoords[3][0] = 0.0f; texCoords[3][1] = 1.0f;

	GL_Program( 0 );
	
	GL_TexCoordPtr( 0, 2, GL_FLOAT, 0, texCoords[0] );

	for ( i=0 ; i<tr.numImages ; i++ ) {
		image = tr.images[i];

		w = glConfig.vidWidth / 40;
		h = glConfig.vidHeight / 30;
		x = i % 40 * w;
		y = i / 40 * h;

		// show in proportional size in mode 2
		if ( r_showImages->integer == 2 ) {
			w *= image->uploadWidth / 512.0f;
			h *= image->uploadHeight / 512.0f;
		}

		GL_BindImages( 1, &image );
		vertexes[0][0] = x;   vertexes[0][1] = y;
		vertexes[1][0] = x+w; vertexes[1][1] = y;
		vertexes[2][0] = x+w; vertexes[2][1] = y+h;
		vertexes[3][0] = x;   vertexes[3][1] = y+h;
		qglVertexPointer( 2, GL_FLOAT, 0, vertexes );
		qglDrawArrays( GL_QUADS, 0, 4 );
	}

	qglFinish();

	end = ri.Milliseconds();
	ri.Printf( PRINT_ALL, "%i msec to draw all images\n", end - start );
}

/*
=============
RB_ColorMask

=============
*/
const void *RB_ColorMask(const void *data)
{
	const colorMaskCommand_t *cmd = data;
	
	qglColorMask(cmd->rgba[0], cmd->rgba[1], cmd->rgba[2], cmd->rgba[3]);
	
	return (const void *)(cmd + 1);
}

/*
=============
RB_ClearDepth

=============
*/
const void *RB_ClearDepth(const void *data)
{
	const clearDepthCommand_t *cmd = data;
	
	if(tess.numIndexes || tess.firstVBO )
		RB_EndSurface();

	// texture swapping test
	if (r_showImages->integer)
		RB_ShowImages();

	qglClear(GL_DEPTH_BUFFER_BIT);
	
	return (const void *)(cmd + 1);
}

/*
=============
RB_SwapBuffers

=============
*/
const void	*RB_SwapBuffers( const void *data ) {
	const swapBuffersCommand_t	*cmd;

	// finish any 2D drawing if needed
	if ( tess.numIndexes || tess.firstVBO ) {
		RB_EndSurface();
	}

	// texture swapping test
	if ( r_showImages->integer ) {
		RB_ShowImages();
	}

	cmd = (const swapBuffersCommand_t *)data;

	// we measure overdraw by reading back the stencil buffer and
	// counting up the number of increments that have happened
	if ( r_measureOverdraw->integer ) {
		int i;
		long sum = 0;
		unsigned char *stencilReadback;

		stencilReadback = ri.Hunk_AllocateTempMemory( glConfig.vidWidth * glConfig.vidHeight );
		qglReadPixels( 0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, stencilReadback );

		for ( i = 0; i < glConfig.vidWidth * glConfig.vidHeight; i++ ) {
			sum += stencilReadback[i];
		}

		backEnd.pc.c_overDraw += sum;
		ri.Hunk_FreeTempMemory( stencilReadback );
	}

	if ( !glState.finishCalled ) {
		qglFinish();
	}

	GLimp_LogComment( "***************** RB_SwapBuffers *****************\n\n\n" );

	GLimp_EndFrame();

	backEnd.projection2D = qfalse;

	return (const void *)(cmd + 1);
}

/*
====================
RB_ExecuteRenderCommands

This function will be called synchronously if running without
smp extensions, or asynchronously by another thread.
====================
*/
void RB_ExecuteRenderCommands( const void *data ) {
	int		t1, t2;

	t1 = ri.Milliseconds ();

	if ( !r_smp->integer || data == backEndData[0]->commands.cmds ) {
		backEnd.smpFrame = 0;
	} else {
		backEnd.smpFrame = 1;
	}

	while ( 1 ) {
		switch ( *(const int *)data ) {
		case RC_SET_COLOR:
			data = RB_SetColor( data );
			break;
		case RC_STRETCH_PIC:
			data = RB_StretchPic( data );
			break;
		case RC_DRAW_SURFS:
			data = RB_DrawSurfs( data );
			break;
		case RC_DRAW_BUFFER:
			data = RB_DrawBuffer( data );
			break;
		case RC_SWAP_BUFFERS:
			data = RB_SwapBuffers( data );
			break;
		case RC_SCREENSHOT:
			data = RB_TakeScreenshotCmd( data );
			break;
		case RC_VIDEOFRAME:
			data = RB_TakeVideoFrameCmd( data );
			break;
		case RC_COLORMASK:
			data = RB_ColorMask(data);
			break;
		case RC_CLEARDEPTH:
			data = RB_ClearDepth(data);
			break;
		case RC_END_OF_LIST:
		default:
			// stop rendering on this thread
			t2 = ri.Milliseconds ();
			backEnd.pc.msec = t2 - t1;
			return;
		}
	}

}


/*
================
RB_RenderThread
================
*/
void RB_RenderThread( void ) {
	const void	*data;

	// wait for either a rendering command or a quit command
	while ( 1 ) {
		// sleep until we have work to do
		data = GLimp_RendererSleep();

		if ( !data ) {
			return;	// all done, renderer is shutting down
		}

		renderThreadActive = qtrue;

		RB_ExecuteRenderCommands( data );

		renderThreadActive = qfalse;
	}
}

