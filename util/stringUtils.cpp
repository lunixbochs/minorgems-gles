/*
 * Modification History
 *
 * 2003-May-10   Jason Rohrer
 * Created.
 * Added a tokenization function.
 *
 * 2003-June-14   Jason Rohrer
 * Added a join function.
 *
 * 2003-June-22   Jason Rohrer
 * Added an autoSprintf function.
 *
 * 2003-July-27  Jason Rohrer
 * Fixed bugs in autoSprintf return values for certain cases.
 *
 * 2003-August-12  Jason Rohrer
 * Added a concatonate function.
 *
 * 2003-September-7  Jason Rohrer
 * Changed so that split returns last part, even if it is empty.
 *
 * 2004-January-15  Jason Rohrer
 * Added work-around for MinGW vsnprintf bug.
 *
 * 2006-June-2  Jason Rohrer
 * Added a stringStartsWith function.
 *
 * 2009-September-7  Jason Rohrer
 * Fixed int types.
 * Switched away from StringBufferOutputStream to new function in SimpleVector.
 *
 * 2010-May-14    Jason Rohrer
 * String parameters as const to fix warnings.
 */



#include "stringUtils.h"

#include <stdarg.h>



char *stringToLowerCase( const char *inString  ) {

    unsigned int length = strlen( inString );

    char *returnString = stringDuplicate( inString );
    
    for( unsigned int i=0; i<length; i++ ) {
        returnString[i] = (char)tolower( returnString[i] );
        }

    return returnString;
    }



char *stringToUpperCase( const char *inString  ) {

    unsigned int length = strlen( inString );

    char *returnString = stringDuplicate( inString );
    
    for( unsigned int i=0; i<length; i++ ) {
        returnString[i] = (char)toupper( returnString[i] );
        }

    return returnString;
    }



char *stringLocateIgnoreCase( const char *inHaystack,
                              const char *inNeedle ) {

    char *lowerHaystack = stringToLowerCase( inHaystack );
    char *lowerNeedle = stringToLowerCase( inNeedle );

    char *matchPointer = strstr( lowerHaystack, lowerNeedle );

    char *returnString = NULL;
    
    if( matchPointer != NULL ) {

        unsigned int matchRemainderLength = strlen( matchPointer );

        unsigned int haystackIndex = 
            strlen( inHaystack ) -  matchRemainderLength;
    
        returnString =  (char*)( &( inHaystack[ haystackIndex ] ) );
        }

    delete [] lowerHaystack;
    delete [] lowerNeedle;

    return returnString;
    }



int stringCompareIgnoreCase( const char *inStringA,
                             const char *inStringB ) {

    char *lowerA = stringToLowerCase( inStringA );
    char *lowerB = stringToLowerCase( inStringB );

    int returnVal = strcmp( lowerA, lowerB );

    delete [] lowerB;
    delete [] lowerA;

    return returnVal;
    }



char stringStartsWith( const char *inString, const char *inPrefix ) {
    unsigned int prefixLength = strlen( inPrefix );
    unsigned int stringLength = strlen( inString );
    
    if( prefixLength > stringLength ) {
        return false;
        }
    else {
        for( unsigned int i=0; i<prefixLength; i++ ) {
            if( inString[i] != inPrefix[i] ) {
                return false;
                }
            }

        // all characters, up to prefix length, are equal in both strings
        return true;
        }
    }



char **split( const char *inString, const char *inSeparator, 
              int *outNumParts ) {
    SimpleVector<char *> *parts = new SimpleVector<char *>();
    
    char *workingString = stringDuplicate( inString );
    char *workingStart = workingString;

    unsigned int separatorLength = strlen( inSeparator );

    char *foundSeparator = strstr( workingString, inSeparator );

    while( foundSeparator != NULL ) {
        // terminate at separator        
        foundSeparator[0] = '\0';
        parts->push_back( stringDuplicate( workingString ) );

        // skip separator
        workingString = &( foundSeparator[ separatorLength ] );
        foundSeparator = strstr( workingString, inSeparator );
        }

    // add the remaining part, even if it is the empty string
    parts->push_back( stringDuplicate( workingString ) );

                      
    delete [] workingStart;

    *outNumParts = parts->size();
    char **returnArray = parts->getElementArray();
    
    delete parts;

    return returnArray;
    }



char *join( char **inStrings, int inNumParts, const char *inGlue ) {
    SimpleVector<char> result;

    for( int i=0; i<inNumParts - 1; i++ ) {
        result.appendElementString( inStrings[i] );
        result.appendElementString( inGlue );
        }
    // no glue after last string
    result.appendElementString( inStrings[ inNumParts - 1 ] );

    char *returnString = result.getElementString();

    return returnString;
    }



char *concatonate( const char *inStringA, const char *inStringB ) {
    char **tempArray = new char*[2];
    tempArray[ 0 ] = (char *)inStringA;
    tempArray[ 1 ] = (char *)inStringB;

    char *result = join( tempArray, 2, "" );

    delete [] tempArray;

    return result;
    }
    


