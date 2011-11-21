/*
 * Modification History
 *
 * 2008-October-27		Jason Rohrer
 * Created.  Copied structure from GLUT version (ScreenGL.cpp). 
 *
 * 2009-March-19		Jason Rohrer
 * UNICODE support for keyboard input of symbols. 
 *
 * 2010-March-17	Jason Rohrer
 * Support for Ctrl, Alt, and Meta.
 *
 * 2010-April-6 	Jason Rohrer
 * Blocked event passing to handlers that were added by event.
 *
 * 2010-April-8 	Jason Rohrer
 * Added post-redraw events.
 * Don't treat wheel events as mouse clicks.
 *
 * 2010-April-9 	Jason Rohrer
 * Checks for closest matching resolution.
 *
 * 2010-April-20   	Jason Rohrer
 * Alt-Enter to leave fullscreen mode.
 * Reload all SingleTextures after GL context change.
 *
 * 2010-May-7   	Jason Rohrer
 * Mapped window close button to ESC key.
 * Extended ASCII.
 *
 * 2010-May-12   	Jason Rohrer
 * Use full 8 lower bits of unicode to support extended ASCII.
 *
 * 2010-May-14    Jason Rohrer
 * String parameters as const to fix warnings.
 *
 * 2010-May-19    Jason Rohrer
 * Added support for 1-bit stencil buffer.
 *
 * 2010-May-21    Jason Rohrer
 * Mapped ctrl-q and alt-q to ESC key.
 *
 * 2010-August-26    Jason Rohrer
 * Fixed a parens warning.
 *
 * 2010-September-6   Jason Rohrer
 * Split display callback into two parts to handle events after frame sleep.
 *
 * 2010-September-8   Jason Rohrer
 * Fixed bug in ASCII key release event.
 * Swap control to eliminate tearing.
 *
 * 2010-September-9   Jason Rohrer
 * Moved frame rate limit into ScreenGL class.
 *
 * 2010-November-2 	Jason Rohrer
 * Support for eating key events.
 *
 * 2010-November-3 	Jason Rohrer
 * Fixed frame timing on platforms where sleeps can be shorter than requested.
 *
 * 2010-November-17   Jason Rohrer
 * Added input recording and playback.
 *
 * 2010-November-18   Jason Rohrer
 * Record and playback rand seed.
 * ASCII key mapping.
 *
 * 2010-December-23   Jason Rohrer
 * Support for delaying start of event playback or recording.
 *
 * 2010-December-27   Jason Rohrer
 * Support for slowdown keys during playback.
 *
 * 2010-December-30   Jason Rohrer
 * Fixed to ignore hidden files in playbackGame directory.
 *
 * 2011-January-3   Jason Rohrer
 * Fast-forward key support.
 * Added custom data to recorded game files.  
 * Fixed bug in hidden file skipping.
 * Support for detecting playback mode.
 *
 * 2011-January-25   Jason Rohrer
 * Now respects current screen's aspect ratio when picking a resolution.
 *
 * 2011-January-26   Jason Rohrer
 * Switched to integer representation for aspect ratio comparisons.
 *
 * 2011-January-31   Jason Rohrer
 * Fixed bugs in aspect ration calculations.
 *
 * 2011-February-3   Jason Rohrer
 * Now always picks resolution at least as big as what is requested.
 * 
 * 2011-February-6   Jason Rohrer
 * Fixed to stop playback when end of recorded event file is reached.
 * Support for getting rough playback done fraction.
 * 
 * 2011-February-7   Jason Rohrer
 * Support for minimizing on Alt-tab out of fullscreen mode.
 * 
 * 2011-February-9   Jason Rohrer
 * Hash checking of custom data in recorded game files.
 * 
 * 2011-February-12   Jason Rohrer
 * Playback display toggle.
 * 
 * 2011-February-22   Jason Rohrer
 * Windowed mode forced if fullscreen dimensions specified in playback file 
 * not available.
 * Got Alt-Tab working for windowed mode too.
 * 
 * 2011-March-14   Jason Rohrer
 * Changed Alt-Tab to explicitly release mouse.  
 */


#include "ScreenGL.h" 
#include "SingleTextureGL.h" 

#include "minorGems/graphics/openGL/glInclude.h"
#include <SDL/SDL.h>

#ifdef HAVE_GLES
#include "minorGems/graphics/openGL/eglport.h"
#endif


#include <math.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>


#include "minorGems/util/stringUtils.h"
#include "minorGems/util/SettingsManager.h"
#include "minorGems/util/log/AppLog.h"
#include "minorGems/io/file/File.h"

#include "minorGems/system/Time.h"
#include "minorGems/system/Thread.h"

#include "minorGems/crypto/hashes/sha1.h"



/* ScreenGL to be accessed by callback functions.
 *
 * Note that this is a bit of a hack, but the callbacks
 * require a C-function (not a C++ member) and have fixed signatures,
 * so there's no way to pass the current screen into the functions.
 *
 * This hack prevents multiple instances of the ScreenGL class from
 * being used simultaneously.
 *
 * Even worse for SLD, because this hack is carried over from GLUT.
 * SDL doesn't even require C callbacks (you provide the event loop).
 */
ScreenGL *currentScreenGL;



// maps SDL-specific special (non-ASCII) key-codes (SDLK) to minorGems key 
// codes (MG_KEY)
int mapSDLSpecialKeyToMG( int inSDLKey );

// for ascii key
char mapSDLKeyToASCII( int inSDLKey );


static unsigned char keyMap[256];



// prototypes
/*
void callbackResize( int inW, int inH );
void callbackKeyboard( unsigned char  inKey, int inX, int inY );
void callbackMotion( int inX, int inY );
void callbackPassiveMotion( int inX, int inY );
void callbackMouse( int inButton, int inState, int inX, int inY );

void callbackJoystick( int num, int inX, int inY );

void callbackPreDisplay();
void callbackDisplay();
void callbackIdle();
*/

