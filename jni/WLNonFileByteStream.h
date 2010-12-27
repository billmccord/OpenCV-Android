/*
OpenCV for Android NDK
Copyright (c) 2006-2009 SIProp Project http://www.siprop.org/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it freely,
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/
#ifndef _WLNonFileByteStream_H_
#define _WLNonFileByteStream_H_

#include <stdio.h>
#include "cv.h"
#include "cxcore.h"
#include "cvaux.h"
#include "highgui.h"
#include "ml.h"
#include "utils.h"

class WLNonFileByteStream {
public:
    WLNonFileByteStream();
    ~WLNonFileByteStream();

    bool    Open(int data_size);
    void    Close();
    void    PutByte( int val );
    void    PutBytes( const void* buffer, int count );
    void    PutWord( int val );
    void    PutDWord( int val ); 
    uchar*  GetByte(); 
    int     GetSize(); 

protected:
    void    Allocate(int data_size);
	void    Deallocate();
    int     _size;
    uchar*  m_start;
    uchar*  m_end;
    uchar*  m_current;
    bool    m_is_opened;

};

#endif/*_WLNonFileByteStream_H_*/
