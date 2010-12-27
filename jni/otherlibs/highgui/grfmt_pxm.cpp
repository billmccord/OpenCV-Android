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
#include "utils.h"
#include "grfmt_pxm.h"

// P?M filter factory

GrFmtPxM::GrFmtPxM()
{
    m_sign_len = 3;
    m_signature = "";
    m_description = "Portable image format (*.pbm;*.pgm;*.ppm;*.pxm;*.pnm)";
}


GrFmtPxM::~GrFmtPxM()
{
}


bool GrFmtPxM::CheckSignature( const char* signature )
{
    return signature[0] == 'P' &&
           '1' <= signature[1] && signature[1] <= '6' &&
           isspace(signature[2]);
}


GrFmtReader* GrFmtPxM::NewReader( const char* filename )
{
    return new GrFmtPxMReader( filename );
}


GrFmtWriter* GrFmtPxM::NewWriter( const char* filename )
{
    return new GrFmtPxMWriter( filename );
}


///////////////////////// P?M reader //////////////////////////////

static int ReadNumber( RLByteStream& strm, int maxdigits )
{
    int code;
    int val = 0;
    int digits = 0;

    code = strm.GetByte();

    if( !isdigit(code))
    {
        do
        {
            if( code == '#' )
            {
                do
                {
                    code = strm.GetByte();
                }
                while( code != '\n' && code != '\r' );
            }
            
            code = strm.GetByte();

            while( isspace(code))
                code = strm.GetByte();
        }
        while( !isdigit( code ));
    }

    do
    {
        val = val*10 + code - '0';
        if( ++digits >= maxdigits ) break;
        code = strm.GetByte();
    }
    while( isdigit(code));

    return val;
}


GrFmtPxMReader::GrFmtPxMReader( const char* filename ) : GrFmtReader( filename )
{
    m_offset = -1;
}


GrFmtPxMReader::~GrFmtPxMReader()
{
}


void  GrFmtPxMReader::Close()
{
    m_strm.Close();
}


bool  GrFmtPxMReader::ReadHeader()
{
    bool result = false;
    
    assert( strlen(m_filename) != 0 );
    if( !m_strm.Open( m_filename )) return false;

    if( setjmp( m_strm.JmpBuf()) == 0 )
    {
        int code = m_strm.GetByte();
        if( code != 'P' )
            BAD_HEADER_ERR();

        code = m_strm.GetByte();
        switch( code )
        {
        case '1': case '4': m_bpp = 1; break;
        case '2': case '5': m_bpp = 8; break;
        case '3': case '6': m_bpp = 24; break;
        default: BAD_HEADER_ERR();
        }
        
        m_binary = code >= '4';
        m_iscolor = m_bpp > 8;

        m_width = ReadNumber( m_strm, INT_MAX );
        m_height = ReadNumber( m_strm, INT_MAX );
        
        m_maxval = m_bpp == 1 ? 1 : ReadNumber( m_strm, INT_MAX );
        if( m_maxval > 65535 )
            BAD_HEADER_ERR();

        //if( m_maxval > 255 ) m_binary = false; nonsense
        if( m_maxval > 255 )
            m_bit_depth = 16;

        if( m_width > 0 && m_height > 0 && m_maxval > 0 && m_maxval < (1 << 16)) 
        {
            m_offset = m_strm.GetPos();
            result = true;
        }
bad_header_exit:
        ;
    }

    if( !result )
    {
        m_offset = -1;
        m_width = m_height = -1;
        m_strm.Close();
    }
    return result;
}


