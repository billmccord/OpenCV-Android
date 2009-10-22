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

#ifdef HAVE_JASPER

#include "grfmt_jpeg2000.h"

// JPEG-2000 Filter Factory
GrFmtJpeg2000::GrFmtJpeg2000()
{
    m_sign_len = 12;
    m_signature = "\x00\x00\x00\x0cjP  \r\n\x87\n";
    m_description = "JPEG-2000 files (*.jp2)";
    jas_init();
}


GrFmtJpeg2000::~GrFmtJpeg2000()
{
    jas_cleanup();
}


GrFmtReader* GrFmtJpeg2000::NewReader( const char* filename )
{
    return new GrFmtJpeg2000Reader( filename );
}


GrFmtWriter* GrFmtJpeg2000::NewWriter( const char* filename )
{
    return new GrFmtJpeg2000Writer( filename );
}


/////////////////////// GrFmtJpeg2000Reader ///////////////////

GrFmtJpeg2000Reader::GrFmtJpeg2000Reader( const char* filename ) : GrFmtReader( filename )
{
	m_stream = 0;
        m_image = 0;
}


GrFmtJpeg2000Reader::~GrFmtJpeg2000Reader()
{
}


void  GrFmtJpeg2000Reader::Close()
{
    if( m_stream )
    {
        jas_stream_close( m_stream );
        m_stream = 0;
    }

    if( m_image )
    {
        jas_image_destroy( m_image );
        m_image = 0;
    }
    GrFmtReader::Close();
}


bool  GrFmtJpeg2000Reader::ReadHeader()
{
    bool result = false;

    Close();

    m_stream = jas_stream_fopen( m_filename, "rb" );
    if( m_stream )
    {
        m_image = jas_image_decode( m_stream, -1, 0 );
        if( m_image ) {
            m_width = jas_image_width( m_image );
            m_height = jas_image_height( m_image );

            int cntcmpts = 0; // count the known components
            int numcmpts = jas_image_numcmpts( m_image );
            for( int i = 0; i < numcmpts; i++ )
            {
                int depth = jas_image_cmptprec( m_image, i );
                if( depth > m_bit_depth )
                    m_bit_depth = depth;
                if( m_bit_depth > 8 )
                    m_bit_depth = 16;

                if( jas_image_cmpttype( m_image, i ) > 2 )
                    continue;
                cntcmpts++;
            }

            if( cntcmpts )
            {
                m_iscolor = (cntcmpts > 1);

                result = true;
            }
        }
    }

    if( !result )
        Close();

    return result;
}


bool  GrFmtJpeg2000Reader::ReadData( uchar* data, int step, int color )
{
    bool result = false;

    color = color > 0 || ( m_iscolor && color < 0 );

    if( m_stream && m_image )
    {
        bool convert;
        int colorspace;
        if( color )
        {
            convert = (jas_image_clrspc( m_image ) != JAS_CLRSPC_SRGB);
            colorspace = JAS_CLRSPC_SRGB;
        }
        else
        {
            convert = (jas_clrspc_fam( jas_image_clrspc( m_image ) ) != JAS_CLRSPC_FAM_GRAY);
            colorspace = JAS_CLRSPC_SGRAY; // TODO GENGRAY or SGRAY?
        }

        // convert to the desired colorspace
        if( convert )
        {
            jas_cmprof_t *clrprof = jas_cmprof_createfromclrspc( colorspace );
            if( clrprof )
            {
                jas_image_t *img = jas_image_chclrspc( m_image, clrprof, JAS_CMXFORM_INTENT_RELCLR );
                if( img )
                {
                    jas_image_destroy( m_image );
                    m_image = img;
                    result = true;
                }
                else
                    fprintf(stderr, "JPEG 2000 LOADER ERROR: cannot convert colorspace\n");
                jas_cmprof_destroy( clrprof );
            }
            else
                fprintf(stderr, "JPEG 2000 LOADER ERROR: unable to create colorspace\n");
        }
        else
            result = true;

        if( result )
        {
            int ncmpts;
            int cmptlut[3];
            if( color )
            {
                cmptlut[0] = jas_image_getcmptbytype( m_image, JAS_IMAGE_CT_RGB_B );
                cmptlut[1] = jas_image_getcmptbytype( m_image, JAS_IMAGE_CT_RGB_G );
                cmptlut[2] = jas_image_getcmptbytype( m_image, JAS_IMAGE_CT_RGB_R );
                if( cmptlut[0] < 0 || cmptlut[1] < 0 || cmptlut[0] < 0 )
                    result = false;
                ncmpts = 3;
            }
            else
            {
                cmptlut[0] = jas_image_getcmptbytype( m_image, JAS_IMAGE_CT_GRAY_Y );
                if( cmptlut[0] < 0 )
                    result = false;
                ncmpts = 1;
            }

            if( result )
            {
                for( int i = 0; i < ncmpts; i++ )
                {
                    int maxval = 1 << jas_image_cmptprec( m_image, cmptlut[i] );
                    int offset =  jas_image_cmptsgnd( m_image, cmptlut[i] ) ? maxval / 2 : 0;

                    int yend = jas_image_cmptbry( m_image, cmptlut[i] );
                    int ystep = jas_image_cmptvstep( m_image, cmptlut[i] );
                    int xend = jas_image_cmptbrx( m_image, cmptlut[i] );
                    int xstep = jas_image_cmpthstep( m_image, cmptlut[i] );

                    jas_matrix_t *buffer = jas_matrix_create( yend / ystep, xend / xstep );
                    if( buffer )
                    {
                        if( !jas_image_readcmpt( m_image, cmptlut[i], 0, 0, xend / xstep, yend / ystep, buffer ))
                        {
                            if( m_bit_depth == 8 || !m_native_depth )
                                result = ReadComponent8u( data + i, buffer, step, cmptlut[i], maxval, offset, ncmpts );
                            else
                                result = ReadComponent16u( ((unsigned short *)data) + i, buffer, step / 2, cmptlut[i], maxval, offset, ncmpts );
                            if( !result )
                            {
                                i = ncmpts;
                                result = false;
                            }
                        }
                        jas_matrix_destroy( buffer );
                    }
                }
            }
        }
        else
	    fprintf(stderr, "JPEG2000 LOADER ERROR: colorspace conversion failed\n" );
    }

    Close();

    return result;
}


