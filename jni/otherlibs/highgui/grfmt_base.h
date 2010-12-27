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

#ifndef _GRFMT_BASE_H_
#define _GRFMT_BASE_H_

#if _MSC_VER >= 1200
    #pragma warning( disable: 4514 )
    #pragma warning( disable: 4711 )
    #pragma warning( disable: 4611 )
#endif

#include "utils.h"
#include "bitstrm.h"

#define  RBS_BAD_HEADER     -125  /* invalid image header */
#define  BAD_HEADER_ERR()   goto bad_header_exit

#ifndef _MAX_PATH
    #define _MAX_PATH    1024
#endif


///////////////////////////////// base class for readers ////////////////////////
class   GrFmtReader
{
public:
    
    GrFmtReader( const char* filename );
    virtual ~GrFmtReader();

    int   GetWidth()  { return m_width; };
    int   GetHeight() { return m_height; };
    bool  IsColor()   { return m_iscolor; };
    int   GetDepth()  { return m_bit_depth; };
    void  UseNativeDepth( bool yes ) { m_native_depth = yes; };
    bool  IsFloat()   { return m_isfloat; };

    virtual bool  ReadHeader() = 0;
    virtual bool  ReadData( uchar* data, int step, int color ) = 0;
    virtual void  Close();

protected:

    bool    m_iscolor;
    int     m_width;    // width  of the image ( filled by ReadHeader )
    int     m_height;   // height of the image ( filled by ReadHeader )
    int     m_bit_depth;// bit depth per channel (normally 8)
    char    m_filename[_MAX_PATH]; // filename
    bool    m_native_depth;// use the native bit depth of the image
    bool    m_isfloat;  // is image saved as float or double?
};


///////////////////////////// base class for writers ////////////////////////////
class   GrFmtWriter
{
public:

    GrFmtWriter( const char* filename );
    virtual ~GrFmtWriter() {};
    virtual bool  IsFormatSupported( int depth );
    virtual bool  WriteImage( const uchar* data, int step,
                              int width, int height, int depth, int channels ) = 0;
protected:
    char    m_filename[_MAX_PATH]; // filename
};


////////////////////////////// base class for filter factories //////////////////
class   GrFmtFilterFactory
{
public:

    GrFmtFilterFactory();
    virtual ~GrFmtFilterFactory() {};

    const char*  GetDescription() { return m_description; };
    int     GetSignatureLength()  { return m_sign_len; };
	virtual bool CheckFile( const char* filename );
    virtual bool CheckSignature( const char* signature );
    virtual bool CheckExtension( const char* filename );
    virtual GrFmtReader* NewReader( const char* filename ) = 0;
    virtual GrFmtWriter* NewWriter( const char* filename ) = 0;

protected:
    const char* m_description;
           // graphic format description in form:
           // <Some textual description>( *.<extension1> [; *.<extension2> ...]).
           // the textual description can not contain symbols '(', ')'
           // and may be, some others. It is safe to use letters, digits and spaces only.
           // e.g. "Targa (*.tga)",
           // or "Portable Graphic Format (*.pbm;*.pgm;*.ppm)"

    int          m_sign_len;    // length of the signature of the format
    const char*  m_signature;   // signature of the format
};


/////////////////////////// list of graphic format filters ///////////////////////////////

typedef void* ListPosition;

class   GrFmtFactoriesList
{
public:

    GrFmtFactoriesList();
    virtual ~GrFmtFactoriesList();
    void  RemoveAll();
    bool  AddFactory( GrFmtFilterFactory* factory );
    int   FactoriesCount() { return m_curFactories; };
    ListPosition  GetFirstFactoryPos();
    GrFmtFilterFactory*  GetNextFactory( ListPosition& pos );
    virtual GrFmtReader*  FindReader( const char* filename );
    virtual GrFmtWriter*  FindWriter( const char* filename );

protected:

    GrFmtFilterFactory** m_factories;
    int  m_maxFactories;
    int  m_curFactories;
};

#endif/*_GRFMT_BASE_H_*/
