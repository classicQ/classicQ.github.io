#include "gl_local.h"
#include "gl_state.h"

void GL_SetAlphaTestBlend(int alphatest, int blend)
{
	static int old_alphatest = 2;
	static int old_blend = 2;

	if (alphatest != old_alphatest)
	{
		if (alphatest)
			glEnable(GL_ALPHA_TEST);
		else
			glDisable(GL_ALPHA_TEST);

		old_alphatest = alphatest;
	}

	if (blend != old_blend)
	{
		if (blend)
			glEnable(GL_BLEND);
		else
			glDisable(GL_BLEND);

		old_blend = blend;
	}
}

void GL_SetArrays(unsigned int arrays)
{
	static unsigned int old_arrays;
	unsigned int diff;

	diff = arrays ^ old_arrays;

	if (diff)
	{
		if ((diff & FQ_GL_VERTEX_ARRAY))
		{
			if ((arrays & FQ_GL_VERTEX_ARRAY))
				glEnableClientState(GL_VERTEX_ARRAY);
			else
				glDisableClientState(GL_VERTEX_ARRAY);
		}

		if ((diff & FQ_GL_COLOR_ARRAY))
		{
			if ((arrays & FQ_GL_COLOR_ARRAY))
				glEnableClientState(GL_COLOR_ARRAY);
			else
				glDisableClientState(GL_COLOR_ARRAY);
		}

		if ((diff & FQ_GL_TEXTURE_COORD_ARRAY))
		{
			glClientActiveTexture(GL_TEXTURE0);

			if ((arrays & FQ_GL_TEXTURE_COORD_ARRAY))
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			else
				glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		}

		if ((diff & FQ_GL_TEXTURE_COORD_ARRAY_1))
		{
			glClientActiveTexture(GL_TEXTURE1);

			if ((arrays & FQ_GL_TEXTURE_COORD_ARRAY_1))
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			else
				glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		}

		if ((diff & FQ_GL_TEXTURE_COORD_ARRAY_2))
		{
			glClientActiveTexture(GL_TEXTURE2);

			if ((arrays & FQ_GL_TEXTURE_COORD_ARRAY_2))
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			else
				glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		}

		old_arrays = arrays;

		glClientActiveTexture(GL_TEXTURE0);
	}
}

void GL_VertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	glVertexPointer(size, type, stride, pointer);
}

void GL_ColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	glColorPointer(size, type, stride, pointer);
}

void GL_TexCoordPointer(unsigned int tmu, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	glClientActiveTexture(GL_TEXTURE0 + tmu);
	glTexCoordPointer(size, type, stride, pointer);
}

