
#define FQ_GL_VERTEX_ARRAY          (1<<0)
#define FQ_GL_COLOR_ARRAY           (1<<1)
#define FQ_GL_TEXTURE_COORD_ARRAY   (1<<2)
#define FQ_GL_TEXTURE_COORD_ARRAY_1 (1<<3)
#define FQ_GL_TEXTURE_COORD_ARRAY_2 (1<<4)

void GL_SetAlphaTestBlend(int alphatest, int blend);
void GL_SetArrays(unsigned int arrays);
void GL_VertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void GL_ColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void GL_TexCoordPointer(unsigned int tmu, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);

