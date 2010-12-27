/*
 *  grfmt_imageio.h
 *  
 *
 *  Created by Morgan Conbere on 5/17/07.
 *
 */

#ifndef _GRFMT_IMAGEIO_H_
#define _GRFMT_IMAGEIO_H_

#ifdef HAVE_IMAGEIO

#include "grfmt_base.h"
#include <ApplicationServices/ApplicationServices.h>

class GrFmtImageIOReader : public GrFmtReader
{
public:
    
    GrFmtImageIOReader( const char* filename );
    ~GrFmtImageIOReader();
    
    bool  ReadData( uchar* data, int step, int color );
    bool  ReadHeader();
    void  Close();

protected:
    
    CGImageRef imageRef;
};

class GrFmtImageIOWriter : public GrFmtWriter
{
public:
    
    GrFmtImageIOWriter( const char* filename );
    ~GrFmtImageIOWriter();

    bool  WriteImage( const uchar* data, int step,
                      int width, int height, int depth, int channels );
};

// ImageIO filter factory
class GrFmtImageIO :public GrFmtFilterFactory
{
public:
    
    GrFmtImageIO();
    ~GrFmtImageIO();
    
    bool CheckFile( const char* filename );
    
    GrFmtReader* NewReader( const char* filename );
    GrFmtWriter* NewWriter( const char* filename );
};

#endif/*HAVE_IMAGEIO*/

#endif/*_GRFMT_IMAGEIO_H_*/