ScreenGL::ScreenGL( int inWide, int inHigh, char inFullScreen,
                    unsigned int inMaxFrameRate,
                    char inRecordEvents,
                    const char *inCustomRecordedGameData,
                    const char *inHashSalt,
                    const char *inWindowName,
					KeyboardHandlerGL *inKeyHandler,
					MouseHandlerGL *inMouseHandler,
					JoystickHandlerGL *inJoystickHandler,
					SceneHandlerGL *inSceneHandler  ) 
	: mWide( inWide ), mHigh( inHigh ),
      mForceAspectRatio( false ),
      mForceSpecifiedDimensions( false ),
      mImageWide( inWide ), mImageHigh( inHigh ),
      mFullScreen( inFullScreen ),
      mMaxFrameRate( inMaxFrameRate ),
      mFullFrameRate( inMaxFrameRate ),
      m2DMode( false ),
	  mViewPosition( new Vector3D( 0, 0, 0 ) ),
	  mViewOrientation( new Angle3D( 0, 0, 0 ) ),
	  mMouseHandlerVector( new SimpleVector<MouseHandlerGL*>() ),
	  mKeyboardHandlerVector( new SimpleVector<KeyboardHandlerGL*>() ),
	  mJoystickHandlerVector( new SimpleVector<JoystickHandlerGL*>() ),
	  mSceneHandlerVector( new SimpleVector<SceneHandlerGL*>() ),
	  mRedrawListenerVector( new SimpleVector<RedrawListenerGL*>() ) {

    mWantToMimimize = false;
    mMinimized = false;
    mWasFullScreenBeforeMinimize = false;
    
    mCustomRecordedGameData = stringDuplicate( inCustomRecordedGameData );
    


    mAllowSlowdownKeysDuringPlayback = false;

	// add handlers if NULL (the default) was not passed in for them
	if( inMouseHandler != NULL ) {
		addMouseHandler( inMouseHandler );
		}
	if( inKeyHandler != NULL ) {
		addKeyboardHandler( inKeyHandler );
		}
	if( inJoystickHandler != NULL ) {
		addJoystickHandler( inJoystickHandler );
		}
	if( inSceneHandler != NULL ) {
		addSceneHandler( inSceneHandler );
		}

    
    mRandSeed = time( NULL );
    

    mShouldShowPlaybackDisplay = true;

    int hidePlaybackDisplayFlag = 
        SettingsManager::getIntSetting( "hidePlaybackDisplay", 0 );
    
    if( hidePlaybackDisplayFlag == 1 ) {
        mShouldShowPlaybackDisplay = false;
        }


    mRecordingOrPlaybackStarted = false;
    
    mRecordingEvents = inRecordEvents;
    mPlaybackEvents = false;
    mEventFile = NULL;
    mEventFileNumBatches = 0;
    mNumBatchesPlayed = 0;
    
    // playback overrides recording, check for it first
    // do this before setting up surface
    
    File playbackDir( NULL, "playbackGame" );
    
    if( !playbackDir.exists() ) {
        playbackDir.makeDirectory();
        }
    
    int numChildren;
    File **childFiles = playbackDir.getChildFiles( &numChildren );

    if( numChildren > 0 ) {

        // take first
        char *fullFileName = childFiles[0]->getFullFileName();
        char *partialFileName = childFiles[0]->getFileName();
        
        // skip hidden files
        int i = 0;
        while( partialFileName != NULL &&
               partialFileName[i] == '.' ) {

            delete [] fullFileName;
            fullFileName = NULL;

            delete [] partialFileName;
            partialFileName = NULL;

            i++;
            if( i < numChildren ) {
                fullFileName = childFiles[i]->getFullFileName();
                partialFileName = childFiles[i]->getFileName();
                }
            }


        if( fullFileName != NULL ) {
            delete [] partialFileName;
            
            

            mEventFile = fopen( fullFileName, "r" );

            if( mEventFile == NULL ) {
                AppLog::error( "Failed to open event playback file" );
                }
            else {

                // count number of newlines in file (close to the number
                // of batches in the file)
                char *fileContents = childFiles[i]->readFileContents();
                
                int fileLength = strlen( fileContents );
                
                for( int j=0; j<fileLength; j++ ) {
                    if( fileContents[j] == '\n' ) {
                        mEventFileNumBatches ++;
                        }
                    }
                delete [] fileContents;
                

                AppLog::getLog()->logPrintf( 
                    Log::INFO_LEVEL,
                    "Playing back game from file %s", fullFileName );
            

                // first, determine max possible length of custom data
                int maxCustomLength = 0;
                
                int readChar = fgetc( mEventFile );
                
                while( readChar != EOF && readChar != '\n' ) {
                    maxCustomLength++;
                    readChar = fgetc( mEventFile );
                    }
                
                // back to start
                rewind( mEventFile );
                
                char *readCustomGameData = new char[ maxCustomLength ];

                char hashString[41];

                
                
                int fullScreenFlag;
                unsigned int readRandSeed;
                unsigned int readMaxFrameRate;
                int readWide;
                int readHigh;
                
                int numScanned =
                    fscanf( 
                        mEventFile, 
                        "%u seed, %u fps, %dx%d, fullScreen=%d, %s %40s\n",
                        &readRandSeed,
                        &readMaxFrameRate,
                        &readWide, &readHigh, &fullScreenFlag, 
                        readCustomGameData,
                        hashString );
                
                if( numScanned == 7 ) {

                    char *stringToHash = autoSprintf( "%s%s",
                                                      readCustomGameData,
                                                      inHashSalt );

                    char *correctHash = computeSHA1Digest( stringToHash );

                    delete [] stringToHash;
                    
                    int difference = strcmp( correctHash, hashString );
                    
                    delete [] correctHash;

                    if( difference == 0 ) {

                        mRecordingEvents = false;
                        mPlaybackEvents = true;
                        
                        mRandSeed = readRandSeed;
                        mMaxFrameRate = readMaxFrameRate;
                        mWide = readWide;
                        mHigh = readHigh,
                        
                        mFullFrameRate = mMaxFrameRate;
                    
                        mImageWide = mWide;
                        mImageHigh = mHigh;
                        
                        AppLog::info( 
                          "Forcing dimensions specified in playback file" );
                        mForceSpecifiedDimensions = true;
                        
                        
                        if( fullScreenFlag ) {
                            mFullScreen = true;
                            }
                        else {
                            mFullScreen = false;
                            }

                        delete [] mCustomRecordedGameData;
                        mCustomRecordedGameData = 
                            stringDuplicate( readCustomGameData );
                        }
                    else {
                        AppLog::error( 
                        "Hash check failed for custom data in playback file" );
                        }
                    }
                else {
                    AppLog::error( 
                        "Failed to parse playback header data" );

                    }
                delete [] readCustomGameData;                
                }
            delete [] fullFileName;
            }
        

        for( int i=0; i<numChildren; i++ ) {
            delete childFiles[i];
            }
        }
    delete [] childFiles;





    mStartedFullScreen = mFullScreen;

    setupSurface();
    

    SDL_WM_SetCaption( inWindowName, NULL );
    

    // turn off repeat
    SDL_EnableKeyRepeat( 0, 0 );

    SDL_EnableUNICODE( true );
    
    
    

    
    


    if( mRecordingEvents ) {
        File recordedGameDir( NULL, "recordedGames" );
    
        if( !recordedGameDir.exists() ) {
            recordedGameDir.makeDirectory();
            }


        // find next event recording file
        int fileNumber = 0;
        
        char hit = true;

        while( hit ) {
            fileNumber++;
            char *fileName = autoSprintf( "recordedGame%05d.txt", 
                                          fileNumber );
            File *file = recordedGameDir.getChildFile( fileName );
            
            delete [] fileName;
            
            if( !file->exists() ) {
                hit = false;
            
                char *fullFileName = file->getFullFileName();
                
                mEventFile = fopen( fullFileName, "w" );

                if( mEventFile == NULL ) {
                    AppLog::error( "Failed to open event recording file" );
                    }
                else {
                    AppLog::getLog()->logPrintf( 
                        Log::INFO_LEVEL,
                        "Recording game into file %s", fullFileName );

                    int fullScreenFlag = 0;
                    if( mFullScreen ) {
                        fullScreenFlag = 1;
                        }

                    char *stringToHash = autoSprintf( "%s%s",
                                                      mCustomRecordedGameData,
                                                      inHashSalt );

                    char *correctHash = computeSHA1Digest( stringToHash );
                    
                    delete [] stringToHash;
                    

                    fprintf( mEventFile, 
                             "%u seed, %u fps, %dx%d, fullScreen=%d, %s %s\n",
                             mRandSeed,
                             mMaxFrameRate, mWide, mHigh, fullScreenFlag,
                             mCustomRecordedGameData,
                             correctHash );

                    delete [] correctHash;
                    }

                delete [] fullFileName;                
                }
            delete file;
            }
        }
    

    for( int i=0; i<256; i++ ) {
        keyMap[i] = (unsigned char)i;
        }
    

    }



ScreenGL::~ScreenGL() {
	delete mViewPosition;
	delete mViewOrientation;
	delete mRedrawListenerVector;
	delete mMouseHandlerVector;
	delete mKeyboardHandlerVector;
	delete mJoystickHandlerVector;
	delete mSceneHandlerVector;

    if( mRecordingEvents ) {    
        writeEventBatchToFile();
        }
    
    if( mEventFile != NULL ) {
        fclose( mEventFile );
        mEventFile = NULL;
        }
        

    delete [] mCustomRecordedGameData;    
	}



void ScreenGL::startRecordingOrPlayback() {
    mRecordingOrPlaybackStarted = true;
    }



char screenGLStencilBufferSupported = false;




// aspect ratio rounded to nearest 1/100
// (16:9  is 177)
// Some screen resolutions, like 854x480, are not exact matches to their
// aspect ratio
int computeAspectRatio( int inW, int inH ) {

    int intRatio = (100 * inW ) / inH;

    return intRatio;
    }




