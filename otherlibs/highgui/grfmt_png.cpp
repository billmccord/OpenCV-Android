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

#ifdef HAVE_PNG

/****************************************************************************************\
    This part of the file implements PNG codec on base of libpng library,
    in particular, this code is based on example.c from libpng
    (see otherlibs/_graphics/readme.txt for copyright notice)
    and png2bmp sample from libpng distribution (Copyright (C) 1999-2001 MIYASAKA Masaru)
\****************************************************************************************/

#if defined WIN32 || defined HAVE_PNG_H
#include <png.h>
#else
#include <libpng/png.h>
#endif
#include "grfmt_png.h"

// PNG Filter Factory
GrFmtPng::GrFmtPng()
{
    m_sign_len = 8;
    m_signature = "\x89\x50\x4e\x47\xd\xa\x1a\xa";
    m_description = "Portable Network Graphics files (*.png)";
}


GrFmtPng::~GrFmtPng()
{
}


GrFmtReader* GrFmtPng::NewReader( const char* filename )
{
    return new GrFmtPngReader( filename );
}


GrFmtWriter* GrFmtPng::NewWriter( const char* filename )
{
    return new GrFmtPngWriter( filename );
}


bool  GrFmtPng::CheckSignature( const char* signature )
{
    return png_check_sig( (uchar*)signature, m_sign_len ) != 0;
}

/////////////////////// GrFmtPngReader ///////////////////

GrFmtPngReader::GrFmtPngReader( const char* filename ) : GrFmtReader( filename )
{
    m_color_type = m_bit_depth = 0;
    m_png_ptr = 0;
    m_info_ptr = m_end_info = 0;
    m_f = 0;
}


GrFmtPngReader::~GrFmtPngReader()
{
}


void  GrFmtPngReader::Close()
{
    if( m_f )
    {
        fclose( m_f );
        m_f = 0;
    }

    if( m_png_ptr )
    {
        png_structp png_ptr = (png_structp)m_png_ptr;
        png_infop info_ptr = (png_infop)m_info_ptr;
        png_infop end_info = (png_infop)m_end_info;
        png_destroy_read_struct( &png_ptr, &info_ptr, &end_info );
        m_png_ptr = m_info_ptr = m_end_info = 0;
    }
    GrFmtReader::Close();
}


bool  GrFmtPngReader::ReadHeader()
{
    bool result = false;

    Close();

    png_structp png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, 0, 0, 0 );

    if( png_ptr )
    {
        png_infop info_ptr = png_create_info_struct( png_ptr );
        png_infop end_info = png_create_info_struct( png_ptr );

        m_png_ptr = png_ptr;
        m_info_ptr = info_ptr;
        m_end_info = end_info;

        if( info_ptr && end_info )
        {
            if( setjmp( png_ptr->jmpbuf ) == 0 )
            {
                m_f = fopen( m_filename, "rb" );
                if( m_f )
                {
                    png_uint_32 width, height;
                    int bit_depth, color_type;
                    
                    png_init_io( png_ptr, m_f );
                    png_read_info( png_ptr, info_ptr );

                    png_get_IHDR( png_ptr, info_ptr, &width, &height,
                                  &bit_depth, &color_type, 0, 0, 0 );

                    m_iscolor = color_type == PNG_COLOR_TYPE_RGB ||
                                color_type == PNG_COLOR_TYPE_RGB_ALPHA ||
                                color_type == PNG_COLOR_TYPE_PALETTE;

                    m_width = (int)width;
                    m_height = (int)height;
                    m_color_type = color_type;
                    m_bit_depth = bit_depth;

                    result = true;
                }
            }
        }
    }

    if( !result )
        Close();

    return result;
}


