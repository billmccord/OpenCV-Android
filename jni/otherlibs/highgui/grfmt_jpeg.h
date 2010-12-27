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

#ifndef _GRFMT_JPEG_H_
#define _GRFMT_JPEG_H_

#include "grfmt_base.h"
#include "bitstrm.h"

#ifdef HAVE_JPEG

/* IJG-based version */

class GrFmtJpegReader : public GrFmtReader
{
public:
    
    GrFmtJpegReader( const char* filename );
    ~GrFmtJpegReader();

    bool  ReadData( uchar* data, int step, int color );
    bool  ReadHeader();
    void  Close();

protected:

    void* m_cinfo; // pointer to IJG JPEG codec structure
    void* m_jerr; // pointer to error processing manager state
    FILE* m_f;
};


class GrFmtJpegWriter : public GrFmtWriter
{
public:
    
    GrFmtJpegWriter( const char* filename );
    ~GrFmtJpegWriter();

    bool  WriteImage( const uchar* data, int step,
                      int width, int height, int depth, int channels );
};

#else

/* hand-crafted implementation */

class RJpegBitStream : public RMBitStream
{
public:
    RMByteStream  m_low_strm;
    
    RJpegBitStream();
    ~RJpegBitStream();

    virtual bool  Open( const char* filename );
    virtual void  Close();

    void  Flush(); // flushes high-level bit stream
    void  AlignOnByte();
    int   FindMarker();

protected:
    virtual void  ReadBlock();
};


//////////////////// JPEG reader /////////////////////

class GrFmtJpegReader : public GrFmtReader
{
public:
    
    GrFmtJpegReader( const char* filename );
    ~GrFmtJpegReader();

    bool  ReadData( uchar* data, int step, int color );
    bool  ReadHeader();
    void  Close();

protected:

    int   m_offset; // offset of first scan
    int   m_version; // JFIF version
    int   m_planes; // 3 (YCrCb) or 1 (Gray)
    int   m_precision; // 8 or 12-bit per sample
    int   m_type; // SOF type
    int   m_MCUs; // MCUs in restart interval
    int   m_ss, m_se, m_ah, m_al; // progressive JPEG parameters
    
    // information about each component
    struct cmp_info
    {
        char h;  // horizontal sampling factor
        char v;  // vertical   sampling factor
        char tq; // quantization table index
        char td, ta; // DC & AC huffman tables
        int  dc_pred; // DC predictor
    };
    
    cmp_info m_ci[3];

    int     m_tq[4][64];
    bool    m_is_tq[4];
    
    short*  m_td[4];
    bool    m_is_td[4];
    
    short*  m_ta[4];
    bool    m_is_ta[4];
    
    RJpegBitStream  m_strm;

protected:

    bool  LoadQuantTables( int length );
    bool  LoadHuffmanTables( int length );
    void  ProcessScan( int* idx, int ns, uchar* data, int step, int color );
    void  ResetDecoder();
    void  GetBlock( int* block, int c );
};


//////////////////// JPEG-specific output bitstream ///////////////////////

class WJpegBitStream : public WMBitStream
{
public:
    WMByteStream  m_low_strm;
    
    WJpegBitStream();
    ~WJpegBitStream();

    virtual void  Flush();
    virtual bool  Open( const char* filename );
    virtual void  Close();

protected:
    virtual void  WriteBlock();
};


//////////////////// JPEG reader /////////////////////

class GrFmtJpegWriter : public GrFmtWriter
{
public:
    
    GrFmtJpegWriter( const char* filename );
    ~GrFmtJpegWriter();

    bool  WriteImage( const uchar* data, int step,
                      int width, int height, int depth, int channels );

protected:

    WJpegBitStream  m_strm;
};

#endif /* HAVE_JPEG */


// JPEG filter factory
class GrFmtJpeg : public GrFmtFilterFactory
{
public:
    
    GrFmtJpeg();
    ~GrFmtJpeg();

    GrFmtReader* NewReader( const char* filename );
    GrFmtWriter* NewWriter( const char* filename );
};


#endif/*_GRFMT_JPEG_H_*/