void ScreenGL::setupSurface() {
#ifndef HAVE_GLES
    SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
    int flags = SDL_OPENGL;
#endif
    
#ifndef PANDORA
    // NOTE:  flags are also adjusted below if fullscreen resolution not
    // available
	if( mFullScreen ) {
        flags = flags | SDL_FULLSCREEN;
        }

    const SDL_VideoInfo* currentScreenInfo = SDL_GetVideoInfo();
    
    int currentW = currentScreenInfo->current_w;
    int currentH = currentScreenInfo->current_h;
    
    // aspect ratio
    int currentAspectRatio = computeAspectRatio( currentW, currentH );

    AppLog::getLog()->logPrintf( 
        Log::INFO_LEVEL,
        "Current screen configuration is %dx%d with aspect ratio %.2f",
        currentW, currentH, currentAspectRatio / 100.0f );



    // check for available modes
    SDL_Rect** modes;

    AppLog::getLog()->logPrintf( 
        Log::INFO_LEVEL,
        "Checking if requested video mode (%dx%d) is available",
        mWide, mHigh );
    
    // Get available fullscreen/hardware modes
    modes = SDL_ListModes( NULL, flags);

    // Check if there are any modes available
    if( modes == NULL ) {
        AppLog::criticalError( "ERROR:  No video modes available");
        exit(-1);
        }

    // Check if our resolution is restricted
    if( modes == (SDL_Rect**)-1 ) {
        AppLog::info( "All resolutions available" );
        }
    else if( mForceSpecifiedDimensions && mFullScreen ) {
        // check if specified dimension available in fullscreen
        
        char match = false;
        
        for( int i=0; modes[i] && ! match; ++i ) {
            if( mWide == modes[i]->w && 
                mHigh == modes[i]->h ) {
                match = true;
                }
            }
        
        if( !match ) {
            AppLog::getLog()->logPrintf( 
                Log::WARNING_LEVEL,
                "  Could not find a full-screen match for the forced screen "
                "dimensions %d x %d\n", mWide, mHigh );
            AppLog::warning( "Reverting to windowed mode" );
            
            mFullScreen = false;
            
#ifndef HAVE_GLES
            flags = SDL_OPENGL;
#else
            flags = 0;
#endif
            }
        }
    else{
        // Print valid modes

        // only count a match of BOTH resolution and aspect ratio
        char match = false;
        
        AppLog::info( "Available video modes:" );
        for( int i=0; modes[i]; ++i ) {
            AppLog::getLog()->logPrintf( Log::DETAIL_LEVEL,
                                         "   %d x %d\n", 
                                         modes[i]->w, 
                                         modes[i]->h );

            int thisAspectRatio = computeAspectRatio( modes[i]->w,
                                                      modes[i]->h );
            
            if( !mForceAspectRatio && thisAspectRatio == currentAspectRatio ) {
                AppLog::info( "   ^^^^ this mode matches current "
                              "aspect ratio" );
                }
            
            if( mWide == modes[i]->w && mHigh == modes[i]->h ) {
                AppLog::info( "   ^^^^ this mode matches requested mode" );
                
                if( ! mForceAspectRatio && 
                    thisAspectRatio != currentAspectRatio ) {
                    AppLog::info( "        but it doesn't match current "
                                  "aspect ratio" );
                    }
                else {
                    match = true;
                    }
                }
            }

        if( !match ) {
            AppLog::warning( "Warning:  No match for requested video mode" );
            AppLog::info( "Trying to find the closest match" );
            
            int bestDistance = 99999999;
            
            int bestIndex = -1;
            
            for( int i=0; modes[i]; ++i ) {
                // don't even consider modes that are SMALLER than our 
                // requested mode in either dimension
                if( modes[i]->w >= mWide &&
                    modes[i]->h >= mHigh ) {
                
                    int distance = (int)(
                        fabs( modes[i]->w - mWide ) +
                        fabs( modes[i]->h - mHigh ) );
                    
                    int thisAspectRatio = computeAspectRatio( modes[i]->w,
                                                              modes[i]->h );

                    if( ( mForceAspectRatio ||
                          thisAspectRatio == currentAspectRatio ) 
                        && 
                        distance < bestDistance ) {
                        
                        bestIndex = i;
                        bestDistance = distance;
                        }
                    }
                
                }
                    

            if( bestIndex != -1 ) {
                
                if( mForceAspectRatio ) {
                    AppLog::getLog()->logPrintf( 
                        Log::INFO_LEVEL,
                        "Picking closest available large-enough resolution:  "
                        "%d x %d\n", 
                        modes[bestIndex]->w, 
                        modes[bestIndex]->h );
                    }
                else {
                    AppLog::getLog()->logPrintf( 
                        Log::INFO_LEVEL,
                        "Picking closest available large-enough resolution "
                        "that matches current aspect ratio:  %d x %d\n", 
                        modes[bestIndex]->w, 
                        modes[bestIndex]->h );
                    }
                }
            else {
                // search again, ignoring aspect match

                if( !mForceAspectRatio ) {
                    
                    AppLog::warning( 
                        "Warning:  No match for current aspect ratio" );
                    AppLog::info( 
                        "Trying to find the closest off-ratio match" );

                
                    for( int i=0; modes[i]; ++i ) {
                        // don't even consider modes that are SMALLER than our 
                        // requested mode in either dimension
                        if( modes[i]->w >= mWide &&
                            modes[i]->h >= mHigh ) {
                            
                            int distance = (int)(
                                fabs( modes[i]->w - mWide ) +
                                fabs( modes[i]->h - mHigh ) );
                            
                            if( distance < bestDistance ) {
                                bestIndex = i;
                                bestDistance = distance;
                                }
                            }
                        }
                    }
                
                
                if( bestIndex != -1 ) {
                    AppLog::getLog()->logPrintf( 
                        Log::INFO_LEVEL,
                        "Picking closest available large-enough resolution:  "
                        "%d x %d\n", 
                        modes[bestIndex]->w, 
                        modes[bestIndex]->h );
                    }
                else {
                    AppLog::warning( 
                        "Warning:  No sufficiently sized resolution found" );
                    AppLog::info( 
                        "Considering closest-match smaller resolution" );
                    
                    for( int i=0; modes[i]; ++i ) {
                        int distance = (int)(
                            fabs( modes[i]->w - mWide ) +
                            fabs( modes[i]->h - mHigh ) );
                        
                        if( distance < bestDistance ) {
                            bestIndex = i;
                            bestDistance = distance;
                            }
                        }
                    
                    if( bestIndex != -1 ) {
                        AppLog::getLog()->logPrintf( 
                            Log::INFO_LEVEL,
                            "Picking closest available resolution:  "
                            "%d x %d\n", 
                            modes[bestIndex]->w, 
                            modes[bestIndex]->h );
                        }
                    else {
                        AppLog::criticalError( 
                            "ERROR:  No video modes available");
                        exit(-1);
                        }
                    }
                
                }


            mWide = modes[bestIndex]->w;
            mHigh = modes[bestIndex]->h;
            }
        
        }


#ifndef HAVE_GLES
    // 1-bit stencil buffer
    SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, 1 );

    // vsync to avoid tearing
    SDL_GL_SetAttribute( SDL_GL_SWAP_CONTROL, 1 );
#endif

    // current color depth
    SDL_Surface *screen = SDL_SetVideoMode( mWide, mHigh, 0, flags);
#else // PANDORA
    SDL_Surface *screen = SDL_SetVideoMode(800, 480, 0, SDL_SWSURFACE | SDL_FULLSCREEN);
#endif

#ifdef HAVE_GLES
    EGL_Init();
#endif

    if ( screen == NULL ) {
        printf( "Couldn't set %dx%d GL video mode: %s\n", 
                mWide, 
                mHigh,
                SDL_GetError() );
        }

#ifndef HAVE_GLES
    int setStencilSize;
    SDL_GL_GetAttribute( SDL_GL_STENCIL_SIZE, &setStencilSize );
    if( setStencilSize > 0 ) {
        // we have a stencil buffer
        screenGLStencilBufferSupported = true;
        }
#endif

    glEnable( GL_DEPTH_TEST );
	glEnable( GL_CULL_FACE );
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glCullFace( GL_BACK );
	glFrontFace( GL_CCW );
    }



void ScreenGL::writeEventBatchToFile() {
    
    int numInBatch = mEventBatch.size();
            
    if( mEventFile != NULL ) {
        if( numInBatch > 0 ) {
            
            char **allEvents = mEventBatch.getElementArray();
            char *eventString = join( allEvents, numInBatch, " " );
            
            fprintf( mEventFile, "%d %s\n", numInBatch, eventString );
        
            
            delete [] allEvents;
            delete [] eventString;
            }
        else {
            fprintf( mEventFile, "0\n" );
            }
        
        fflush( mEventFile );
        }
            
    for( int i=0; i<numInBatch; i++ ) {
        delete [] *( mEventBatch.getElement( i ) );
        }
    mEventBatch.deleteAll();
    }



