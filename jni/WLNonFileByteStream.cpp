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
#include "WLNonFileByteStream.h"

///////////////////////////// WLNonFileByteStream /////////////////////////////////// 

WLNonFileByteStream::WLNonFileByteStream()
{
    m_start = m_end = m_current = 0;
    _size = 0;
    m_is_opened = false;
}


WLNonFileByteStream::~WLNonFileByteStream()
{
	Deallocate();
}


void  WLNonFileByteStream::Allocate(int data_size)
{
	if(!m_start)
    	m_start = new uchar[data_size];

    m_end = m_start + data_size;
    m_current = m_start;
}

void  WLNonFileByteStream::Deallocate()
{
	if(m_start)
	{
		delete [] m_start;
		m_start = 0;
	}
}

bool  WLNonFileByteStream::Open(int data_size)
{
    Close();
    Allocate(data_size);
    
    m_is_opened = true;
    m_current = m_start;
    _size = data_size;
    
    return true;
}


void  WLNonFileByteStream::Close()
{
    m_is_opened = false;
	Deallocate();
}


void WLNonFileByteStream::PutByte( int val )
{
    *m_current++ = (uchar)val;
}


void WLNonFileByteStream::PutBytes( const void* buffer, int count )
{
    uchar* data = (uchar*)buffer;
    
    assert( data && m_current && count >= 0 );

    while( count )
    {
        int l = (int)(m_end - m_current);
        
        if( l > count )
            l = count;
        
        if( l > 0 )
        {
            memcpy( m_current, data, l );
            m_current += l;
            data += l;
            count -= l;
        }
    }
}


void WLNonFileByteStream::PutWord( int val )
{
    uchar *current = m_current;

    if( current+1 < m_end )
    {
        current[0] = (uchar)val;
        current[1] = (uchar)(val >> 8);
        m_current = current + 2;
    }
    else
    {
        PutByte(val);
        PutByte(val >> 8);
    }
}


void WLNonFileByteStream::PutDWord( int val )
{
    uchar *current = m_current;

    if( current+3 < m_end )
    {
        current[0] = (uchar)val;
        current[1] = (uchar)(val >> 8);
        current[2] = (uchar)(val >> 16);
        current[3] = (uchar)(val >> 24);
        m_current = current + 4;
    }
    else
    {
        PutByte(val);
        PutByte(val >> 8);
        PutByte(val >> 16);
        PutByte(val >> 24);
    }
}


uchar* WLNonFileByteStream::GetByte()
{
	return m_start;
}

int WLNonFileByteStream::GetSize()
{
	return _size;
}

