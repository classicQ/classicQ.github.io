/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// gl_local.h -- private refresh defs

// disable data conversion warnings

#ifdef __MACOSX__
#if 0
#include <AGL/agl.h>
#endif

#include <OpenGL/gl.h>

#else
#include <GL/gl.h>
#endif

#ifdef AROS
#include <proto/mesa.h>
#endif

#include "gl_texture.h"
#include "gl_model.h"

#include "render.h"
#include "protocol.h"
#include "client.h"

#ifndef APIENTRY
#define APIENTRY
#endif

extern int glwidth, glheight;

#define ALIAS_BASE_SIZE_RATIO		(1.0 / 11.0)
					// normalizing factor so player model works out to about
					//  1 pixel per triangle
#define	MAX_LBM_HEIGHT		480

#define SKYSHIFT		7
#define	SKYSIZE			(1 << SKYSHIFT)
#define SKYMASK			(SKYSIZE - 1)

#define BACKFACE_EPSILON	0.01

#ifdef FOD_BIGENDIAN
#define COLOURMASK_RGBA 0xffffff00
#else
#define COLOURMASK_RGBA 0x00ffffff
#endif


void R_InitGL(void);
void R_TimeRefresh_f (void);

//====================================================


int QMB_InitParticles(void);
void QMB_ShutdownParticles();
void QMB_ClearParticles(void);
void QMB_DrawParticles(void);

void QMB_RunParticleEffect(const vec3_t org, const vec3_t dir, int color, int count);
void QMB_ParticleTrail (vec3_t start, vec3_t end, vec3_t *, trail_type_t type);
void QMB_BlobExplosion (vec3_t org);
void QMB_ParticleExplosion (vec3_t org);
void QMB_LavaSplash (vec3_t org);
void QMB_TeleportSplash (vec3_t org);

void QMB_DetpackExplosion (vec3_t org);

void QMB_InfernoFlame (vec3_t org);
void QMB_StaticBubble (entity_t *ent);

extern qboolean qmb_initialized;

void GL_Particles_CvarInit(void);
void GL_Particles_TextureInit(void);

void Classic_LoadParticleTextures(void);

//====================================================

extern	entity_t	r_worldentity;
extern	qboolean	r_cache_thrash;		// compatability
extern	vec3_t		modelorg, r_entorigin;
extern	entity_t	*currententity;
extern	int			r_visframecount;
extern	int			r_framecount;
extern	mplane_t	frustum[4];
extern	int			c_brush_polys, c_alias_polys;

// view origin
extern	vec3_t	vup;
extern	vec3_t	vpn;
extern	vec3_t	vright;
extern	vec3_t	r_origin;

// screen size info
extern	refdef_t	r_refdef;
extern	mleaf_t		*r_viewleaf, *r_oldviewleaf;
extern	mleaf_t		*r_viewleaf2, *r_oldviewleaf2;	// for watervis hack
extern	texture_t	*r_notexture_mip;
extern	int			d_lightstylevalue[256];	// 8.8 fraction of base light value

extern	int	particletexture;
extern	int	netgraphtexture;
extern	int	playertextures;
extern	int	playerfbtextures[MAX_CLIENTS];
extern	int	skyboxtextures;
extern	int underwatertexture, detailtexture;

#include "gl_cvars.h"

extern	int		lightmode;		// set to gl_lightmode on mapchange

extern	const char *gl_vendor;
extern	const char *gl_renderer;
extern	const char *gl_version;
extern	const char *gl_extensions;

// gl_warp.c
void GL_SubdivideSurface(model_t *model, msurface_t *fa);
void EmitBothSkyLayers (msurface_t *fa);
void EmitWaterPolys(model_t *model, msurface_t *fa);
void EmitCausticsPolys (void);
void R_DrawSkyChain (void);
void R_LoadSky_f(void);
void R_DrawSkyBox (void);
extern qboolean	r_skyboxloaded;

// gl_draw.c
void GL_Set2D (void);
void Draw_SizeChanged(void);

// gl_rmain.c
qboolean R_CullBox (vec3_t mins, vec3_t maxs);
qboolean R_CullSphere (vec3_t centre, float radius);
void R_PolyBlend (void);
void R_BrightenScreen (void);
void R_DrawEntitiesOnList (visentlist_t *vislist);

// gl_rlight.c
void R_MarkLights(model_t *model, dlight_t *light, int bit, unsigned int nodenum);
void R_AnimateLight (void);
void R_RenderDlights (void);
int R_LightPoint (vec3_t p);

// gl_refrag.c
void R_StoreEfrags (efrag_t **ppefrag);

// gl_mesh.c
void GL_MakeAliasModelDisplayLists (model_t *m, aliashdr_t *hdr);

// gl_rsurf.c
void R_DrawBrushModel (entity_t *e);
void R_DrawWorld (void);
void R_DrawWaterSurfaces (void);
void GL_BuildLightmaps (void);

// gl_ngraph.c
void R_NetGraph (void);

// gl_rmisc.c
void R_InitOtherTextures(void);