void ScreenGL::playNextEventBatch() {
    // read and playback next batch
    int batchSize = 0;
    int numRead = fscanf( mEventFile, "%d", &batchSize );
            
    if( numRead == 0 || numRead == EOF ) {
        printf( "Reached end of recorded event file during playback\n" );
        // stop playback
        mPlaybackEvents = false;
        }
    

    for( int i=0; i<batchSize; i++ ) {
                
        char code[3];
        code[0] = '\0';
                
        fscanf( mEventFile, "%2s", code );
                
        switch( code[0] ) {
            case 'm':
                switch( code[1] ) {
                    case 'm': {
                        int x, y;
                        fscanf( mEventFile, "%d %d", &x, &y );
                                
                        callbackPassiveMotion( x, y );
                        }
                        break;
                    case 'd': {
                        int x, y;
                        fscanf( mEventFile, "%d %d", &x, &y );
                                
                        callbackMotion( x, y );
                        }
                        break;
                    case 'b': {
                        int button, state, x, y;
                        fscanf( mEventFile, "%d %d %d %d", 
                                &button, &state, &x, &y );
                                
                        if( state == 1 ) {
                            state = SDL_PRESSED;
                            }
                        else {
                            state = SDL_RELEASED;
                            }
                        
                        callbackMouse( button, state, x, y );
                        }
                        break;
                    }
                break;
            case 'k': {
                int c, x, y;
                fscanf( mEventFile, "%d %d %d", &c, &x, &y );

                switch( code[1] ) {
                    case 'd':          
                        callbackKeyboard( c, x, y );
                        break;
                    case 'u':
                        callbackKeyboardUp( c, x, y );                        
                        break;
                    }
                }
                break;
            case 's': {
                int c, x, y;
                fscanf( mEventFile, "%d %d %d", &c, &x, &y );

                switch( code[1] ) {
                    case 'd':          
                        callbackSpecialKeyboard( c, x, y );
                        break;
                    case 'u':
                        callbackSpecialKeyboardUp( c, x, y );
                        break;
                    }
                }
                break;
            default:
                AppLog::error( "Unknown code '%s' in playback file\n",
                               code );
            }            
        
        }


    mNumBatchesPlayed++;
    }




const char *ScreenGL::getCustomRecordedGameData() {
    return mCustomRecordedGameData;
    }



char ScreenGL::isPlayingBack() {
    return mPlaybackEvents;
    }


float ScreenGL::getPlaybackDoneFraction() {
    if( mEventFileNumBatches == 0 || mEventFile == NULL ) {
        return 0;
        }
    
    return mNumBatchesPlayed / (float)mEventFileNumBatches;    
    }



char ScreenGL::shouldShowPlaybackDisplay() {
    return mShouldShowPlaybackDisplay;
    }


char ScreenGL::isMinimized() {
    return mMinimized;
    }





