#ifndef GAME_GRAPHICS_INCLUDED
#define GAME_GRAPHICS_INCLUDED


// platform-independent wrapper for graphics primitives


#include "doublePair.h"

#include "minorGems/graphics/Image.h"



void setDrawColor( float inR, float inG, float inB, float inA );

void setDrawFade( float inA );


// defaults to non-additive blending
void toggleAdditiveBlend( char inAdditive );


// defautls to nearest-neighbor texture magnification
void toggleLinearMagFilter( char inLinearFilterOn );


// draw using last set color
// four vertices per quad
#ifndef HAVE_GLES
void drawQuads( int inNumQuads, double inVertices[] );
#else
void drawQuads( int inNumQuads, float inVertices[] );
#endif

// four r,g,b,a values per quad vertex
#ifndef HAVE_GLES
void drawQuads( int inNumQuads, double inVertices[], float inVertexColors[] );
#else
void drawQuads( int inNumQuads, float inVertices[], float inVertexColors[] );
#endif

// draw using last set color
// three vertices per triangle
void drawTriangles( int inNumTriangles, double inVertices[], 
                    char inStrip=false, char inFan=false );


// four r,g,b,a values per triangle vertex
void drawTrianglesColor( int inNumTriangles, double inVertices[], 
                         float inVertexColors[],
                         char inStrip=false, char inFan=false );


// switch into stencil-buffer drawing mode
// inAdd determines whether drawn primitives add to or subtract from
//   pass-through area of stencil
void startAddingToStencil( char inDrawColorToo, char inAdd );

// switch into mode where further drawing operations are passed through
// stenciled area
void startDrawingThroughStencil( char inInvertStencil=false );

// stop stenciling and clear stencil buffer
void stopStencil();



typedef void * SpriteHandle;


// can be NULL on load failure
SpriteHandle loadSprite( const char *inTGAFileName, 
                         char inTransparentLowerLeftCorner = true );

SpriteHandle fillSprite( Image *inImage, 
                         char inTransparentLowerLeftCorner = true );

SpriteHandle fillSprite( unsigned char *inRGBA, 
                         unsigned int inWidth, unsigned int inHeight );

// fill a one-channel (alpha-only) sprite
// other channels will be set to black.
// (more efficient than full RGBA for things like shadows)
SpriteHandle fillSpriteAlphaOnly( unsigned char *inA,
                                  unsigned int inWidth, 
                                  unsigned int inHeight );



// Image destroyed by caller
Image *readTGAFile( const char *inTGAFileName );

// write a TGA file into main directory
// Image destroyed by caller
void writeTGAFile( const char *inTGAFileName, Image *inImage );


void freeSprite( SpriteHandle inSprite );


// draw with current draw color
// mag filter defaults to off (nearest neighbor, big pixels)
void drawSprite( SpriteHandle inSprite, doublePair inCenter, 
                 double inZoom = 1.0 );



#endif
                 