bool  GrFmtPngReader::ReadData( uchar* data, int step, int color )
{
    bool result = false;
    uchar** buffer = 0;

    color = color > 0 || ( m_iscolor && color < 0 );
    
    if( m_png_ptr && m_info_ptr && m_end_info && m_width && m_height )
    {
        png_structp png_ptr = (png_structp)m_png_ptr;
        png_infop info_ptr = (png_infop)m_info_ptr;
        png_infop end_info = (png_infop)m_end_info;
        
        if( setjmp(png_ptr->jmpbuf) == 0 )
        {
            int y;

            if( m_bit_depth > 8 && !m_native_depth )
                png_set_strip_16( png_ptr );
            else if( !isBigEndian() )
                png_set_swap( png_ptr );

            /* observation: png_read_image() writes 400 bytes beyond
             * end of data when reading a 400x118 color png
             * "mpplus_sand.png".  OpenCV crashes even with demo
             * programs.  Looking at the loaded image I'd say we get 4
             * bytes per pixel instead of 3 bytes per pixel.  Test
             * indicate that it is a good idea to always ask for
             * stripping alpha..  18.11.2004 Axel Walthelm
             */
            png_set_strip_alpha( png_ptr );

            if( m_color_type == PNG_COLOR_TYPE_PALETTE )
                png_set_palette_to_rgb( png_ptr );

            if( m_color_type == PNG_COLOR_TYPE_GRAY && m_bit_depth < 8 )
                png_set_gray_1_2_4_to_8( png_ptr );

            if( m_iscolor && color )
                png_set_bgr( png_ptr ); // convert RGB to BGR
            else if( color )
                png_set_gray_to_rgb( png_ptr ); // Gray->RGB
            else
                png_set_rgb_to_gray( png_ptr, 1, -1, -1 ); // RGB->Gray

            png_read_update_info( png_ptr, info_ptr );

            buffer = new uchar*[m_height];

            for( y = 0; y < m_height; y++ )
                buffer[y] = data + y*step;

            png_read_image( png_ptr, buffer );
            png_read_end( png_ptr, end_info );

            result = true;
        }
    }

    Close();
    delete[] buffer;

    return result;
}


/////////////////////// GrFmtPngWriter ///////////////////


GrFmtPngWriter::GrFmtPngWriter( const char* filename ) : GrFmtWriter( filename )
{
}


GrFmtPngWriter::~GrFmtPngWriter()
{
}


bool  GrFmtPngWriter::IsFormatSupported( int depth )
{
    return depth == IPL_DEPTH_8U || depth == IPL_DEPTH_16U;
}

bool  GrFmtPngWriter::WriteImage( const uchar* data, int step,
                                  int width, int height, int depth, int channels )
{
    png_structp png_ptr = png_create_write_struct( PNG_LIBPNG_VER_STRING, 0, 0, 0 );
    png_infop info_ptr = 0;
    FILE* f = 0;
    uchar** buffer = 0;
    int y;
    bool result = false;

    if( depth != IPL_DEPTH_8U && depth != IPL_DEPTH_16U )
        return false;

    if( png_ptr )
    {
        info_ptr = png_create_info_struct( png_ptr );
        
        if( info_ptr )
        {
            if( setjmp( png_ptr->jmpbuf ) == 0 )
            {
                f = fopen( m_filename, "wb" );

                if( f )
                {
                    png_init_io( png_ptr, f );

                    png_set_compression_mem_level( png_ptr, MAX_MEM_LEVEL );

                    png_set_IHDR( png_ptr, info_ptr, width, height, depth,
                        channels == 1 ? PNG_COLOR_TYPE_GRAY :
                        channels == 3 ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_RGBA,
                        PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                        PNG_FILTER_TYPE_DEFAULT );

                    png_write_info( png_ptr, info_ptr );

                    png_set_bgr( png_ptr );
                    if( !isBigEndian() )
                        png_set_swap( png_ptr );

                    buffer = new uchar*[height];
                    for( y = 0; y < height; y++ )
                        buffer[y] = (uchar*)(data + y*step);

                    png_write_image( png_ptr, buffer );
                    png_write_end( png_ptr, info_ptr );

                    delete[] buffer;
                
                    result = true;
                }
            }
        }
    }

    png_destroy_write_struct( &png_ptr, &info_ptr );

    if(f) fclose( f );

    return result;
}

#endif

/* End of file. */