void ScreenGL::start() {
	currentScreenGL = this;

    // call our resize callback (GLUT used to do this for us when the
    // window was created)
    callbackResize( mWide, mHigh );


    // oversleep on last loop (discount it from next sleep)
    // can be negative (add to next sleep)
    int oversleepMSec = 0;


#ifdef PANDORA
    SDL_Joystick *nub0, *nub1;
    SDL_JoystickEventState(SDL_ENABLE);
    nub0 = SDL_JoystickOpen(1);
    nub1 = SDL_JoystickOpen(2);

    int lastJoyX[] = {0, 0, 0};
    int lastJoyY[] = {0, 0, 0};
#endif
    // main loop
    while( true ) {
        
        unsigned long frameStartSec, frameStartMSec;
        
        Time::getCurrentTime( &frameStartSec, &frameStartMSec );


        // pre-display first, this might involve a sleep for frame timing
        // purposes
        callbackPreDisplay();
        


        // now handle pending events BEFORE actually drawing the screen.
        // Thus, screen reflects all the latest events (not just those
        // that happened before any sleep called during the pre-display).
        // This makes controls much more responsive.

        

        SDL_Event event;
        
        while( !( mPlaybackEvents && mRecordingOrPlaybackStarted )
               && SDL_PollEvent( &event ) ) {
            
            SDLMod mods = SDL_GetModState();

            // alt-enter, toggle fullscreen (but only if we started there,
            // to prevent window content centering issues due to mWidth and
            // mHeight changes mid-game)
            if( mStartedFullScreen && 
                event.type == SDL_KEYDOWN && 
                event.key.keysym.sym == SDLK_RETURN && 
                ( ( mods & KMOD_META ) || ( mods & KMOD_ALT ) ) ) {  
                printf( "Toggling fullscreen\n" );
                
                mFullScreen = !mFullScreen;
                
                setupSurface();

                callbackResize( mWide, mHigh );

                // reload all textures into OpenGL
                SingleTextureGL::contextChanged();
                }
#ifndef HAVE_GLES
            // alt-tab when not in fullscreen mode
            else if( ! mFullScreen &&
                     ! mMinimized &&
                     event.type == SDL_KEYDOWN && 
                     event.key.keysym.sym == SDLK_TAB && 
                     ( ( mods & KMOD_META ) || ( mods & KMOD_ALT ) ) ) {
                
                mWantToMimimize = true;
                mWasFullScreenBeforeMinimize = false;

                if( SDL_WM_GrabInput( SDL_GRAB_QUERY ) == SDL_GRAB_ON ) {
                    mWasInputGrabbedBeforeMinimize = true;
                    }
                else {
                    mWasInputGrabbedBeforeMinimize = false;
                    }
                SDL_WM_GrabInput( SDL_GRAB_OFF );
                }
#endif
            // handle alt-tab to minimize out of full-screen mode
            else if( mFullScreen &&
                     ! mMinimized &&
                     event.type == SDL_KEYDOWN && 
                     event.key.keysym.sym == SDLK_TAB && 
                     ( ( mods & KMOD_META ) || ( mods & KMOD_ALT ) ) ) { 
                
                printf( "Minimizing from fullscreen on Alt-tab\n" );

                mFullScreen = false;
                
                setupSurface();

                callbackResize( mWide, mHigh );

                // reload all textures into OpenGL
                SingleTextureGL::contextChanged();

                mWantToMimimize = true;
                mWasFullScreenBeforeMinimize = true;

#ifndef HAVE_GLES
                if( SDL_WM_GrabInput( SDL_GRAB_QUERY ) == SDL_GRAB_ON ) {
                    mWasInputGrabbedBeforeMinimize = true;
                    }
                else {
                    mWasInputGrabbedBeforeMinimize = false;
                    }
                SDL_WM_GrabInput( SDL_GRAB_OFF );
#endif
                }
            // active event after minimizing from windowed mode
            else if( mMinimized && 
                     ! mWasFullScreenBeforeMinimize &&
                     event.type == SDL_ACTIVEEVENT && 
                     event.active.gain && 
                     event.active.state == SDL_APPACTIVE ) {
                // window becoming active out of minimization, needs
                // to return to full-screen mode

                printf( "Restoring to window after Alt-tab\n" );

                mWantToMimimize = false;
                mWasFullScreenBeforeMinimize = false;
                mMinimized = false;

#ifndef HAVE_GLES
                if( mWasInputGrabbedBeforeMinimize ) {
                    SDL_WM_GrabInput( SDL_GRAB_ON );
                    }
#endif
                }
            // active event after minimizing from fullscreen mode
            else if( mMinimized && 
                     mWasFullScreenBeforeMinimize &&
                     event.type == SDL_ACTIVEEVENT && 
                     event.active.gain && 
                     event.active.state == SDL_APPACTIVE ) {
                // window becoming active out of minimization, needs
                // to return to full-screen mode

                printf( "Restoring to fullscreen after Alt-tab\n" );

                mFullScreen = true;
                
                setupSurface();

                callbackResize( mWide, mHigh );

                // reload all textures into OpenGL
                SingleTextureGL::contextChanged();
                
                mWantToMimimize = false;
                mWasFullScreenBeforeMinimize = false;
                mMinimized = false;

#ifndef HAVE_GLES
                if( mWasInputGrabbedBeforeMinimize ) {
                    SDL_WM_GrabInput( SDL_GRAB_ON );
                    }
#endif
                }
            // map CTRL-q to ESC
            // 17 is "DC1" which is ctrl-q on some platforms
            else if( event.type == SDL_KEYDOWN &&
                     ( ( event.key.keysym.sym == SDLK_q
                         &&
                         ( ( mods & KMOD_META ) || ( mods & KMOD_ALT )
                           || ( mods & KMOD_CTRL ) ) )
                       ||
                       ( ( event.key.keysym.unicode & 0xFF ) == 17 ) ) ) {
                
                // map to 27, escape
                int mouseX, mouseY;
                SDL_GetMouseState( &mouseX, &mouseY );
                
                callbackKeyboard( 27, mouseX, mouseY );    
                }
            else {
                

            switch( event.type ) {
                case SDL_QUIT: {
                    // map to 27, escape
                    int mouseX, mouseY;
                    SDL_GetMouseState( &mouseX, &mouseY );

                    callbackKeyboard( 27, mouseX, mouseY );
                    }
                    break;
                case SDL_KEYDOWN:
                case SDL_KEYUP: {
                    int mouseX, mouseY;
                    SDL_GetMouseState( &mouseX, &mouseY );
                    
                    
                    // check if special key
                    int mgKey = mapSDLSpecialKeyToMG( event.key.keysym.sym );
                    
                    if( mgKey != 0 ) {
                        if( event.type == SDL_KEYDOWN ) {
                            callbackSpecialKeyboard( mgKey, mouseX, mouseY );
                            }
                        else {
                            callbackSpecialKeyboardUp( mgKey, mouseX, mouseY );
                            }
                        }
                    else {
                        unsigned char asciiKey;

                        // try unicode first, if 8-bit clean (extended ASCII)
                        if( ( event.key.keysym.unicode & 0xFF00 ) == 0 &&
                            ( event.key.keysym.unicode & 0x00FF ) != 0 ) {
                            asciiKey = event.key.keysym.unicode & 0xFF;
                            }
                        else {
                            // else unicode-to-ascii failed

                            // fall back
                            asciiKey = 
                                mapSDLKeyToASCII( event.key.keysym.sym );
                            }

                      
                        if( asciiKey != 0 ) {
                            // shift and caps cancel each other
                            if( ( ( event.key.keysym.mod & KMOD_SHIFT )
                                  &&
                                  !( event.key.keysym.mod & KMOD_CAPS ) )
                                ||
                                ( !( event.key.keysym.mod & KMOD_SHIFT )
                                  &&
                                  ( event.key.keysym.mod & KMOD_CAPS ) ) ) {
                                
                                asciiKey = toupper( asciiKey );
                                }
                        
                            if( event.type == SDL_KEYDOWN ) {
                                callbackKeyboard( asciiKey, mouseX, mouseY );
                                }
                            else {
                                callbackKeyboardUp( asciiKey, mouseX, mouseY );
                                }
                            }
                        }
                    }
                    break;
                case SDL_MOUSEMOTION:
                    if( event.motion.state & SDL_BUTTON( 1 )
                        || 
                        event.motion.state & SDL_BUTTON( 2 )
                        ||
                        event.motion.state & SDL_BUTTON( 3 ) ) {
                        
                        callbackMotion( event.motion.x, event.motion.y );
                        }
                    else {
                        callbackPassiveMotion( event.motion.x, 
                                               event.motion.y );
                        }
                    break;
                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP:
                    callbackMouse( event.button.button,
                                   event.button.state,
                                   event.button.x, 
                                   event.button.y );
                    break;
#ifdef PANDORA
                case SDL_JOYAXISMOTION:
                    if (event.jaxis.axis == 0) { // x
                        lastJoyX[event.jaxis.which] = event.jaxis.value;
                        callbackJoystick(event.jaxis.which, event.jaxis.value, lastJoyY[event.jaxis.which]);
                    } else { // y
                        lastJoyY[event.jaxis.which] = event.jaxis.value;
                        callbackJoystick(event.jaxis.which, lastJoyX[event.jaxis.which], event.jaxis.value);
                    }
                    break;
                    }
                }
#endif
            }

        // record them?
        if( mRecordingEvents && mRecordingOrPlaybackStarted ) {
            writeEventBatchToFile();
            }

        if( mPlaybackEvents && mRecordingOrPlaybackStarted && 
            mEventFile != NULL ) {
            
            playNextEventBatch();


            // dump events, but responde to ESC to stop playback
            // let player take over from that point
            while( SDL_PollEvent( &event ) ) {
                SDLMod mods = SDL_GetModState();
                // map CTRL-q to ESC
                // 17 is "DC1" which is ctrl-q on some platforms
                if( event.type == SDL_KEYDOWN &&
                    ( ( event.key.keysym.sym == SDLK_q
                        &&
                        ( ( mods & KMOD_META ) || ( mods & KMOD_ALT )
                          || ( mods & KMOD_CTRL ) ) )
                      ||
                      ( ( event.key.keysym.unicode & 0xFF ) == 17 ) ) ) {
                    
                    // map to 27, escape
                    int mouseX, mouseY;
                    SDL_GetMouseState( &mouseX, &mouseY );
                    
                    printf( "User terminated recorded event playback "
                            "with ESC\n" );
        
                    // stop playback
                    mPlaybackEvents = false;
                    }
                else {
                    switch( event.type ) {
                        case SDL_QUIT: {
                            // map to 27, escape
                            int mouseX, mouseY;
                            SDL_GetMouseState( &mouseX, &mouseY );
                            
                            // actual quit event, still pass through 
                            // as ESC to signal a full quit
                            callbackKeyboard( 27, mouseX, mouseY );
                            }
                            break;
                        case SDL_KEYDOWN: {
                            
                            unsigned char asciiKey;
                            
                            // try unicode first, 
                            // if 8-bit clean (extended ASCII)
                            if( ( event.key.keysym.unicode & 0xFF00 ) == 0 &&
                                ( event.key.keysym.unicode & 0x00FF ) != 0 ) {
                                asciiKey = event.key.keysym.unicode & 0xFF;
                                }
                            else {
                                // else unicode-to-ascii failed

                                // fall back
                                asciiKey = 
                                    mapSDLKeyToASCII( event.key.keysym.sym );
                                }
                            if( asciiKey == 27 ) {
                                // pass ESC through
                                // map to 27, escape
                                int mouseX, mouseY;
                                SDL_GetMouseState( &mouseX, &mouseY );
                                
                                printf( 
                                    "User terminated recorded event playback "
                                    "with ESC\n" );

                                // stop playback
                                mPlaybackEvents = false;
                                }
                            else if( asciiKey == '%' ) {
                                mShouldShowPlaybackDisplay =
                                    ! mShouldShowPlaybackDisplay;
                                }
                            if( mAllowSlowdownKeysDuringPlayback ) {
                                
                                if( asciiKey == '^' ) {
                                    setMaxFrameRate( 2 );
                                    }
                                else if( asciiKey == '&' ) {
                                    setMaxFrameRate( mFullFrameRate );
                                    }
                                else if( asciiKey == '*' ) {
                                    // fast forward
                                    setMaxFrameRate( mFullFrameRate * 2 );
                                    }
                                else if( asciiKey == '(' ) {
                                    // fast forward
                                    setMaxFrameRate( mFullFrameRate * 4 );
                                    }
                                }
                            }
                        }                    
                    }
                
                }
            
            if( !mPlaybackEvents ) {
                // playback ended
                // send through full spectrum of release events
                // so no presses linger after playback end
               
                int mouseX, mouseY;
                SDL_GetMouseState( &mouseX, &mouseY );
                callbackMouse( SDL_BUTTON_LEFT, 
                               SDL_RELEASED, mouseX, mouseY );

                callbackMouse( SDL_BUTTON_MIDDLE, 
                               SDL_RELEASED, mouseX, mouseY );

                callbackMouse( SDL_BUTTON_RIGHT, 
                               SDL_RELEASED, mouseX, mouseY );

                callbackMouse( SDL_BUTTON_WHEELUP, 
                               SDL_RELEASED, mouseX, mouseY );

                callbackMouse( SDL_BUTTON_WHEELDOWN, 
                               SDL_RELEASED, mouseX, mouseY );

                for( int i=0; i<255; i++ ) {
                    callbackKeyboardUp( i, mouseX, mouseY );
                    }
                for( int i=MG_KEY_FIRST_CODE; i<=MG_KEY_LAST_CODE; i++ ) {
                    callbackSpecialKeyboardUp( i, mouseX, mouseY );
                    }
                }
            

            }
        
        



        // now all events handled, actually draw the screen
        callbackDisplay();

        int frameTime =
            Time::getMillisecondsSince( frameStartSec, frameStartMSec );
        
    
        // lock down to mMaxFrameRate frames per second
        int minFrameTime = 1000 / mMaxFrameRate;
        if( ( frameTime + oversleepMSec ) < minFrameTime ) {
            int timeToSleep = 
                minFrameTime - ( frameTime + oversleepMSec );
        
            //SDL_Delay( timeToSleep );
            unsigned long sleepStartSec, sleepStartMSec;
            Time::getCurrentTime( &sleepStartSec, &sleepStartMSec );
            
            Thread::staticSleep( timeToSleep );

            int actualSleepTime = 
                Time::getMillisecondsSince( sleepStartSec, sleepStartMSec );
            
            oversleepMSec = actualSleepTime - timeToSleep;
            }
        else { 
            oversleepMSec = 0;
            }
        
        }
    SDL_JoystickClose(nub0); 
    SDL_JoystickClose(nub1); 
    }



