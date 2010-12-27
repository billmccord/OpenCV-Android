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
// Copyright (C) 2000, Intel Corporation, all rights reserved.
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

#include "_highgui.h"

#include "grfmt_base.h"
#include "bitstrm.h"


GrFmtReader::GrFmtReader( const char* filename )
{
    strncpy( m_filename, filename, sizeof(m_filename) - 1 );
    m_filename[sizeof(m_filename)-1] = '\0';
    m_width = m_height = 0;
    m_iscolor = false;
    m_bit_depth = 8;
    m_native_depth = false;
    m_isfloat = false;
}


GrFmtReader::~GrFmtReader()
{
    Close();
}


void GrFmtReader::Close()
{
    m_width = m_height = 0;
    m_iscolor = false;
}


GrFmtWriter::GrFmtWriter( const char* filename )
{
    strncpy( m_filename, filename, sizeof(m_filename) - 1 );
    m_filename[sizeof(m_filename)-1] = '\0';
}


bool  GrFmtWriter::IsFormatSupported( int depth )
{
    return depth == IPL_DEPTH_8U;
}


GrFmtFilterFactory::GrFmtFilterFactory()
{
    m_description = m_signature = 0;
    m_sign_len = 0;
}


bool  GrFmtFilterFactory::CheckFile( const char* filename )
{
	FILE* f = 0;
	char signature[4096];
	int sign_len = 0;
	int cur_sign_len = GetSignatureLength();
	
	if( !filename ) return false;
	
	assert( cur_sign_len <= (int)sizeof( signature ) );
	f = fopen( filename, "rb" );
	
	if( f )
	{
		sign_len = (int)fread( signature, 1, cur_sign_len, f );
		fclose( f );
		
		if( cur_sign_len <= sign_len && CheckSignature( signature ) )
            return true;
	}
	
	return false;
}


bool GrFmtFilterFactory::CheckSignature( const char* signature )
{
    return m_sign_len > 0 && signature != 0 &&
           memcmp( signature, m_signature, m_sign_len ) == 0;
}


static int GetExtensionLength( const char* buffer )
{
    int len = 0;

    if( buffer )
    {
        const char* ext = strchr( buffer, '.');
        if( ext++ )
            while( isalnum(ext[len]) && len < _MAX_PATH )
                len++;
    }

    return len;
}


bool GrFmtFilterFactory::CheckExtension( const char* format )
{
    const char* descr = 0;
    int len = 0;
        
    if( !format || !m_description )
        return false;

    // find the right-most extension of the passed format string
    for(;;)
    {
        const char* ext = strchr( format + 1, '.' );
        if( !ext ) break;
        format = ext;
    }

    len = GetExtensionLength( format );

    if( format[0] != '.' || len == 0 )
        return false;

    descr = strchr( m_description, '(' );

    while( descr )
    {
        descr = strchr( descr + 1, '.' );
        int i, len2 = GetExtensionLength( descr ); 

        if( len2 == 0 )
            break;

        if( len2 == len )
        {
            for( i = 0; i < len; i++ )
            {
                int c1 = tolower(format[i+1]);
                int c2 = tolower(descr[i+1]);

                if( c1 != c2 )
                    break;
            }
            if( i == len )
                return true;
        }
    }

    return false;
}



///////////////////// GrFmtFilterList //////////////////////////

GrFmtFactoriesList::GrFmtFactoriesList()
{
    m_factories = 0;
    RemoveAll();
}


GrFmtFactoriesList::~GrFmtFactoriesList()
{
    RemoveAll();
}


void  GrFmtFactoriesList::RemoveAll()
{
    if( m_factories )
    {
        for( int i = 0; i < m_curFactories; i++ ) delete m_factories[i];
        delete[] m_factories;
    }
    m_factories = 0;
    m_maxFactories = m_curFactories = 0;
}


bool  GrFmtFactoriesList::AddFactory( GrFmtFilterFactory* factory )
{
    assert( factory != 0 );
    if( m_curFactories == m_maxFactories )
    {
        // reallocate the factorys pointers storage
        int newMaxFactories = 2*m_maxFactories;
        if( newMaxFactories < 16 ) newMaxFactories = 16;

        GrFmtFilterFactory** newFactories = new GrFmtFilterFactory*[newMaxFactories];

        for( int i = 0; i < m_curFactories; i++ ) newFactories[i] = m_factories[i];

        delete[] m_factories;
        m_factories = newFactories;
        m_maxFactories = newMaxFactories;
    }

    m_factories[m_curFactories++] = factory;
    return true;
}


ListPosition  GrFmtFactoriesList::GetFirstFactoryPos()
{
    return (ListPosition)m_factories;
}


GrFmtFilterFactory* GrFmtFactoriesList::GetNextFactory( ListPosition& pos )
{
    GrFmtFilterFactory* factory = 0;
    GrFmtFilterFactory** temp = (GrFmtFilterFactory**)pos;

    assert( temp == 0 || (m_factories <= temp && temp < m_factories + m_curFactories));
    if( temp )
    {
        factory = *temp++;
        pos = (ListPosition)(temp < m_factories + m_curFactories ? temp : 0);
    }
    return factory;
}


GrFmtReader* GrFmtFactoriesList::FindReader( const char* filename )
{
	if( !filename ) return 0;
	
	GrFmtReader* reader = 0;
	ListPosition pos = GetFirstFactoryPos();
	
	while( pos )
	{
		GrFmtFilterFactory* tempFactory = GetNextFactory( pos );
		if( tempFactory->CheckFile( filename ) )
		{
			reader = tempFactory->NewReader( filename );
			break;
		}
	}
	
    return reader;
}


GrFmtWriter* GrFmtFactoriesList::FindWriter( const char* filename )
{
    GrFmtWriter* writer = 0;
    ListPosition pos = GetFirstFactoryPos();

    if( !filename ) return 0;

    while( pos )
    {
        GrFmtFilterFactory* tempFactory = GetNextFactory(pos);
        if( tempFactory->CheckExtension( filename ))
        {
            writer = tempFactory->NewWriter( filename );
            break;
        }
    }

    return writer;
}

/* End of file. */

