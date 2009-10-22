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
#include "grfmt_sunras.h"

static const char* fmtSignSunRas = "\x59\xA6\x6A\x95";

// Sun Raster filter factory

GrFmtSunRaster::GrFmtSunRaster()
{
    m_sign_len = 4;
    m_signature = fmtSignSunRas;
    m_description = "Sun raster files (*.sr;*.ras)";
}


GrFmtSunRaster::~GrFmtSunRaster()
{
}


GrFmtReader* GrFmtSunRaster::NewReader( const char* filename )
{
    return new GrFmtSunRasterReader( filename );
}


GrFmtWriter* GrFmtSunRaster::NewWriter( const char* filename )
{
    return new GrFmtSunRasterWriter( filename );
}


/************************ Sun Raster reader *****************************/

GrFmtSunRasterReader::GrFmtSunRasterReader( const char* filename ) : GrFmtReader( filename )
{
    m_offset = -1;
}


GrFmtSunRasterReader::~GrFmtSunRasterReader()
{
}


void  GrFmtSunRasterReader::Close()
{
    m_strm.Close();
}


bool  GrFmtSunRasterReader::ReadHeader()
{
    bool result = false;
    
    assert( strlen(m_filename) != 0 );
    if( !m_strm.Open( m_filename )) return false;

    if( setjmp( m_strm.JmpBuf()) == 0 )
    {
        m_strm.Skip( 4 );
        m_width  = m_strm.GetDWord();
        m_height = m_strm.GetDWord();
        m_bpp    = m_strm.GetDWord();
        int palSize = 3*(1 << m_bpp);

        m_strm.Skip( 4 );
        m_type   = (SunRasType)m_strm.GetDWord();
        m_maptype = (SunRasMapType)m_strm.GetDWord();
        m_maplength = m_strm.GetDWord();

        if( m_width > 0 && m_height > 0 &&
            (m_bpp == 1 || m_bpp == 8 || m_bpp == 24 || m_bpp == 32) &&
            (m_type == RAS_OLD || m_type == RAS_STANDARD ||
             (m_type == RAS_BYTE_ENCODED && m_bpp == 8) || m_type == RAS_FORMAT_RGB) &&
            (m_maptype == RMT_NONE && m_maplength == 0 ||
             m_maptype == RMT_EQUAL_RGB && m_maplength <= palSize && m_bpp <= 8))
        {
            memset( m_palette, 0, sizeof(m_palette));
            
            if( m_maplength != 0 )
            {
                int readed;
                uchar buffer[256*3];

                m_strm.GetBytes( buffer, m_maplength, &readed );
                if( readed == m_maplength )
                {
                    int i;
                    palSize = m_maplength/3;

                    for( i = 0; i < palSize; i++ )
                    {
                        m_palette[i].b = buffer[i + 2*palSize];
                        m_palette[i].g = buffer[i + palSize];
                        m_palette[i].r = buffer[i];
                        m_palette[i].a = 0;
                    }

                    m_iscolor = IsColorPalette( m_palette, m_bpp );
                    m_offset = m_strm.GetPos();

                    assert( m_offset == 32 + m_maplength );
                    result = true;
                }
            }
            else
            {
                m_iscolor = m_bpp > 8;
                
                if( !m_iscolor )
                    FillGrayPalette( m_palette, m_bpp );

                m_offset = m_strm.GetPos();

                assert( m_offset == 32 + m_maplength );
                result = true;
            }
        }
    }

    if( !result )
    {
        m_offset = -1;
        m_width = m_height = -1;
        m_strm.Close();
    }
    return result;
}