void ScreenGL::setMaxFrameRate( unsigned int inMaxFrameRate ) {
    mMaxFrameRate = inMaxFrameRate;
    }



unsigned int ScreenGL::getMaxFramerate() {
    return mMaxFrameRate;
    }


unsigned int ScreenGL::getRandSeed() {
    return mRandSeed;
    }


void ScreenGL::setKeyMapping( unsigned char inFromKey,
                              unsigned char inToKey ) {
    keyMap[ inFromKey ] = inToKey;

    AppLog::getLog()->logPrintf( 
        Log::INFO_LEVEL,
        "Mapping key '%c' to '%c'", inFromKey, inToKey );
    }

        



void ScreenGL::switchTo2DMode() {
    m2DMode = true;    
    }



void ScreenGL::setFullScreen() {
    //glutFullScreen();
    }



void ScreenGL::changeWindowSize( int inWidth, int inHeight ) {
    //glutReshapeWindow( inWidth, inHeight );
    }



void ScreenGL::applyViewTransform() {
    // compute view angle

    // default angle is 90, but we want to force a 1:1 aspect ratio to avoid
    // distortion.
    // If our screen's width is different than its height, we need to decrease
    // the view angle so that the angle coresponds to the smaller dimension.
    // This keeps the zoom factor constant in the smaller dimension.

    // Of course, because of the way perspective works, only one Z-slice
    // will have a constant zoom factor
    // The zSlice variable sets the distance of this slice from the picture
    // plane
    double zSlice = .31;

    double maxDimension = mWide;
    if( mHigh > mWide ) {
        maxDimension = mHigh;
        }
    double aspectDifference = fabs( mWide / 2 - mHigh / 2 ) / maxDimension;
    // default angle is 90 degrees... half the angle is PI/4
    double angle = atan( tan( M_PI / 4 ) +
                         aspectDifference / zSlice );

    // double it to get the full angle
    angle *= 2;
    
    
    // convert to degrees
    angle = 360 * angle / ( 2 * M_PI );


    // set up the projection matrix
    glMatrixMode( GL_PROJECTION );

	glLoadIdentity();
    
    //gluPerspective( 90, mWide / mHigh, 1, 9999 );
    gluPerspective( angle,
                    1,
                    1, 9999 );

    
    // set up the model view matrix
    glMatrixMode( GL_MODELVIEW );
    
    glLoadIdentity();

	// create default view and up vectors, 
	// then rotate them by orientation angle
	Vector3D *viewDirection = new Vector3D( 0, 0, 1 );
	Vector3D *upDirection = new Vector3D( 0, 1, 0 );
	
	viewDirection->rotate( mViewOrientation );
	upDirection->rotate( mViewOrientation );
	
	// get a point out in front of us in the view direction
	viewDirection->add( mViewPosition );
	
	// look at takes a viewer position, 
	// a point to look at, and an up direction
	gluLookAt( mViewPosition->mX, 
				mViewPosition->mY, 
				mViewPosition->mZ,
				viewDirection->mX, 
				viewDirection->mY, 
				viewDirection->mZ,
				upDirection->mX, 
				upDirection->mY, 
				upDirection->mZ );
	
	delete viewDirection;
	delete upDirection;
    }



void callbackResize( int inW, int inH ) {	
	ScreenGL *s = currentScreenGL;
	s->mWide = inW;
	s->mHigh = inH;

    
    int bigDimension = s->mImageWide;
    if( bigDimension < s->mImageHigh ) {
        bigDimension = s->mImageHigh;
        }
    
    int excessW = s->mWide - bigDimension;
    int excessH = s->mHigh - bigDimension;
    
    // viewport is square of biggest image dimension, centered on screen
    glViewport( excessW / 2,
                excessH / 2, 
                bigDimension,
                bigDimension );
    }



void callbackKeyboard( unsigned char inKey, int inX, int inY ) {
    // all playback events are already mapped
    if( ! ( currentScreenGL->mPlaybackEvents && 
            currentScreenGL->mRecordingOrPlaybackStarted ) ) {
        inKey = keyMap[inKey];
        }
    
    if( currentScreenGL->mRecordingEvents && 
        currentScreenGL->mRecordingOrPlaybackStarted ) {
        
        char *eventString = autoSprintf( "kd %d %d %d", inKey, inX, inY );
        
        currentScreenGL->mEventBatch.push_back( eventString );
        }


	char someFocused = currentScreenGL->isKeyboardHandlerFocused();

    int h;
    // flag those that exist right now
    // because handlers might remove themselves or add new handlers,
    // and we don't want to fire to those that weren't present when
    // callback was called
    
    for( h=0; h<currentScreenGL->mKeyboardHandlerVector->size(); h++ ) {
        KeyboardHandlerGL *handler 
			= *( currentScreenGL->mKeyboardHandlerVector->getElement( h ) );
        handler->mHandlerFlagged = true;
        }    
		
	// fire to all handlers, stop if eaten
	for( h=0; h<currentScreenGL->mKeyboardHandlerVector->size(); h++ ) {
		KeyboardHandlerGL *handler 
			= *( currentScreenGL->mKeyboardHandlerVector->getElement( h ) );

        if( handler->mHandlerFlagged ) {
            // if some are focused, only fire to this handler if it is one
            // of the focused handlers
            if( !someFocused || handler->isFocused() ) {
                handler->keyPressed( inKey, inX, inY );
                if( handler->mEatEvent ) {
                    handler->mEatEvent = false;
                    goto down_eaten;                  
                    }
                }
            }
		}

    down_eaten:
    


    // deflag for next time
    for( h=0; h<currentScreenGL->mKeyboardHandlerVector->size(); h++ ) {
        KeyboardHandlerGL *handler 
			= *( currentScreenGL->mKeyboardHandlerVector->getElement( h ) );
        handler->mHandlerFlagged = false;
        }
	}



void callbackKeyboardUp( unsigned char inKey, int inX, int inY ) {
    // all playback events are already mapped
    if( ! ( currentScreenGL->mPlaybackEvents &&
            currentScreenGL->mRecordingOrPlaybackStarted ) ) {
        inKey = keyMap[inKey];
        }
    
    if( currentScreenGL->mRecordingEvents &&
        currentScreenGL->mRecordingOrPlaybackStarted ) {

        char *eventString = autoSprintf( "ku %d %d %d", inKey, inX, inY );
        
        currentScreenGL->mEventBatch.push_back( eventString );
        }

	char someFocused = currentScreenGL->isKeyboardHandlerFocused();
	
    int h;
    // flag those that exist right now
    // because handlers might remove themselves or add new handlers,
    // and we don't want to fire to those that weren't present when
    // callback was called
    
    for( h=0; h<currentScreenGL->mKeyboardHandlerVector->size(); h++ ) {
        KeyboardHandlerGL *handler 
			= *( currentScreenGL->mKeyboardHandlerVector->getElement( h ) );
        handler->mHandlerFlagged = true;
        }

	// fire to all handlers, stop if eaten
	for( h=0; h<currentScreenGL->mKeyboardHandlerVector->size(); h++ ) {
		KeyboardHandlerGL *handler 
			= *( currentScreenGL->mKeyboardHandlerVector->getElement( h ) );

        if( handler->mHandlerFlagged ) {
            
            // if some are focused, only fire to this handler if it is one
            // of the focused handlers
            if( !someFocused || handler->isFocused() ) {
                handler->keyReleased( inKey, inX, inY );
                if( handler->mEatEvent ) {
                    handler->mEatEvent = false;
                    goto up_eaten;                  
                    }
                }
            }
        }

    up_eaten:
    

    // deflag for next time
    for( h=0; h<currentScreenGL->mKeyboardHandlerVector->size(); h++ ) {
        KeyboardHandlerGL *handler 
			= *( currentScreenGL->mKeyboardHandlerVector->getElement( h ) );
        handler->mHandlerFlagged = false;
        }
    
	}	

	
	