char *replaceOnce( const char *inHaystack, const char *inTarget,
                   const char *inSubstitute,
                   char *outFound ) {
    
    char *haystackCopy = stringDuplicate( inHaystack );
    
	char *fieldTargetPointer = strstr( haystackCopy, inTarget );


    if( fieldTargetPointer == NULL ) {
        // target not found
        *outFound = false;
        return haystackCopy;
        }
    else {
        // target found

		// prematurely terminate haystack copy string at
        // start of target occurence
        // (okay, since we're working with a copy of the haystack argument)
		fieldTargetPointer[0] = '\0';

		// pointer to first char after target occurrence
		char *fieldPostTargetPointer =
            &( fieldTargetPointer[ strlen( inTarget ) ] );

        char *returnString = new char[
            strlen( inHaystack )
            - strlen( inTarget )
            + strlen( inSubstitute ) + 1 ];
        
		sprintf( returnString, "%s%s%s",
				 haystackCopy,
				 inSubstitute,
				 fieldPostTargetPointer );

		delete [] haystackCopy;

        *outFound = true;
        return returnString;
		}
    
    }



char *replaceAll( const char *inHaystack, const char *inTarget,
                  const char *inSubstitute,
                  char *outFound ) {

    // repeatedly replace once until replacing fails
    
    char lastFound = true;
    char atLeastOneFound = false;
    char *returnString = stringDuplicate( inHaystack );

    while( lastFound ) {

        char *nextReturnString =
            replaceOnce( returnString, inTarget, inSubstitute, &lastFound );

        delete [] returnString;
        
        returnString = nextReturnString;

        if( lastFound ) {
            atLeastOneFound = true;
            }
        }

    *outFound = atLeastOneFound;
    
    return returnString;    
    }



char *replaceTargetListWithSubstituteList(
    const char *inHaystack,
    SimpleVector<char *> *inTargetVector,
    SimpleVector<char *> *inSubstituteVector ) {

    int numTargets = inTargetVector->size();

    char *newHaystack = stringDuplicate( inHaystack );

    char tagFound;
    
    for( int i=0; i<numTargets; i++ ) {
        char *newHaystackWithReplacements
            = replaceAll( newHaystack,
                          *( inTargetVector->getElement( i ) ),
                          *( inSubstituteVector->getElement( i ) ),
                          &tagFound );
        delete [] newHaystack;

        newHaystack = newHaystackWithReplacements;
        }

    return newHaystack;
    }



SimpleVector<char *> *tokenizeString( const char *inString ) {

    char *tempString = stringDuplicate( inString );

    char *restOfString = tempString;
    
    SimpleVector<char *> *foundTokens = new SimpleVector<char *>();

    SimpleVector<char> *currentToken = new SimpleVector<char>();


    while( restOfString[0] != '\0' ) {
        // characters remain

        // skip whitespace
        char nextChar = restOfString[0];
        while( nextChar == ' ' || nextChar == '\n' ||
               nextChar == '\r' || nextChar == '\t' ) {

            restOfString = &( restOfString[1] );
            nextChar = restOfString[0];
            }

        if( restOfString[0] != '\0' ) {

            // a token

            while( nextChar != ' ' && nextChar != '\n' &&
                   nextChar != '\r' && nextChar != '\t' &&
                   nextChar != '\0'  ) {

                // still not whitespace
                currentToken->push_back( nextChar );
                
                restOfString = &( restOfString[1] );
                nextChar = restOfString[0];
                }

            // reached end of token
            foundTokens->push_back( currentToken->getElementString() );
            currentToken->deleteAll();
            }        
        }

    delete [] tempString;

    delete currentToken;

    return foundTokens;
    }



char *autoSprintf( const char* inFormatString, ... ) {

    unsigned int bufferSize = 50;

    va_list argList;
    va_start( argList, inFormatString );

    char *buffer = new char[ bufferSize ];
    
    int stringLength =
        vsnprintf( buffer, bufferSize, inFormatString, argList );
    
    va_end( argList );


    if( stringLength != -1 ) {
        // follows C99 standard...
        // stringLength is the length of the string that would have been
        // written if the buffer was big enough

        //  not room for string and terminating \0 in bufferSize bytes
        if( (unsigned int)stringLength >= bufferSize ) {

            // need to reprint with a bigger buffer
            delete [] buffer;

            bufferSize = (unsigned int)( stringLength + 1 );

            va_list argList;
            va_start( argList, inFormatString );

            buffer = new char[ bufferSize ];

            // can simply use vsprintf now
            vsprintf( buffer, inFormatString, argList );
    
            va_end( argList );

            return buffer;
            }
        else {
            // buffer was big enough

            // trim the buffer to fit the string
            char *returnString = stringDuplicate( buffer );
            delete [] buffer;
            
            return returnString;
            }
        }
    else {
        // follows old ANSI standard
        // -1 means the buffer was too small

        // Note that some buggy non-C99 vsnprintf implementations
        // (notably MinGW)
        // do not return -1 if stringLength equals bufferSize (in other words,
        // if there is not enough room for the trailing \0).

        // Thus, we need to check for both
        //   (A)  stringLength == -1
        //   (B)  stringLength == bufferSize
        // below.
        
        // keep doubling buffer size until it's big enough
        while( stringLength == -1 || 
               (unsigned int)stringLength == bufferSize ) {

            delete [] buffer;

            if( (unsigned int)stringLength == bufferSize ) {
                // only occurs if vsnprintf implementation is buggy

                // might as well use the information, though
                // (instead of doubling the buffer size again)
                bufferSize = bufferSize + 1;
                }
            else {
                // double buffer size again
                bufferSize = 2 * bufferSize;
                }

            va_list argList;
            va_start( argList, inFormatString );

            buffer = new char[ bufferSize ];
    
            stringLength =
                vsnprintf( buffer, bufferSize, inFormatString, argList );
            
            va_end( argList );            
            }

        // trim the buffer to fit the string
        char *returnString = stringDuplicate( buffer );
        delete [] buffer;

        return returnString;
        }
    }

