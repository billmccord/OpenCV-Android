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

#ifndef _GRFMT_JASPER_H_
#define _GRFMT_JASPER_H_

#ifdef HAVE_JASPER

#ifdef WIN32
#define JAS_WIN_MSVC_BUILD 1
#endif

#include <jasper/jasper.h>
// FIXME bad hack
#undef uchar
#undef ulong
#include "grfmt_base.h"


/* libpng version only */

class GrFmtJpeg2000Reader : public GrFmtReader
{
public:

    GrFmtJpeg2000Reader( const char* filename );
    ~GrFmtJpeg2000Reader();

    bool  CheckFormat( const char* signature );
    bool  ReadData( uchar* data, int step, int color );
    bool  ReadHeader();
    void  Close();

protected:
    bool  ReadComponent8u( uchar *data, jas_matrix_t *buffer, int step, int cmpt,
                           int maxval, int offset, int ncmpts );
    bool  ReadComponent16u( unsigned short *data, jas_matrix_t *buffer, int step, int cmpt,
                            int maxval, int offset, int ncmpts );

    jas_stream_t *m_stream;
    jas_image_t  *m_image;
};


class GrFmtJpeg2000Writer : public GrFmtWriter
{
public:

    GrFmtJpeg2000Writer( const char* filename );
    ~GrFmtJpeg2000Writer();

    bool  IsFormatSupported( int depth );
    bool  WriteImage( const uchar* data, int step,
                      int width, int height, int depth, int channels );
protected:
    bool  WriteComponent8u( jas_image_t *img, const uchar *data,
                            int step, int ncmpts, int w, int h );
    bool  WriteComponent16u( jas_image_t *img, const unsigned short *data,
                             int step, int ncmpts, int w, int h );
};


// PNG filter factory
class GrFmtJpeg2000 : public GrFmtFilterFactory
{
public:

    GrFmtJpeg2000();
    ~GrFmtJpeg2000();

    GrFmtReader* NewReader( const char* filename );
    GrFmtWriter* NewWriter( const char* filename );
};

#endif

#endif/*_GRFMT_JASPER_H_*/