void callbackSpecialKeyboard( int inKey, int inX, int inY ) {
    if( currentScreenGL->mRecordingEvents &&
        currentScreenGL->mRecordingOrPlaybackStarted ) {

        char *eventString = autoSprintf( "sd %d %d %d", inKey, inX, inY );
        
        currentScreenGL->mEventBatch.push_back( eventString );
        }


	char someFocused = currentScreenGL->isKeyboardHandlerFocused();
	
    int h;
    // flag those that exist right now
    // because handlers might remove themselves or add new handlers,
    // and we don't want to fire to those that weren't present when
    // callback was called
    
    for( h=0; h<currentScreenGL->mKeyboardHandlerVector->size(); h++ ) {
        KeyboardHandlerGL *handler 
			= *( currentScreenGL->mKeyboardHandlerVector->getElement( h ) );
        handler->mHandlerFlagged = true;
        }


	// fire to all handlers, stop if eaten
	for( h=0; h<currentScreenGL->mKeyboardHandlerVector->size(); h++ ) {
		KeyboardHandlerGL *handler 
			= *( currentScreenGL->mKeyboardHandlerVector->getElement( h ) );
		
        if( handler->mHandlerFlagged ) {
            
            // if some are focused, only fire to this handler if it is one
            // of the focused handlers
            if( !someFocused || handler->isFocused() ) {
                handler->specialKeyPressed( inKey, inX, inY );
                if( handler->mEatEvent ) {
                    handler->mEatEvent = false;
                    goto special_down_eaten;                  
                    }
                }
            }
        }

    special_down_eaten:
    
    // deflag for next time
    for( h=0; h<currentScreenGL->mKeyboardHandlerVector->size(); h++ ) {
        KeyboardHandlerGL *handler 
			= *( currentScreenGL->mKeyboardHandlerVector->getElement( h ) );
        handler->mHandlerFlagged = false;
        }
	}
	


void callbackSpecialKeyboardUp( int inKey, int inX, int inY ) {
    if( currentScreenGL->mRecordingEvents &&
        currentScreenGL->mRecordingOrPlaybackStarted ) {

        char *eventString = autoSprintf( "su %d %d %d", inKey, inX, inY );
        
        currentScreenGL->mEventBatch.push_back( eventString );
        }


	char someFocused = currentScreenGL->isKeyboardHandlerFocused();

    int h;
    // flag those that exist right now
    // because handlers might remove themselves or add new handlers,
    // and we don't want to fire to those that weren't present when
    // callback was called
    
    for( h=0; h<currentScreenGL->mKeyboardHandlerVector->size(); h++ ) {
        KeyboardHandlerGL *handler 
			= *( currentScreenGL->mKeyboardHandlerVector->getElement( h ) );
        handler->mHandlerFlagged = true;
        }
	
	// fire to all handlers, stop if eaten
	for( h=0; h<currentScreenGL->mKeyboardHandlerVector->size(); h++ ) {
		KeyboardHandlerGL *handler 
			= *( currentScreenGL->mKeyboardHandlerVector->getElement( h ) );

        if( handler->mHandlerFlagged ) {
            
            // if some are focused, only fire to this handler if it is one
            // of the focused handlers
            if( !someFocused || handler->isFocused() ) {
                handler->specialKeyReleased( inKey, inX, inY );
                if( handler->mEatEvent ) {
                    handler->mEatEvent = false;
                    goto special_up_eaten;                  
                    }
                }
            }
        }

    special_up_eaten:
    
    // deflag for next time
    for( h=0; h<currentScreenGL->mKeyboardHandlerVector->size(); h++ ) {
        KeyboardHandlerGL *handler 
			= *( currentScreenGL->mKeyboardHandlerVector->getElement( h ) );
        handler->mHandlerFlagged = false;
        }
    
	}	


	
void callbackMotion( int inX, int inY ) {
    if( currentScreenGL->mRecordingEvents && 
        currentScreenGL->mRecordingOrPlaybackStarted ) {

        char *eventString = autoSprintf( "md %d %d", inX, inY );
        
        currentScreenGL->mEventBatch.push_back( eventString );
        }

	// fire to all handlers
    int h;
    // flag those that exist right now
    // because handlers might remove themselves or add new handlers,
    // and we don't want to fire to those that weren't present when
    // callback was called
    
    for( h=0; h<currentScreenGL->mMouseHandlerVector->size(); h++ ) {
        MouseHandlerGL *handler 
			= *( currentScreenGL->mMouseHandlerVector->getElement( h ) );
        handler->mHandlerFlagged = true;
        }

	for( h=0; h<currentScreenGL->mMouseHandlerVector->size(); h++ ) {
		MouseHandlerGL *handler 
			= *( currentScreenGL->mMouseHandlerVector->getElement( h ) );
		if( handler->mHandlerFlagged ) {
            handler->mouseDragged( inX, inY );
            }
		}

    // deflag for next time
    for( h=0; h<currentScreenGL->mMouseHandlerVector->size(); h++ ) {
        MouseHandlerGL *handler 
			= *( currentScreenGL->mMouseHandlerVector->getElement( h ) );
        handler->mHandlerFlagged = false;
        }
	}
	


void callbackPassiveMotion( int inX, int inY ) {

    if( currentScreenGL->mRecordingEvents &&
        currentScreenGL->mRecordingOrPlaybackStarted ) {

        char *eventString = autoSprintf( "mm %d %d", inX, inY );
        
        currentScreenGL->mEventBatch.push_back( eventString );
        }

	// fire to all handlers
    int h;
    // flag those that exist right now
    // because handlers might remove themselves or add new handlers,
    // and we don't want to fire to those that weren't present when
    // callback was called
    
    for( h=0; h<currentScreenGL->mMouseHandlerVector->size(); h++ ) {
        MouseHandlerGL *handler 
			= *( currentScreenGL->mMouseHandlerVector->getElement( h ) );
        handler->mHandlerFlagged = true;
        }

	for( h=0; h<currentScreenGL->mMouseHandlerVector->size(); h++ ) {
		MouseHandlerGL *handler 
			= *( currentScreenGL->mMouseHandlerVector->getElement( h ) );
		
        if( handler->mHandlerFlagged ) {
            handler->mouseMoved( inX, inY );
            }
		}

    // deflag for next time
    for( h=0; h<currentScreenGL->mMouseHandlerVector->size(); h++ ) {
        MouseHandlerGL *handler 
			= *( currentScreenGL->mMouseHandlerVector->getElement( h ) );
        handler->mHandlerFlagged = false;
        }
	}			
	
	
	
void callbackMouse( int inButton, int inState, int inX, int inY ) {
	
    // ignore wheel events
    if( inButton == SDL_BUTTON_WHEELUP ||
        inButton == SDL_BUTTON_WHEELDOWN ) {
        return;
        }
    
    if( currentScreenGL->mRecordingEvents &&
        currentScreenGL->mRecordingOrPlaybackStarted ) {

        int stateEncoding = 0;
        if( inState == SDL_PRESSED ) {
            stateEncoding = 1;
            }
        
        char *eventString = autoSprintf( "mb %d %d %d %d",
                                         inButton, stateEncoding, inX, inY );
        
        currentScreenGL->mEventBatch.push_back( eventString );
        }
    

    // fire to all handlers
    int h;
    
    // flag those that exist right now
    // because handlers might remove themselves or add new handlers,
    // and we don't want to fire to those that weren't present when
    // callbackMouse was called
    
    for( h=0; h<currentScreenGL->mMouseHandlerVector->size(); h++ ) {
        MouseHandlerGL *handler 
			= *( currentScreenGL->mMouseHandlerVector->getElement( h ) );
        handler->mHandlerFlagged = true;
        }
    
	for( h=0; h<currentScreenGL->mMouseHandlerVector->size(); h++ ) {
		MouseHandlerGL *handler 
			= *( currentScreenGL->mMouseHandlerVector->getElement( h ) );
		
        if( handler->mHandlerFlagged ) {
            
            handler->mouseMoved( inX, inY );
            if( inState == SDL_PRESSED ) {
                handler->mousePressed( inX, inY );
                }
            else if( inState == SDL_RELEASED ) {
                handler->mouseReleased( inX, inY );
                }
            else {
                printf( "Error:  Unknown mouse state received from SDL\n" );
                }	
            }
		}

    // deflag for next time
    for( h=0; h<currentScreenGL->mMouseHandlerVector->size(); h++ ) {
        MouseHandlerGL *handler 
			= *( currentScreenGL->mMouseHandlerVector->getElement( h ) );
        handler->mHandlerFlagged = false;
        }

	}