bool  GrFmtSunRasterReader::ReadData( uchar* data, int step, int color )
{
    const  int buffer_size = 1 << 12;
    uchar  buffer[buffer_size];
    uchar  bgr_buffer[buffer_size];
    uchar  gray_palette[256];
    bool   result = false;
    uchar* src = buffer;
    uchar* bgr = bgr_buffer;
    int  src_pitch = ((m_width*m_bpp + 7)/8 + 1) & -2;
    int  nch = color ? 3 : 1;
    int  width3 = m_width*nch;
    int  y;

    if( m_offset < 0 || !m_strm.IsOpened())
        return false;
    
    if( src_pitch+32 > buffer_size )
        src = new uchar[src_pitch+32];

    if( m_width*3 + 32 > buffer_size )
        bgr = new uchar[m_width*3 + 32];

    if( !color && m_maptype == RMT_EQUAL_RGB )
        CvtPaletteToGray( m_palette, gray_palette, 1 << m_bpp );

    if( setjmp( m_strm.JmpBuf()) == 0 )
    {
        m_strm.SetPos( m_offset );
        
        switch( m_bpp )
        {
        /************************* 1 BPP ************************/
        case 1:
            if( m_type != RAS_BYTE_ENCODED )
            {
                for( y = 0; y < m_height; y++, data += step )
                {
                    m_strm.GetBytes( src, src_pitch );
                    if( color )
                        FillColorRow1( data, src, m_width, m_palette );
                    else
                        FillGrayRow1( data, src, m_width, gray_palette );
                }
                result = true;
            }
            else
            {
                uchar* line_end = src + (m_width*m_bpp + 7)/8;
                uchar* tsrc = src;
                y = 0;
               
                for(;;)
                {
                    int max_count = (int)(line_end - tsrc);
                    int code = 0, len = 0, len1 = 0;

                    do
                    {
                        code = m_strm.GetByte();
                        if( code == 0x80 )
                        {
                            len = m_strm.GetByte();
                            if( len != 0 ) break;
                        }
                        tsrc[len1] = (uchar)code;
                    }
                    while( ++len1 < max_count );

                    tsrc += len1;

                    if( len > 0 ) // encoded mode
                    {
                        ++len;
                        code = m_strm.GetByte();
                        if( len > line_end - tsrc )
                        {
                            assert(0);
                            goto bad_decoding_1bpp;
                        }

                        memset( tsrc, code, len );
                        tsrc += len;
                    }

                    if( tsrc >= line_end )
                    {
                        tsrc = src;
                        if( color )
                            FillColorRow1( data, src, m_width, m_palette );
                        else
                            FillGrayRow1( data, src, m_width, gray_palette );
                        data += step;
                        if( ++y >= m_height ) break;
                    }
                }
                result = true;
bad_decoding_1bpp:
                ;
            }
            break;
        /************************* 8 BPP ************************/
        case 8:
            if( m_type != RAS_BYTE_ENCODED )
            {
                for( y = 0; y < m_height; y++, data += step )
                {
                    m_strm.GetBytes( src, src_pitch );
                    if( color )
                        FillColorRow8( data, src, m_width, m_palette );
                    else
                        FillGrayRow8( data, src, m_width, gray_palette );
                }
                result = true;
            }
            else // RLE-encoded
            {
                uchar* line_end = data + width3;
                y = 0;

                for(;;)
                {
                    int max_count = (int)(line_end - data);
                    int code = 0, len = 0, len1;
                    uchar* tsrc = src;

                    do
                    {
                        code = m_strm.GetByte();
                        if( code == 0x80 )
                        {
                            len = m_strm.GetByte();
                            if( len != 0 ) break;
                        }
                        *tsrc++ = (uchar)code;
                    }
                    while( (max_count -= nch) > 0 );

                    len1 = (int)(tsrc - src);

                    if( len1 > 0 )
                    {
                        if( color )
                            FillColorRow8( data, src, len1, m_palette );
                        else
                            FillGrayRow8( data, src, len1, gray_palette );
                        data += len1*nch;
                    }

                    if( len > 0 ) // encoded mode
                    {
                        len = (len + 1)*nch;
                        code = m_strm.GetByte();

                        if( color )
                            data = FillUniColor( data, line_end, step, width3, 
                                                 y, m_height, len,
                                                 m_palette[code] );
                        else
                            data = FillUniGray( data, line_end, step, width3, 
                                                y, m_height, len,
                                                gray_palette[code] );
                        if( y >= m_height )
                            break;
                    }

                    if( data == line_end )
                    {
                        if( m_strm.GetByte() != 0 )
                            goto bad_decoding_end;
                        line_end += step;
                        data = line_end - width3;
                        if( ++y >= m_height ) break;
                    }
                }

                result = true;
bad_decoding_end:
                ;
            }
            break;
        /************************* 24 BPP ************************/
        case 24:
            for( y = 0; y < m_height; y++, data += step )
            {
                m_strm.GetBytes( color ? data : bgr, src_pitch );

                if( color )
                {
                    if( m_type == RAS_FORMAT_RGB )
                        icvCvt_RGB2BGR_8u_C3R( data, 0, data, 0, cvSize(m_width,1) );
                }
                else
                {
                    icvCvt_BGR2Gray_8u_C3C1R( bgr, 0, data, 0, cvSize(m_width,1),
                                              m_type == RAS_FORMAT_RGB ? 2 : 0 );
                }
            }
            result = true;
            break;
        /************************* 32 BPP ************************/
        case 32:
            for( y = 0; y < m_height; y++, data += step )
            {
                /* hack: a0 b0 g0 r0 a1 b1 g1 r1 ... are written to src + 3,
                   so when we look at src + 4, we see b0 g0 r0 x b1 g1 g1 x ... */
                m_strm.GetBytes( src + 3, src_pitch );
                
                if( color )
                    icvCvt_BGRA2BGR_8u_C4C3R( src + 4, 0, data, 0, cvSize(m_width,1),
                                              m_type == RAS_FORMAT_RGB ? 2 : 0 );
                else
                    icvCvt_BGRA2Gray_8u_C4C1R( src + 4, 0, data, 0, cvSize(m_width,1),
                                               m_type == RAS_FORMAT_RGB ? 2 : 0 );
            }
            result = true;
            break;
        default:
            assert(0);
        }
    }

    if( src != buffer ) delete[] src; 
    if( bgr != bgr_buffer ) delete[] bgr;

    return result;
}


//////////////////////////////////////////////////////////////////////////////////////////

GrFmtSunRasterWriter::GrFmtSunRasterWriter( const char* filename ) : GrFmtWriter( filename )
{
}


GrFmtSunRasterWriter::~GrFmtSunRasterWriter()
{
}


bool  GrFmtSunRasterWriter::WriteImage( const uchar* data, int step,
                                        int width, int height, int /*depth*/, int channels )
{
    bool result = false;
    int  fileStep = (width*channels + 1) & -2;
    int  y;

    assert( data && width > 0 && height > 0 && step >= fileStep);
    
    if( m_strm.Open( m_filename ) )
    {
        m_strm.PutBytes( fmtSignSunRas, (int)strlen(fmtSignSunRas) );
        m_strm.PutDWord( width );
        m_strm.PutDWord( height );
        m_strm.PutDWord( channels*8 );
        m_strm.PutDWord( fileStep*height );
        m_strm.PutDWord( RAS_STANDARD );
        m_strm.PutDWord( RMT_NONE );
        m_strm.PutDWord( 0 );

        for( y = 0; y < height; y++, data += step )
            m_strm.PutBytes( data, fileStep );
        
        m_strm.Close();
        result = true;
    }
    return result;
}