bool  GrFmtJpeg2000Reader::ReadComponent8u( uchar *data, jas_matrix_t *buffer,
                                            int step, int cmpt,
                                            int maxval, int offset, int ncmpts )
{
    int xstart = jas_image_cmpttlx( m_image, cmpt );
    int xend = jas_image_cmptbrx( m_image, cmpt );
    int xstep = jas_image_cmpthstep( m_image, cmpt );
    int xoffset = jas_image_tlx( m_image );
    int ystart = jas_image_cmpttly( m_image, cmpt );
    int yend = jas_image_cmptbry( m_image, cmpt );
    int ystep = jas_image_cmptvstep( m_image, cmpt );
    int yoffset = jas_image_tly( m_image );
    int x, y, x1, y1, j;
    int rshift = cvRound(log(maxval/256.)/log(2.));
    int lshift = MAX(0, -rshift);
    rshift = MAX(0, rshift);
    int delta = (rshift > 0 ? 1 << (rshift - 1) : 0) + offset;

    for( y = 0; y < yend - ystart; )
    {
        jas_seqent_t* pix_row = &jas_matrix_get( buffer, y / ystep, 0 );
        uchar* dst = data + (y - yoffset) * step - xoffset;

        if( xstep == 1 )
        {
            if( maxval == 256 && offset == 0 )
                for( x = 0; x < xend - xstart; x++ )
                {
                    int pix = pix_row[x];
                    dst[x*ncmpts] = CV_CAST_8U(pix);
                }
            else
                for( x = 0; x < xend - xstart; x++ )
                {
                    int pix = ((pix_row[x] + delta) >> rshift) << lshift;
                    dst[x*ncmpts] = CV_CAST_8U(pix);
                }
        }
        else if( xstep == 2 && offset == 0 )
            for( x = 0, j = 0; x < xend - xstart; x += 2, j++ )
            {
                int pix = ((pix_row[j] + delta) >> rshift) << lshift;
                dst[x*ncmpts] = dst[(x+1)*ncmpts] = CV_CAST_8U(pix);
            }
        else
            for( x = 0, j = 0; x < xend - xstart; j++ )
            {
                int pix = ((pix_row[j] + delta) >> rshift) << lshift;
                pix = CV_CAST_8U(pix);
                for( x1 = x + xstep; x < x1; x++ )
                    dst[x*ncmpts] = (uchar)pix;
            }
        y1 = y + ystep;
        for( ++y; y < y1; y++, dst += step )
            for( x = 0; x < xend - xstart; x++ )
                dst[x*ncmpts + step] = dst[x*ncmpts];
    }

    return true;
}


