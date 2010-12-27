/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                        Intel License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2008, Nils Hasler, all rights reserved.
// Third party copyrights are property of their respective owners.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of Intel Corporation may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the Intel Corporation or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/

// Author: Bill McCord
//
//         Intuitive Automata

//
// capture video from a socket connection
//

#include "_highgui.h"
#include <android/log.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>

#define LOGV(...) __android_log_print(ANDROID_LOG_SILENT, LOG_TAG, __VA_ARGS__)
#define LOG_TAG "CVJNI"

#ifdef NDEBUG
#define CV_WARN(message)
#else
#define CV_WARN(message) fprintf(stderr, "warning: %s (%s:%d)\n", message, __FILE__, __LINE__)
#endif

#define IMAGE( i, x, y, n )   *(( unsigned char * )(( i )->imageData      \
                                    + ( x ) * sizeof( unsigned char ) * 3 \
                                    + ( y ) * ( i )->widthStep ) + ( n ))

class CVCapture_Socket : public CvCapture
{
public:
    CVCapture_Socket()
    {
		pAddrInfo = 0;
		width = 0;
		height = 0;
		readBufSize = 0;
		readBuf = 0;
        frame = 0;
    }

    virtual ~CVCapture_Socket()
    {
        close();
    }

    virtual bool open(const char* _address, const char* _port, int _width, int _height);
    virtual void close();
    virtual double getProperty(int);
    virtual bool setProperty(int, double);
    virtual bool grabFrame();
    virtual IplImage* retrieveFrame();

protected:
	struct addrinfo *pAddrInfo;
	int width; // the width of the images received over the socket
	int height; // the height of the images received over the socket
	long readBufSize; // the length of the read buffer
	char *readBuf; // the read buffer

    IplImage* frame;
};

// The open method simply initializes some variables we will need later.
bool CVCapture_Socket::open(const char* _address, const char* _port, int _width, int _height)
{	
	// Free the addrinfo if it was allocated.
	if (pAddrInfo)
	{
		freeaddrinfo(pAddrInfo);
		pAddrInfo = 0;
	}
	
	// Check the easy stuff first.
	width = _width;
	height = _height;
	if (width <= 0 || height <= 0)
	{
		LOGV("Invalid width or height!");
		return false;
	}
	
	// Setup a new addrinfo to support a streaming socket at the given address and port.
	struct addrinfo hints;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICHOST;

	int error = getaddrinfo(_address, _port, &hints, &pAddrInfo);
	if (error)
	{
		char buffer[100];
		sprintf(buffer, "getaddrinfo error: %s", gai_strerror(error));
		LOGV(buffer);
		freeaddrinfo(pAddrInfo);
		pAddrInfo = 0;
		return false;
	}
	
	readBufSize = width * height * sizeof(int);
	readBuf = (char*)malloc(readBufSize);
	if (!readBuf)
	{
		LOGV("out of memory error");
		freeaddrinfo(pAddrInfo);
		pAddrInfo = 0;
		return false;
	}
		
	return true;
}

// Close cleans up all of our state and cached data.
void CVCapture_Socket::close()
{
	LOGV("Setting simple vars to 0");
	width = 0;
	height = 0;
	readBufSize = 0;
	
	LOGV("Freeing Addr Info");
	if (pAddrInfo)
	{
		freeaddrinfo(pAddrInfo);
		pAddrInfo = 0;
	}
	
	LOGV("Freeing Buffer");
	if (readBuf)
	{
		free(readBuf);
		readBuf = 0;
	}
	
	LOGV("Releasing Image");
	if (frame)
	{
		cvReleaseImage( &frame );
		frame = 0;
	}

	LOGV("Done closing Capture Socket");
}

// Helper to load pixels from a byte stream received over a socket.
static IplImage* loadPixels(char* pixels, int width, int height) {

	int x, y, pos, int_size = sizeof(int);
	IplImage *img = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 3);

	for ( y = 0; y < height; y++ ) {
		pos = y * width * int_size;
        for ( x = 0; x < width; x++, pos += int_size ) {
            // blue
            IMAGE( img, x, y, 0 ) = pixels[pos + 3] & 0xFF;
            // green
            IMAGE( img, x, y, 1 ) = pixels[pos + 2] & 0xFF;
            // red
            IMAGE( img, x, y, 2 ) = pixels[pos + 1] & 0xFF;
        }
    }

	return img;
}

// Grabs a frame (image) from a socket.
bool CVCapture_Socket::grabFrame()
{
	// First ensure that our addrinfo and read buffer are allocated.
	if (pAddrInfo == 0 || readBuf == 0)
	{
		LOGV("You haven't opened the socket capture yet!");
		return false;
	}
	
	// Establish the socket.
	int sockd = socket(pAddrInfo->ai_family, pAddrInfo->ai_socktype, pAddrInfo->ai_protocol);
	if (sockd < 0 || errno != 0)
	{
		char buffer[100];
		sprintf(buffer, "Failed to create socket, errno = %d", errno);
		LOGV(buffer);
		::close(sockd);
		return false;
	}
	
	// Now connect to the socket.
	if (connect(sockd, pAddrInfo->ai_addr, pAddrInfo->ai_addrlen) < 0 || errno != 0)
	{
		char buffer[100];
		sprintf(buffer, "socket connection errorno = %d", errno);
		LOGV(buffer);
		::close(sockd);
		return false;
	}

	// Release the image if it hasn't been already because we are going to overwrite it.
	if (frame)
	{
		cvReleaseImage( &frame );
		frame = 0;
	}
	
	// Read the socket until we have filled the data with the space allocated OR run
	// out of data which we treat as an error.
	long read_count, total_read = 0;
	while (total_read < readBufSize)
	{
		read_count = read(sockd, &readBuf[total_read], readBufSize);
		if (read_count <= 0 || errno != 0)
		{
			char buffer[100];
			sprintf(buffer, "socket read errorno = %d", errno);
			LOGV(buffer);
			break;
		}
		total_read += read_count;
	}
	
	// If we read all of the data we expected, we will load the frame from the pixels.
	if (total_read == readBufSize)
	{
		frame = loadPixels(readBuf, width, height);
	}
	else
	{
		LOGV("full read of pixels failed");
	}
	
	// Close the socket and return the frame!
	::close(sockd);
	
    return frame != 0;
}

IplImage* CVCapture_Socket::retrieveFrame()
{
    return frame;
}

double CVCapture_Socket::getProperty(int id)
{
	LOGV("unknown/unhandled property");
    return 0;
}

bool CVCapture_Socket::setProperty(int id, double value)
{
    LOGV("unknown/unhandled property");
    return false;
}

CvCapture* cvCreateCameraCapture_Socket( const char *address, const char *port, int width, int height )
{
	CVCapture_Socket* capture = new CVCapture_Socket;
	if ( capture-> open(address, port, width, height) )
		return capture;
		
	delete capture;
	return 0;
}