void callbackJoystick( int num, int inX, int inY ) {
    // fire to all handlers
    int h;

    // flag those that exist right now
    // because handlers might remove themselves or add new handlers,
    // and we don't want to fire to those that weren't present when
    // callbackJoystick was called

    for( h=0; h<currentScreenGL->mJoystickHandlerVector->size(); h++ ) {
        JoystickHandlerGL *handler 
            = *( currentScreenGL->mJoystickHandlerVector->getElement( h ) );
        handler->mHandlerFlagged = true;
    }

    for( h=0; h<currentScreenGL->mJoystickHandlerVector->size(); h++ ) {
        JoystickHandlerGL *handler 
            = *( currentScreenGL->mJoystickHandlerVector->getElement( h ) );

        if( handler->mHandlerFlagged ) {
            handler->joystickMoved( num, inX, inY );
        }
    }
}
	
void callbackPreDisplay() {
	ScreenGL *s = currentScreenGL;
	
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );


    // fire to all redraw listeners
    // do this first so that they can update our view transform
    // this makes control much more responsive
	for( int r=0; r<s->mRedrawListenerVector->size(); r++ ) {
		RedrawListenerGL *listener 
			= *( s->mRedrawListenerVector->getElement( r ) );
		listener->fireRedraw();
		}
    }



void callbackDisplay() {
    ScreenGL *s = currentScreenGL;

	if( ! s->m2DMode ) {    
        // apply our view transform
        s->applyViewTransform();
        }


	// fire to all handlers
	for( int h=0; h<currentScreenGL->mSceneHandlerVector->size(); h++ ) {
		SceneHandlerGL *handler 
			= *( currentScreenGL->mSceneHandlerVector->getElement( h ) );
		handler->drawScene();
		}

    for( int r=0; r<s->mRedrawListenerVector->size(); r++ ) {
		RedrawListenerGL *listener 
			= *( s->mRedrawListenerVector->getElement( r ) );
		listener->postRedraw();
		}

#ifndef HAVE_GLES
	SDL_GL_SwapBuffers();
#else
	EGL_SwapBuffers();
#endif

    // thanks to Andrew McClure for the idea of doing this AFTER
    // the next redraw (for pretty minimization)
    if( s->mWantToMimimize ) {
        s->mWantToMimimize = false;
        SDL_WM_IconifyWindow();
        s->mMinimized = true;
        }
    }



void callbackIdle() {
	//glutPostRedisplay();
	}		



int mapSDLSpecialKeyToMG( int inSDLKey ) {
    switch( inSDLKey ) {
        case SDLK_F1: return MG_KEY_F1;
        case SDLK_F2: return MG_KEY_F2;
        case SDLK_F3: return MG_KEY_F3;
        case SDLK_F4: return MG_KEY_F4;
        case SDLK_F5: return MG_KEY_F5;
        case SDLK_F6: return MG_KEY_F6;
        case SDLK_F7: return MG_KEY_F7;
        case SDLK_F8: return MG_KEY_F8;
        case SDLK_F9: return MG_KEY_F9;
        case SDLK_F10: return MG_KEY_F10;
        case SDLK_F11: return MG_KEY_F11;
        case SDLK_F12: return MG_KEY_F12;
        case SDLK_LEFT: return MG_KEY_LEFT;
        case SDLK_UP: return MG_KEY_UP;
        case SDLK_RIGHT: return MG_KEY_RIGHT;
        case SDLK_DOWN: return MG_KEY_DOWN;
        case SDLK_PAGEUP: return MG_KEY_PAGE_UP;
        case SDLK_PAGEDOWN: return MG_KEY_PAGE_DOWN;
        case SDLK_HOME: return MG_KEY_HOME;
        case SDLK_END: return MG_KEY_END;
        case SDLK_INSERT: return MG_KEY_INSERT;
        case SDLK_RSHIFT: return MG_KEY_RSHIFT;
        case SDLK_LSHIFT: return MG_KEY_LSHIFT;
        case SDLK_RCTRL: return MG_KEY_RCTRL;
        case SDLK_LCTRL: return MG_KEY_LCTRL;
        case SDLK_RALT: return MG_KEY_RALT;
        case SDLK_LALT: return MG_KEY_LALT;
        case SDLK_RMETA: return MG_KEY_RMETA;
        case SDLK_LMETA: return MG_KEY_LMETA;
        default: return 0;
        }
    }


char mapSDLKeyToASCII( int inSDLKey ) {
    // map world keys  (SDLK_WORLD_X) directly to ASCII
    if( inSDLKey >= 160 && inSDLKey <=255 ) {
        return inSDLKey;
        }
    

    switch( inSDLKey ) {
        case SDLK_UNKNOWN: return 0;
        case SDLK_BACKSPACE: return 8;
        case SDLK_TAB: return 9;
        case SDLK_CLEAR: return 12;
        case SDLK_RETURN: return 13;
        case SDLK_PAUSE: return 19;
        case SDLK_ESCAPE: return 27;
        case SDLK_SPACE: return ' ';
        case SDLK_EXCLAIM: return '!';
        case SDLK_QUOTEDBL: return '"';
        case SDLK_HASH: return '#';
        case SDLK_DOLLAR: return '$';
        case SDLK_AMPERSAND: return '&';
        case SDLK_QUOTE: return '\'';
        case SDLK_LEFTPAREN: return '(';
        case SDLK_RIGHTPAREN: return ')';
        case SDLK_ASTERISK: return '*';
        case SDLK_PLUS: return '+';
        case SDLK_COMMA: return ',';
        case SDLK_MINUS: return '-';
        case SDLK_PERIOD: return '.';
        case SDLK_SLASH: return '/';
        case SDLK_0: return '0';
        case SDLK_1: return '1';
        case SDLK_2: return '2';
        case SDLK_3: return '3';
        case SDLK_4: return '4';
        case SDLK_5: return '5';
        case SDLK_6: return '6';
        case SDLK_7: return '7';
        case SDLK_8: return '8';
        case SDLK_9: return '9';
        case SDLK_COLON: return ':';
        case SDLK_SEMICOLON: return ';';
        case SDLK_LESS: return '<';
        case SDLK_EQUALS: return '=';
        case SDLK_GREATER: return '>';
        case SDLK_QUESTION: return '?';
        case SDLK_AT: return '@';
        case SDLK_LEFTBRACKET: return '[';
        case SDLK_BACKSLASH: return '\\';
        case SDLK_RIGHTBRACKET: return ']';
        case SDLK_CARET: return '^';
        case SDLK_UNDERSCORE: return '_';
        case SDLK_BACKQUOTE: return '`';
        case SDLK_a: return 'a';
        case SDLK_b: return 'b';
        case SDLK_c: return 'c';
        case SDLK_d: return 'd';
        case SDLK_e: return 'e';
        case SDLK_f: return 'f';
        case SDLK_g: return 'g';
        case SDLK_h: return 'h';
        case SDLK_i: return 'i';
        case SDLK_j: return 'j';
        case SDLK_k: return 'k';
        case SDLK_l: return 'l';
        case SDLK_m: return 'm';
        case SDLK_n: return 'n';
        case SDLK_o: return 'o';
        case SDLK_p: return 'p';
        case SDLK_q: return 'q';
        case SDLK_r: return 'r';
        case SDLK_s: return 's';
        case SDLK_t: return 't';
        case SDLK_u: return 'u';
        case SDLK_v: return 'v';
        case SDLK_w: return 'w';
        case SDLK_x: return 'x';
        case SDLK_y: return 'y';
        case SDLK_z: return 'z';
        case SDLK_DELETE: return 127;
        case SDLK_WORLD_0:
            
        default: return 0;
        }
    }
 