bool  GrFmtJpeg2000Reader::ReadComponent16u( unsigned short *data, jas_matrix_t *buffer,
                                             int step, int cmpt,
                                             int maxval, int offset, int ncmpts )
{
    int xstart = jas_image_cmpttlx( m_image, cmpt );
    int xend = jas_image_cmptbrx( m_image, cmpt );
    int xstep = jas_image_cmpthstep( m_image, cmpt );
    int xoffset = jas_image_tlx( m_image );
    int ystart = jas_image_cmpttly( m_image, cmpt );
    int yend = jas_image_cmptbry( m_image, cmpt );
    int ystep = jas_image_cmptvstep( m_image, cmpt );
    int yoffset = jas_image_tly( m_image );
    int x, y, x1, y1, j;
    int rshift = cvRound(log(maxval/65536.)/log(2.));
    int lshift = MAX(0, -rshift);
    rshift = MAX(0, rshift);
    int delta = (rshift > 0 ? 1 << (rshift - 1) : 0) + offset;

    for( y = 0; y < yend - ystart; )
    {
        jas_seqent_t* pix_row = &jas_matrix_get( buffer, y / ystep, 0 );
        ushort* dst = data + (y - yoffset) * step - xoffset;

        if( xstep == 1 )
        {
            if( maxval == 65536 && offset == 0 )
                for( x = 0; x < xend - xstart; x++ )
                {
                    int pix = pix_row[x];
                    dst[x*ncmpts] = CV_CAST_16U(pix);
                }
            else
                for( x = 0; x < xend - xstart; x++ )
                {
                    int pix = ((pix_row[x] + delta) >> rshift) << lshift;
                    dst[x*ncmpts] = CV_CAST_16U(pix);
                }
        }
        else if( xstep == 2 && offset == 0 )
            for( x = 0, j = 0; x < xend - xstart; x += 2, j++ )
            {
                int pix = ((pix_row[j] + delta) >> rshift) << lshift;
                dst[x*ncmpts] = dst[(x+1)*ncmpts] = CV_CAST_16U(pix);
            }
        else
            for( x = 0, j = 0; x < xend - xstart; j++ )
            {
                int pix = ((pix_row[j] + delta) >> rshift) << lshift;
                pix = CV_CAST_16U(pix);
                for( x1 = x + xstep; x < x1; x++ )
                    dst[x*ncmpts] = (ushort)pix;
            }
        y1 = y + ystep;
        for( ++y; y < y1; y++, dst += step )
            for( x = 0; x < xend - xstart; x++ )
                dst[x*ncmpts + step] = dst[x*ncmpts];
    }

    return true;
}


/////////////////////// GrFmtJpeg2000Writer ///////////////////


GrFmtJpeg2000Writer::GrFmtJpeg2000Writer( const char* filename ) : GrFmtWriter( filename )
{
}


GrFmtJpeg2000Writer::~GrFmtJpeg2000Writer()
{
}


bool  GrFmtJpeg2000Writer::IsFormatSupported( int depth )
{
    return depth == IPL_DEPTH_8U || depth == IPL_DEPTH_16U;
}


bool  GrFmtJpeg2000Writer::WriteImage( const uchar* data, int step,
                                  int width, int height, int depth, int channels )
{
    if( channels > 3 || channels < 1 )
        return false;

    jas_image_cmptparm_t component_info[3];
    for( int i = 0; i < channels; i++ )
    {
        component_info[i].tlx = 0;
        component_info[i].tly = 0;
        component_info[i].hstep = 1;
        component_info[i].vstep = 1;
        component_info[i].width = width;
        component_info[i].height = height;
        component_info[i].prec = depth;
        component_info[i].sgnd = 0;
    }
    jas_image_t *img = jas_image_create( channels, component_info, (channels == 1) ? JAS_CLRSPC_SGRAY : JAS_CLRSPC_SRGB );
    if( !img )
        return false;

    if(channels == 1)
        jas_image_setcmpttype( img, 0, JAS_IMAGE_CT_GRAY_Y );
    else
    {
        jas_image_setcmpttype( img, 0, JAS_IMAGE_CT_RGB_B );
        jas_image_setcmpttype( img, 1, JAS_IMAGE_CT_RGB_G );
        jas_image_setcmpttype( img, 2, JAS_IMAGE_CT_RGB_R );
    }

    bool result;
    if( depth == 8 )
        result = WriteComponent8u( img, data, step, channels, width, height );
    else
        result = WriteComponent16u( img, (const unsigned short *)data, step / 2, channels, width, height );
    if( result )
    {
        jas_stream_t *stream = jas_stream_fopen( m_filename, "wb" );
        if( stream )
        {
            result = !jas_image_encode( img, stream, jas_image_strtofmt( "jp2" ), "" );

            jas_stream_close( stream );
        }

    }
    jas_image_destroy( img );

    return result;
}


bool  GrFmtJpeg2000Writer::WriteComponent8u( jas_image_t *img, const uchar *data,
                                             int step, int ncmpts, int w, int h )
{
    jas_matrix_t *row = jas_matrix_create( 1, w );
    if(!row)
        return false;

    for( int y = 0; y < h; y++, data += step )
    {
        for( int i = 0; i < ncmpts; i++ )
        {
            for( int x = 0; x < w; x++)
                jas_matrix_setv( row, x, data[x * ncmpts + i] );
            jas_image_writecmpt( img, i, 0, y, w, 1, row );
        }
    }

    jas_matrix_destroy( row );

    return true;
}


bool  GrFmtJpeg2000Writer::WriteComponent16u( jas_image_t *img, const unsigned short *data,
                                              int step, int ncmpts, int w, int h )
{
    jas_matrix_t *row = jas_matrix_create( 1, w );
    if(!row)
        return false;

    for( int y = 0; y < h; y++, data += step )
    {
        for( int i = 0; i < ncmpts; i++ )
        {
            for( int x = 0; x < w; x++)
                jas_matrix_setv( row, x, data[x * ncmpts + i] );
            jas_image_writecmpt( img, i, 0, y, w, 1, row );
        }
    }

    jas_matrix_destroy( row );

    return true;
}

#endif

/* End of file. */
