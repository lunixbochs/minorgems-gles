/*
 * Modification History
 *
 * 2001-September-15		Jason Rohrer
 * Created.
 *
 * 2006-July-3		Jason Rohrer
 * Fixed warnings.
 *
 * 2006-September-8		Jason Rohrer
 * Made alpha-aware.
 */
 
 
#ifndef GUI_PANEL_GL_INCLUDED
#define GUI_PANEL_GL_INCLUDED 

#include "GUIComponentGL.h"
#include "GUIContainerGL.h"
#include "minorGems/util/SimpleVector.h"

#include "minorGems/graphics/Color.h"

#include "minorGems/graphics/openGL/glInclude.h"


/**
 * A container with a background color that is drawn
 * behind the components.
 *
 * @author Jason Rohrer
 */
class GUIPanelGL : public GUIContainerGL {


	public:



		/**
		 * Constructs a panel.
		 *
		 * @param inAnchorX the x position of the upper left corner
		 *   of this component.
		 * @param inAnchorY the y position of the upper left corner
		 *   of this component.
		 * @param inWidth the width of this component.
		 * @param inHeight the height of this component.
		 * @param inColor the background color for this panel.
		 *   Will be destroyed when this class is destroyed.
		 */
		GUIPanelGL(
			double inAnchorX, double inAnchorY, double inWidth,
			double inHeight, Color *inColor );


		
		virtual ~GUIPanelGL();


		
		// override fireRedraw() in GUIComponentGL
		virtual void fireRedraw();


		
	protected:
		Color *mColor;
				
	};



inline GUIPanelGL::GUIPanelGL(
	double inAnchorX, double inAnchorY, double inWidth,
	double inHeight, Color *inColor )
	: GUIContainerGL( inAnchorX, inAnchorY, inWidth, inHeight ),
	  mColor( inColor ) {

	}



inline GUIPanelGL::~GUIPanelGL() {
	delete mColor;
	}

		
		
inline void GUIPanelGL::fireRedraw() {
	
	// draw our background color as a rectangle
	glColor4f( mColor->r, mColor->g, mColor->b, mColor->a ); 
	
#ifndef HAVE_GLES
	glBegin( GL_QUADS ); {
		glVertex2d( mAnchorX, mAnchorY ); 
		glVertex2d( mAnchorX + mWidth, mAnchorY ); 
		glVertex2d( mAnchorX + mWidth, mAnchorY + mHeight );
		glVertex2d( mAnchorX, mAnchorY + mHeight );
		}
	glEnd();
#else
	GLfloat verts[] = {
		mAnchorX, mAnchorY,
		mAnchorX + mWidth, mAnchorY,
		mAnchorX + mWidth, mAnchorY + mHeight,
		mAnchorX, mAnchorY + mHeight
	};

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, verts);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glDisableClientState(GL_VERTEX_ARRAY);
#endif
	
	// call the supercalss redraw
	GUIContainerGL::fireRedraw();
	}



#endif