//vid_common_gl.c

//texture compression
#define GL_COMPRESSED_ALPHA_ARB					0x84E9
#define GL_COMPRESSED_LUMINANCE_ARB				0x84EA
#define GL_COMPRESSED_LUMINANCE_ALPHA_ARB		0x84EB
#define GL_COMPRESSED_INTENSITY_ARB				0x84EC
#define GL_COMPRESSED_RGB_ARB					0x84ED
#define GL_COMPRESSED_RGBA_ARB					0x84EE
#define GL_TEXTURE_COMPRESSION_HINT_ARB			0x84EF
#define GL_TEXTURE_IMAGE_SIZE_ARB				0x86A0
#define GL_TEXTURE_COMPRESSED_ARB				0x86A1
#define GL_NUM_COMPRESSED_TEXTURE_FORMATS_ARB	0x86A2
#define GL_COMPRESSED_TEXTURE_FORMATS_ARB		0x86A3

//combine extension
#define GL_COMBINE_EXT				0x8570
#define GL_COMBINE_RGB_EXT			0x8571
#define GL_RGB_SCALE_EXT			0x8573

/* GL_ARB_vertex_buffer_object */
#define GL_ARRAY_BUFFER_ARB                             0x8892
#define GL_STATIC_DRAW_ARB                              0x88E4

#ifdef _WIN32
#define GL_CLAMP_TO_EDGE 0x812F

#define GL_MAX_TEXTURE_UNITS 0x84E2
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_TEXTURE2 0x84C2
#endif

#ifdef __MORPHOS__
#define GL_MAX_TEXTURE_UNITS GL_MAX_TEXTURE_UNITS_ARB

#define glMultiTexCoord2f glMultiTexCoord2fARB
#define glClientActiveTexture glClientActiveTextureARB
#define glActiveTexture glActiveTextureARB
#endif

#ifdef _WIN32
void glMultiTexCoord2f(GLenum target, GLfloat s, GLfloat t);
void glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices);
void glClientActiveTexture(GLenum texture);
void glActiveTexture(GLenum texture);
#endif

#if !defined(GL_VERSION_1_5) && !defined(__MORPHOS__)
typedef ptrdiff_t GLintptrARB;
typedef ptrdiff_t GLsizeiptrARB;
#endif

extern void (APIENTRY *qglBindBufferARB)(GLenum, GLuint);
extern void (APIENTRY *qglBufferDataARB)(GLenum, GLsizeiptrARB, const GLvoid *, GLenum);
extern void (APIENTRY *qglBufferSubDataARB)(GLenum, GLintptrARB, GLsizeiptrARB, const GLvoid *);

/* GLSL stuff */
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84

#if !defined(GL_VERSION_2_0) && !defined(__MORPHOS__)
typedef char GLchar;
#endif

extern void (APIENTRY *qglAttachShader)(GLuint program, GLuint shader);
extern void (APIENTRY *qglCompileShader)(GLuint shader);
extern GLuint (APIENTRY *qglCreateProgram)(void);
extern GLuint (APIENTRY *qglCreateShader)(GLenum shaderType);
extern void (APIENTRY *qglDeleteProgram)(GLuint program);
extern void (APIENTRY *qglDeleteShader)(GLuint shader);
extern void (APIENTRY *qglGetProgramInfoLog)(GLuint program, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
extern void (APIENTRY *qglGetProgramiv)(GLuint program,  GLenum pname, GLint *params);
extern void (APIENTRY *qglGetShaderInfoLog)(GLuint shader, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
extern void (APIENTRY *qglGetShaderiv)(GLuint shader,  GLenum pname, GLint *params);
extern GLint (APIENTRY *qglGetUniformLocation)(GLuint program, const GLchar *name);
extern void (APIENTRY *qglLinkProgram)(GLuint program);
extern void (APIENTRY *qglShaderSource)(GLuint shader, GLsizei count, const GLchar **string, const GLint *length);
extern void (APIENTRY *qglUniform1f)(GLint location, GLfloat v0);
extern void (APIENTRY *qglUniform1i)(GLint location, GLint v0);
extern void (APIENTRY *qglUniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
extern void (APIENTRY *qglUseProgram)(GLuint program);

extern void (APIENTRY *qglBindAttribLocation)(GLuint program, GLuint index, const GLchar *name);
extern void (APIENTRY *qglDisableVertexAttribArray)(GLuint index);
extern void (APIENTRY *qglEnableVertexAttribArray)(GLuint index);
extern void (APIENTRY *qglVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);



extern float gldepthmin, gldepthmax;
extern byte color_white[4], color_black[4];
extern qboolean gl_mtexable;
extern int gl_textureunits;
extern qboolean gl_combine, gl_add_ext, gl_npot, gl_vbo, gl_vs, gl_fs;

extern int vbo_number;

qboolean CheckExtension (const char *extension);
void Check_Gamma (unsigned char *pal);
void VID_SetPalette (unsigned char *palette);
void GL_CvarInit(void);
void GL_Init (void);