bool  GrFmtPxMReader::ReadData( uchar* data, int step, int color )
{
    const  int buffer_size = 1 << 12;
    uchar  buffer[buffer_size];
    uchar  pal_buffer[buffer_size];
    PaletteEntry palette[256];
    bool   result = false;
    uchar* src = buffer;
    uchar* gray_palette = pal_buffer;
    int  src_pitch = (m_width*m_bpp*m_bit_depth/8 + 7)/8;
    int  nch = m_iscolor ? 3 : 1;
    int  width3 = m_width*nch;
    int  i, x, y;

    if( m_offset < 0 || !m_strm.IsOpened())
        return false;
    
    if( src_pitch+32 > buffer_size )
        src = new uchar[width3*m_bit_depth/8 + 32];

    // create LUT for converting colors
    if( m_bit_depth == 8 )
    {
        if( m_maxval + 1 > buffer_size )
            gray_palette = new uchar[m_maxval + 1];

        for( i = 0; i <= m_maxval; i++ )
        {
            gray_palette[i] = (uchar)((i*255/m_maxval)^(m_bpp == 1 ? 255 : 0));
        }

        FillGrayPalette( palette, m_bpp==1 ? 1 : 8 , m_bpp == 1 );
    }

    if( setjmp( m_strm.JmpBuf()) == 0 )
    {
        m_strm.SetPos( m_offset );
        
        switch( m_bpp )
        {
        ////////////////////////// 1 BPP /////////////////////////
        case 1:
            if( !m_binary )
            {
                for( y = 0; y < m_height; y++, data += step )
                {
                    for( x = 0; x < m_width; x++ )
                        src[x] = ReadNumber( m_strm, 1 ) != 0;

                    if( color )
                        FillColorRow8( data, src, m_width, palette );
                    else
                        FillGrayRow8( data, src, m_width, gray_palette );
                }
            }
            else
            {
                for( y = 0; y < m_height; y++, data += step )
                {
                    m_strm.GetBytes( src, src_pitch );
                    
                    if( color )
                        FillColorRow1( data, src, m_width, palette );
                    else
                        FillGrayRow1( data, src, m_width, gray_palette );
                }
            }
            result = true;
            break;

        ////////////////////////// 8 BPP /////////////////////////
        case 8:
        case 24:
            for( y = 0; y < m_height; y++, data += step )
            {
                if( !m_binary )
                {
                    for( x = 0; x < width3; x++ )
                    {
                        int code = ReadNumber( m_strm, INT_MAX );
                        if( (unsigned)code > (unsigned)m_maxval ) code = m_maxval;
                        if( m_bit_depth == 8 )
                            src[x] = gray_palette[code];
                        else
                            ((ushort *)src)[x] = (ushort)code;
                    }
                }
                else
                {
                    m_strm.GetBytes( src, src_pitch );
                    if( m_bit_depth == 16 && !isBigEndian() )
                    {
                        for( x = 0; x < width3; x++ )
                        {
                            uchar v = src[x * 2];
                            src[x * 2] = src[x * 2 + 1];
                            src[x * 2 + 1] = v;
                        }
                    }
                }

                if( !m_native_depth && m_bit_depth == 16 )
                {
                    for( x = 0; x < width3; x++ )
                    {
                        int v = ((ushort *)src)[x];
                        src[x] = (uchar)(v >> 8);
                    }
                }

                if( m_bpp == 8 ) // image has one channel
                {
                    if( color )
                    {
                        if( m_bit_depth == 8 || !m_native_depth ) {
                            uchar *d = data, *s = src, *end = src + m_width;
                            for( ; s < end; d += 3, s++)
                                d[0] = d[1] = d[2] = *s;
                        } else {
                            ushort *d = (ushort *)data, *s = (ushort *)src, *end = ((ushort *)src) + m_width;
                            for( ; s < end; s++, d += 3)
                                d[0] = d[1] = d[2] = *s;
                        }
                    }
                    else if( m_native_depth )
                        memcpy( data, src, m_width*m_bit_depth/8 );
                    else
                        memcpy( data, src, m_width );
                }
                else
                {
                    if( color )
                    {
                        if( m_bit_depth == 8 || !m_native_depth )
                            icvCvt_RGB2BGR_8u_C3R( src, 0, data, 0, cvSize(m_width,1) );
                        else
                            icvCvt_RGB2BGR_16u_C3R( (ushort *)src, 0, (ushort *)data, 0, cvSize(m_width,1) );
                    }
                    else if( m_bit_depth == 8 || !m_native_depth )
                        icvCvt_BGR2Gray_8u_C3C1R( src, 0, data, 0, cvSize(m_width,1), 2 );
                    else
                        icvCvt_BGR2Gray_16u_C3C1R( (ushort *)src, 0, (ushort *)data, 0, cvSize(m_width,1), 2 );
                }
            }
            result = true;
            break;
        default:
            assert(0);
        }
    }

    if( src != buffer )
        delete[] src; 

    if( gray_palette != pal_buffer )
        delete[] gray_palette;

    return result;
}


