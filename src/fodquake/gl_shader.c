#include <stdlib.h>

#include "gl_local.h"
#include "gl_shader.h"

static int GL_CompileShader(GLenum type, const char *shader)
{
	int object;
	int compiled;

	object = qglCreateShader(type);
	qglShaderSource(object, 1, &shader, 0);
	qglCompileShader(object);
	qglGetShaderiv(object, GL_COMPILE_STATUS, &compiled);
	if (!compiled)
	{
		char *log;
		int loglength;

		fprintf(stderr, "Failed to compile the following shader:\n%s\n", shader);

		qglGetShaderiv(object, GL_INFO_LOG_LENGTH, &loglength);
		if (loglength)
		{
			log = malloc(loglength);
			if (log)
			{
				qglGetShaderInfoLog(object, loglength, NULL, log);

				fprintf(stderr, "OpenGL returned:\n%s\n", log);

				free(log);
			}
		}

		qglDeleteShader(object);

		return 0;
	}

	return object;
}

int GL_SetupShaderProgram(int vertexobject, const char *vertexshader, int fragmentobject, const char *fragmentshader)
{
	int programobject;
	int linked;

	if ((vertexobject && vertexshader) || (fragmentobject && fragmentshader))
		return 0;

	if (vertexshader)
	{
		vertexobject = GL_CompileShader(GL_VERTEX_SHADER, vertexshader);
	}

	if (fragmentshader)
	{
		fragmentobject = GL_CompileShader(GL_FRAGMENT_SHADER, fragmentshader);
	}

	if ((vertexobject || vertexshader == 0) && (fragmentobject || fragmentshader == 0))
	{
		programobject = qglCreateProgram();

		if (vertexobject)
			qglAttachShader(programobject, vertexobject);

		if (fragmentobject)
			qglAttachShader(programobject, fragmentobject);
	}
	else
		programobject = 0;

	if (vertexshader && vertexobject)
		qglDeleteShader(vertexobject);

	if (fragmentshader && fragmentobject)
		qglDeleteShader(fragmentobject);

	if (!programobject)
		return 0;

	qglLinkProgram(programobject);
	qglGetProgramiv(programobject, GL_LINK_STATUS, &linked);

	if (!linked)
	{
		char *log;
		int loglength;

		fprintf(stderr, "Failed to link the following shader(s):\n");
		if (vertexshader)
			fprintf(stderr, "Vertex shader:\n%s\n", vertexshader);

		if (fragmentshader)
			fprintf(stderr, "Fragment shader:\n%s\n", fragmentshader);

		qglGetProgramiv(programobject, GL_INFO_LOG_LENGTH, &loglength);
		if (loglength)
		{
			log = malloc(loglength);
			if (log)
			{
				qglGetProgramInfoLog(programobject, loglength, NULL, log);

				fprintf(stderr, "OpenGL returned:\n%s\n", log);

				free(log);
			}
		}
		else
		{
			fprintf(stderr, "OpenGL returned fuck all\n");
		}

		qglDeleteProgram(programobject);

		return 0;
	}

	return programobject;
}

int GL_Shader_Init()
{
	return 1;
}

void GL_Shader_Shutdown()
{
}

