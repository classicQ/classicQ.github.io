/*
Copyright (C) 2002-2003 A Nourai

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

// vid_common_gl.c -- Common code for vid_wgl.c and vid_glx.c

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "quakedef.h"
#include "gl_local.h"
#include "gl_state.h"

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;
unsigned int gl_version_number;
unsigned int gl_revision_number;

qboolean gl_mtexable = false;
int gl_textureunits = 1;

/* VBO stuff */
void (APIENTRY *qglBindBufferARB)(GLenum, GLuint);
void (APIENTRY *qglBufferDataARB)(GLenum, GLsizeiptrARB, const GLvoid *, GLenum);
void (APIENTRY *qglBufferSubDataARB)(GLenum, GLintptrARB, GLsizeiptrARB, const GLvoid *);

/* GLSL stuff */
void (APIENTRY *qglAttachShader)(GLuint program, GLuint shader);
void (APIENTRY *qglCompileShader)(GLuint shader);
GLuint (APIENTRY *qglCreateProgram)(void);
GLuint (APIENTRY *qglCreateShader)(GLenum shaderType);
void (APIENTRY *qglDeleteProgram)(GLuint program);
void (APIENTRY *qglDeleteShader)(GLuint shader);
void (APIENTRY *qglGetProgramInfoLog)(GLuint program, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
void (APIENTRY *qglGetProgramiv)(GLuint program, GLenum pname, GLint *params);
void (APIENTRY *qglGetShaderInfoLog)(GLuint shader, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
void (APIENTRY *qglGetShaderiv)(GLuint shader, GLenum pname, GLint *params);
GLint (APIENTRY *qglGetUniformLocation)(GLuint program, const GLchar *name);
void (APIENTRY *qglLinkProgram)(GLuint program);
void (APIENTRY *qglShaderSource)(GLuint shader, GLsizei count, const GLchar **string, const GLint *length);
void (APIENTRY *qglUniform1f)(GLint location, GLfloat v0);
void (APIENTRY *qglUniform1i)(GLint location, GLint v0);
void (APIENTRY *qglUniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void (APIENTRY *qglUseProgram)(GLuint program);

void (APIENTRY *qglBindAttribLocation)(GLuint program, GLuint index, const GLchar *name);
void (APIENTRY *qglDisableVertexAttribArray)(GLuint index);
void (APIENTRY *qglEnableVertexAttribArray)(GLuint index);
void (APIENTRY *qglVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);


/* GLSL stuff, ARB version */
#if !defined(GL_ARB_shader_objects) && !defined(__MORPHOS__)
typedef char GLcharARB;
typedef unsigned int GLhandleARB;
#endif

void (*qglDeleteObjectARB)(GLhandleARB);
void (*qglGetInfoLogARB)(GLhandleARB, GLsizei, GLsizei *, GLcharARB *);
void (*qglGetObjectParameterivARB)(GLhandleARB, GLenum, GLint *);

void qglDeleteProgramARBEmulation(GLuint program)
{
	qglDeleteObjectARB(program);
}

void qglDeleteShaderARBEmulation(GLuint shader)
{
	qglDeleteObjectARB(shader);
}

void qglGetProgramInfoLogARBEmulation(GLuint program, GLsizei maxLength, GLsizei *length, GLchar *infoLog)
{
	qglGetInfoLogARB(program, maxLength, length, infoLog);
}

void qglGetProgramivARBEmulation(GLuint program, GLenum pname, GLint *params)
{
	qglGetObjectParameterivARB(program, pname, params);
}

void qglGetShaderInfoLogARBEmulation(GLuint shader, GLsizei maxLength, GLsizei *length, GLchar *infoLog)
{
	qglGetInfoLogARB(shader, maxLength, length, infoLog);
}

void qglGetShaderivARBEmulation(GLuint shader, GLenum pname, GLint *params)
{
	qglGetObjectParameterivARB(shader, pname, params);
}

qboolean gl_combine = false;

qboolean gl_add_ext = false;

qboolean gl_npot;

qboolean gl_vbo = false;

qboolean gl_vs;
qboolean gl_fs;

float gldepthmin, gldepthmax;

float vid_gamma = 1.0;
byte vid_gamma_table[256];

unsigned short d_8to16table[256];
unsigned d_8to24table[256];
unsigned d_8to24table2[256];

byte color_white[4] = {255, 255, 255, 0};
byte color_black[4] = {0, 0, 0, 0};

qboolean OnChange_gl_ext_texture_compression(cvar_t *, char *);

cvar_t gl_strings = {"gl_strings", "", CVAR_ROM};
cvar_t gl_ext_texture_compression = {"gl_ext_texture_compression", "0", 0, OnChange_gl_ext_texture_compression};

/************************************* EXTENSIONS *************************************/

qboolean CheckExtension (const char *extension)
{
	const char *start;
	char *where, *terminator;

	if (!gl_extensions && !(gl_extensions = (char *)glGetString (GL_EXTENSIONS)))
		return false;

	if (!extension || *extension == 0 || strchr (extension, ' '))
		return false;

	for (start = gl_extensions; (where = strstr(start, extension)); start = terminator)
	{
		terminator = where + strlen (extension);
		if ((where == start || *(where - 1) == ' ') && (*terminator == 0 || *terminator == ' '))
			return true;
	}

	return false;
}

void CheckMultiTextureExtensions (void)
{
	if (COM_CheckParm("-nomtex"))
		return;

	if (strstr(gl_renderer, "Savage"))
	{
		Com_Printf("Multitexturing disabled for this graphics card.\n");
		return;
	}

	glGetIntegerv(GL_MAX_TEXTURE_UNITS, &gl_textureunits);

	if (COM_CheckParm("-maxtmu2")/* || !strcmp(gl_vendor, "ATI Technologies Inc.")*/)
		gl_textureunits = min(gl_textureunits, 2);

	gl_textureunits = min(gl_textureunits, 4);
	gl_mtexable = true;

	if (gl_textureunits < 2)
		gl_mtexable = false;

	if (!gl_mtexable)
		Com_Printf("OpenGL multitexturing extensions not available.\n");
}

void GL_CheckExtensions (void)
{
	CheckMultiTextureExtensions ();

	gl_combine = CheckExtension("GL_ARB_texture_env_combine");
	gl_add_ext = CheckExtension("GL_ARB_texture_env_add");
	gl_npot = CheckExtension("GL_ARB_texture_non_power_of_two");
	gl_vbo = gl_version_number >= 2 || CheckExtension("GL_ARB_vertex_buffer_object");
	gl_vs = gl_version_number >= 2 || CheckExtension("GL_ARB_vertex_shader");
	gl_fs = gl_version_number >= 2 || CheckExtension("GL_ARB_fragment_shader");

	if (gl_vbo)
	{
		if (gl_version_number >= 2)
		{
			qglBindBufferARB = VID_GetProcAddress("glBindBuffer");
			qglBufferDataARB = VID_GetProcAddress("glBufferData");
			qglBufferSubDataARB = VID_GetProcAddress("glBufferSubData");
		}
		else
		{
			qglBindBufferARB = VID_GetProcAddress("glBindBufferARB");
			qglBufferDataARB = VID_GetProcAddress("glBufferDataARB");
			qglBufferSubDataARB = VID_GetProcAddress("glBufferSubDataARB");
		}

		if (qglBindBufferARB == 0 || qglBufferDataARB == 0 || qglBufferSubDataARB == 0)
			gl_vbo = false;
	}

	if (gl_vs || gl_fs)
	{
		if (gl_version_number >= 2)
		{
			qglAttachShader = VID_GetProcAddress("glAttachShader");
			qglCompileShader = VID_GetProcAddress("glCompileShader");
			qglCreateProgram = VID_GetProcAddress("glCreateProgram");
			qglCreateShader = VID_GetProcAddress("glCreateShader");
			qglDeleteProgram = VID_GetProcAddress("glDeleteProgram");
			qglDeleteShader = VID_GetProcAddress("glDeleteShader");
			qglGetProgramInfoLog = VID_GetProcAddress("glGetProgramInfoLog");
			qglGetProgramiv = VID_GetProcAddress("glGetProgramiv");
			qglGetShaderInfoLog = VID_GetProcAddress("glGetShaderInfoLog");
			qglGetShaderiv = VID_GetProcAddress("glGetShaderiv");
			qglGetUniformLocation = VID_GetProcAddress("glGetUniformLocation");
			qglLinkProgram = VID_GetProcAddress("glLinkProgram");
			qglShaderSource = VID_GetProcAddress("glShaderSource");
			qglUniform1f = VID_GetProcAddress("glUniform1f");
			qglUniform1i = VID_GetProcAddress("glUniform1i");
			qglUniformMatrix4fv = VID_GetProcAddress("glUniformMatrix4fv");
			qglUseProgram = VID_GetProcAddress("glUseProgram");

			if (qglAttachShader == 0
			 || qglCompileShader == 0
			 || qglCreateProgram == 0
			 || qglCreateShader == 0
			 || qglDeleteProgram == 0
			 || qglDeleteShader == 0
			 || qglGetProgramInfoLog == 0
			 || qglGetProgramiv == 0
			 || qglGetUniformLocation == 0
			 || qglLinkProgram == 0
			 || qglShaderSource == 0
			 || qglUniform1f == 0
			 || qglUniform1i == 0
			 || qglUniformMatrix4fv == 0
			 || qglUseProgram == 0)
			{
				gl_vs = 0;
				gl_fs = 0;
			}
			else if (gl_vs)
			{
				qglBindAttribLocation = VID_GetProcAddress("glBindAttribLocation");
				qglDisableVertexAttribArray = VID_GetProcAddress("glDisableVertexAttribArray");
				qglEnableVertexAttribArray = VID_GetProcAddress("glEnableVertexAttribArray");
				qglVertexAttribPointer = VID_GetProcAddress("glVertexAttribPointer");

				if (qglBindAttribLocation == 0
				 || qglDisableVertexAttribArray == 0
				 || qglEnableVertexAttribArray == 0
				 || qglVertexAttribPointer == 0)
				{
					gl_vs = 0;
				}
			}
		}
		else
		{
			qglAttachShader = VID_GetProcAddress("glAttachObjectARB");
			qglCompileShader = VID_GetProcAddress("glCompileShaderARB");
			qglCreateProgram = VID_GetProcAddress("glCreateProgramObjectARB");
			qglCreateShader = VID_GetProcAddress("glCreateShaderObjectARB");
			qglDeleteObjectARB = VID_GetProcAddress("glDeleteObjectARB");
			qglGetInfoLogARB = VID_GetProcAddress("glGetInfoLogARB");
			qglGetObjectParameterivARB = VID_GetProcAddress("glGetObjectParameterivARB");
			qglGetUniformLocation = VID_GetProcAddress("glGetUniformLocationARB");
			qglLinkProgram = VID_GetProcAddress("glLinkProgramARB");
			qglShaderSource = VID_GetProcAddress("glShaderSourceARB");
			qglUniform1f = VID_GetProcAddress("glUniform1fARB");
			qglUniform1i = VID_GetProcAddress("glUniform1iARB");
			qglUniformMatrix4fv = VID_GetProcAddress("glUniformMatrix4fvARB");
			qglUseProgram = VID_GetProcAddress("glUseProgramObjectARB");

			if (qglAttachShader == 0
			 || qglCompileShader == 0
			 || qglCreateProgram == 0
			 || qglCreateShader == 0
			 || qglDeleteObjectARB == 0
			 || qglGetInfoLogARB == 0
			 || qglGetObjectParameterivARB == 0
			 || qglGetUniformLocation == 0
			 || qglLinkProgram == 0
			 || qglShaderSource == 0
			 || qglUniform1f == 0
			 || qglUniform1i == 0
			 || qglUniformMatrix4fv == 0
			 || qglUseProgram == 0)
			{
				gl_vs = 0;
				gl_fs = 0;
			}
			else if (gl_vs)
			{
				qglDeleteProgram = qglDeleteProgramARBEmulation;
				qglDeleteShader = qglDeleteShaderARBEmulation;
				qglGetProgramInfoLog = qglGetProgramInfoLogARBEmulation;
				qglGetProgramiv = qglGetProgramivARBEmulation;
				qglGetShaderInfoLog = qglGetShaderInfoLogARBEmulation;
				qglGetShaderiv = qglGetShaderivARBEmulation;

				qglBindAttribLocation = VID_GetProcAddress("glBindAttribLocationARB");
				qglDisableVertexAttribArray = VID_GetProcAddress("glDisableVertexAttribArrayARB");
				qglEnableVertexAttribArray = VID_GetProcAddress("glEnableVertexAttribArrayARB");
				qglVertexAttribPointer = VID_GetProcAddress("glVertexAttribPointerARB");

				if (qglBindAttribLocation == 0
				 || qglDisableVertexAttribArray == 0
				 || qglEnableVertexAttribArray == 0
				 || qglVertexAttribPointer == 0)
				{
					gl_vs = 0;
				}
			}
		}
	}

	if (CheckExtension("GL_ARB_texture_compression"))
	{
		Com_Printf("Texture compression extensions found\n");
	}
}

qboolean OnChange_gl_ext_texture_compression(cvar_t *var, char *string)
{
	float newval = Q_atof(string);

	if (!newval == !var->value)
		return false;

	gl_alpha_format = newval ? GL_COMPRESSED_RGBA_ARB : GL_RGBA;
	gl_solid_format = newval ? GL_COMPRESSED_RGB_ARB : GL_RGB;

	return false;
}

/************************************** GL INIT **************************************/

void GL_CvarInit()
{
	Cvar_Register (&gl_strings);
	Cvar_SetCurrentGroup(CVAR_GROUP_TEXTURES);
	Cvar_Register (&gl_ext_texture_compression);	
	Cvar_ResetCurrentGroup();
}

void GL_Init (void)
{
	const char *p;

	gl_vendor = (char *)glGetString(GL_VENDOR);
	Com_Printf("GL_VENDOR: %s\n", gl_vendor);
	gl_renderer = (char *)glGetString(GL_RENDERER);
	Com_Printf("GL_RENDERER: %s\n", gl_renderer);
	gl_version = (char *)glGetString(GL_VERSION);
	Com_Printf("GL_VERSION: %s\n", gl_version);
	gl_extensions = (char *)glGetString(GL_EXTENSIONS);
	if (COM_CheckParm("-gl_ext"))
		Com_Printf("GL_EXTENSIONS: %s\n", gl_extensions);

	Cvar_ForceSet (&gl_strings, va("GL_VENDOR: %s\nGL_RENDERER: %s\n"
		"GL_VERSION: %s\nGL_EXTENSIONS: %s", gl_vendor, gl_renderer, gl_version, gl_extensions));

	gl_version_number = atoi(gl_version);

	p = gl_version;
	while(*p >= 0 && *p <= '9')
		p++;

	if (*p == '.')
		gl_revision_number = atoi(p+1);
	else
		gl_revision_number = 0;

	glClearColor (1,0,0,0);
	glCullFace(GL_FRONT);
	glEnable(GL_TEXTURE_2D);

	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.666);

	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel (GL_FLAT);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	GL_CheckExtensions();

	GL_SetArrays(0);
}

/************************************* VID GAMMA *************************************/

void Check_Gamma (unsigned char *pal)
{
	float inf;
	unsigned char palette[768];
	int i;

	if ((i = COM_CheckParm("-gamma")) != 0 && i + 1 < com_argc)
		vid_gamma = bound (0.3, Q_atof(com_argv[i + 1]), 1);
	else
		vid_gamma = 1;

	Cvar_SetDefault (&v_gamma, vid_gamma);

	if (vid_gamma != 1)
	{
		for (i = 0; i < 256; i++)
		{
			inf = 255 * pow((i + 0.5) / 255.5, vid_gamma) + 0.5;
			if (inf > 255)
				inf = 255;

			vid_gamma_table[i] = inf;
		}
	}
	else
	{
		for (i = 0; i < 256; i++)
			vid_gamma_table[i] = i;
	}

	for (i = 0; i < 768; i++)
		palette[i] = vid_gamma_table[pal[i]];

	memcpy (pal, palette, sizeof(palette));
}

/************************************* HW GAMMA *************************************/

void VID_SetPalette (unsigned char *palette)
{
	int i;
	byte *pal;
	unsigned r,g,b, v, *table;

	// 8 8 8 encoding
	pal = palette;
	table = d_8to24table;
	for (i = 0; i < 256; i++)
	{
		r = pal[0];
		g = pal[1];
		b = pal[2];
		pal += 3;

		v = (255 << 24) + (b << 16) + (g << 8) + (r << 0);
		*table++ = LittleLong(v);
	}
	d_8to24table[255] = 0;	// 255 is transparent

	// Tonik: create a brighter palette for bmodel textures
	pal = palette;
	table = d_8to24table2;

	for (i = 0; i < 256; i++)
	{
		r = pal[0] * (2.0 / 1.5); if (r > 255) r = 255;
		g = pal[1] * (2.0 / 1.5); if (g > 255) g = 255;
		b = pal[2] * (2.0 / 1.5); if (b > 255) b = 255;
		pal += 3;
		v = (255 << 24) + (b << 16) + (g << 8) + (r << 0);
		*table++ = LittleLong(v);
	}

	d_8to24table2[255] = 0;	// 255 is transparent
}

