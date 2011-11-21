#ifdef __mac__

#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#ifdef HAVE_GLES
#error GLES on Mac is undefined
#endif

#else

#ifndef HAVE_GLES
#include <GL/gl.h>
#include <GL/glu.h>
#else

#include <GLES/gl.h>
#include <minorGems/graphics/openGL/GLES/glu.h>

#define GL_CLAMP GL_CLAMP_TO_EDGE
#define GL_DOUBLE GL_FLOAT
#define GL_QUADS GL_TRIANGLE_FAN
#define GLdouble GLfloat
#define glOrtho glOrthof

#endif

#endif
