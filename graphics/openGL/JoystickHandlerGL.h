/*
 * Modification History
 *
 * 2011-November-16		Ryan Hileman
 * Copied from MouseHandlerGL.h 
 */
 
 
#ifndef JOYSTICK_HANDLER_GL_INCLUDED
#define JOYSTICK_HANDLER_GL_INCLUDED 


/**
 * Interface for an object that can field OpenGL joystick events.
 *
 * @author Jason Rohrer 
 */
class JoystickHandlerGL { 
	
	public:

		virtual ~JoystickHandlerGL() {
            }

		/**
		 * Callback function for when joystick moves.
		 *
		 * @param num joystick number.
		 * @param inX x position of joystick.
		 * @param inY y position of joystick.
		 */
		virtual void joystickMoved( int num, int inX, int inY ) = 0;
        
        char mHandlerFlagged;
			
    protected:
        
        JoystickHandlerGL()
                : mHandlerFlagged( false ) {
            }
        
	};



#endif