//////////////////////////////////////////////////////////////////////////////////////////

GrFmtPxMWriter::GrFmtPxMWriter( const char* filename ) : GrFmtWriter( filename )
{
}


GrFmtPxMWriter::~GrFmtPxMWriter()
{
}


bool  GrFmtPxMWriter::IsFormatSupported( int depth )
{
    return depth == IPL_DEPTH_8U || depth == IPL_DEPTH_16U;
}


bool  GrFmtPxMWriter::WriteImage( const uchar* data, int step,
                                  int width, int height, int depth, int _channels )
{
    bool isBinary = true;
    bool result = false;

    int  channels = _channels > 1 ? 3 : 1;
    int  fileStep = width*channels*(depth/8);
    int  x, y;

    assert( data && width > 0 && height > 0 && step >= fileStep );
    
    if( m_strm.Open( m_filename ) )
    {
        int  lineLength;
        int  bufferSize = 128; // buffer that should fit a header
        char* buffer = 0;

        if( isBinary )
            lineLength = channels * width * depth / 8;
        else
            lineLength = (6 * channels + (channels > 1 ? 2 : 0)) * width + 32;

        if( bufferSize < lineLength )
            bufferSize = lineLength;

        buffer = new char[bufferSize];
        if( !buffer )
        {
            m_strm.Close();
            return false;
        }

        // write header;
        sprintf( buffer, "P%c\n%d %d\n%d\n",
                 '2' + (channels > 1 ? 1 : 0) + (isBinary ? 3 : 0),
                 width, height, (1 << depth) - 1 );

        m_strm.PutBytes( buffer, (int)strlen(buffer) );

        for( y = 0; y < height; y++, data += step )
        {
            if( isBinary )
            {
                if( _channels == 3 )
                {
                    if( depth == 8 )
                        icvCvt_BGR2RGB_8u_C3R( (uchar*)data, 0,
                            (uchar*)buffer, 0, cvSize(width,1) );
                    else
                        icvCvt_BGR2RGB_16u_C3R( (ushort*)data, 0,
                            (ushort*)buffer, 0, cvSize(width,1) );
                }

                // swap endianness if necessary
                if( depth == 16 && !isBigEndian() )
                {
                    if( _channels == 1 )
                        memcpy( buffer, data, fileStep );
                    for( x = 0; x < width*channels*2; x += 2 )
                    {
                        uchar v = buffer[x];
                        buffer[x] = buffer[x + 1];
                        buffer[x + 1] = v;
                    }
                }
                m_strm.PutBytes( (channels > 1 || depth > 8) ? buffer : (char*)data, fileStep );
            }
            else
            {
                char* ptr = buffer;

                if( channels > 1 )
                {
                    if( depth == 8 )
                    {
                        for( x = 0; x < width*channels; x += channels )
                        {
                            sprintf( ptr, "% 4d", data[x + 2] );
                            ptr += 4;
                            sprintf( ptr, "% 4d", data[x + 1] );
                            ptr += 4;
                            sprintf( ptr, "% 4d", data[x] );
                            ptr += 4;
                            *ptr++ = ' ';
                            *ptr++ = ' ';
                        }
                    }
                    else
                    {
                        for( x = 0; x < width*channels; x += channels )
                        {
                            sprintf( ptr, "% 6d", ((ushort *)data)[x + 2] );
                            ptr += 6;
                            sprintf( ptr, "% 6d", ((ushort *)data)[x + 1] );
                            ptr += 6;
                            sprintf( ptr, "% 6d", ((ushort *)data)[x] );
                            ptr += 6;
                            *ptr++ = ' ';
                            *ptr++ = ' ';
                        }
                    }
                }
                else
                {
                    if( depth == 8 )
                    {
                        for( x = 0; x < width; x++ )
                        {
                            sprintf( ptr, "% 4d", data[x] );
                            ptr += 4;
                        }
                    }
                    else
                    {
                        for( x = 0; x < width; x++ )
                        {
                            sprintf( ptr, "% 6d", ((ushort *)data)[x] );
                            ptr += 6;
                        }
                    }
                }

                *ptr++ = '\n';

                m_strm.PutBytes( buffer, (int)(ptr - buffer) );
            }
        }
        delete[] buffer;
        m_strm.Close();
        result = true;
    }
    
    return result;
}

