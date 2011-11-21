/*
 * Modification History
 *
 * 2004-May-9   Jason Rohrer
 * Created.
 */



#ifndef AIFF_INCLUDED
#define AIFF_INCLUDED



/**
 * Constructs an AIFF header.
 *
 * @param inNumChannels the number of channels.
 * @param inSampleSizeInBits the size of each sound sample in bits.
 * @param inSampleRateInHertz the number of sample frames per second.
 * @param inNumSampleFrames the total number of sample frames in the file.
 * @param outHeaderLength pointer to where the length of the header, in bytes,
 *   should be returned.
 *
 * @return the header.
 *   Must be destroyed by caller.
 */
unsigned char *getAIFFHeader( int inNumChannels, int inSampleSizeInBits,
                              int inSampleRateInHertz,
                              int inNumSampleFrames, int *outHeaderLength );



#endif
