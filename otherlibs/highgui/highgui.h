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

#ifndef _HIGH_GUI_
#define _HIGH_GUI_

#ifndef SKIP_INCLUDES

  #include "cxcore.h"
  #if defined WIN32 || defined WIN64
    #include <windows.h>
  #endif

#else // SKIP_INCLUDES

  #if defined WIN32 || defined WIN64
    #define CV_CDECL __cdecl
    #define CV_STDCALL __stdcall
  #else
    #define CV_CDECL
    #define CV_STDCALL
  #endif

  #ifndef CV_EXTERN_C
    #ifdef __cplusplus
      #define CV_EXTERN_C extern "C"
      #define CV_DEFAULT(val) = val
    #else
      #define CV_EXTERN_C
      #define CV_DEFAULT(val)
    #endif
  #endif

  #ifndef CV_EXTERN_C_FUNCPTR
    #ifdef __cplusplus
      #define CV_EXTERN_C_FUNCPTR(x) extern "C" { typedef x; }
    #else
      #define CV_EXTERN_C_FUNCPTR(x) typedef x
    #endif
  #endif

  #ifndef CV_INLINE
    #if defined __cplusplus
      #define CV_INLINE inline
    #elif (defined WIN32 || defined WIN64) && !defined __GNUC__
      #define CV_INLINE __inline
    #else
      #define CV_INLINE static
    #endif
  #endif /* CV_INLINE */

  #if (defined WIN32 || defined WIN64) && defined CVAPI_EXPORTS
    #define CV_EXPORTS __declspec(dllexport)
  #else
    #define CV_EXPORTS
  #endif

  #ifndef CVAPI
    #define CVAPI(rettype) CV_EXTERN_C CV_EXPORTS rettype CV_CDECL
  #endif

#endif // SKIP_INCLUDES

#if defined(_CH_)
  #pragma package <chopencv>
  #include <chdl.h>
  LOAD_CHDL(highgui)
#endif

#ifdef __cplusplus
  extern "C" {
#endif /* __cplusplus */

#define CV_EVENT_MOUSEMOVE      0
#define CV_EVENT_LBUTTONDOWN    1
#define CV_EVENT_RBUTTONDOWN    2
#define CV_EVENT_MBUTTONDOWN    3
#define CV_EVENT_LBUTTONUP      4
#define CV_EVENT_RBUTTONUP      5
#define CV_EVENT_MBUTTONUP      6
#define CV_EVENT_LBUTTONDBLCLK  7
#define CV_EVENT_RBUTTONDBLCLK  8
#define CV_EVENT_MBUTTONDBLCLK  9

#define CV_EVENT_FLAG_LBUTTON   1
#define CV_EVENT_FLAG_RBUTTON   2
#define CV_EVENT_FLAG_MBUTTON   4
#define CV_EVENT_FLAG_CTRLKEY   8
#define CV_EVENT_FLAG_SHIFTKEY  16
#define CV_EVENT_FLAG_ALTKEY    32


/* 8bit, color or not */
#define CV_LOAD_IMAGE_UNCHANGED  -1
/* 8bit, gray */
#define CV_LOAD_IMAGE_GRAYSCALE   0
/* ?, color */
#define CV_LOAD_IMAGE_COLOR       1
/* any depth, ? */
#define CV_LOAD_IMAGE_ANYDEPTH    2
/* ?, any color */
#define CV_LOAD_IMAGE_ANYCOLOR    4

/* load image from file
  iscolor can be a combination of above flags where CV_LOAD_IMAGE_UNCHANGED
  overrides the other flags
  using CV_LOAD_IMAGE_ANYCOLOR alone is equivalent to CV_LOAD_IMAGE_UNCHANGED
  unless CV_LOAD_IMAGE_ANYDEPTH is specified images are converted to 8bit
*/
CVAPI(IplImage*) cvLoadImage( const char* filename, int iscolor CV_DEFAULT(CV_LOAD_IMAGE_COLOR));
CVAPI(CvMat*) cvLoadImageM( const char* filename, int iscolor CV_DEFAULT(CV_LOAD_IMAGE_COLOR));

/* save image to file */
CVAPI(int) cvSaveImage( const char* filename, const CvArr* image );

#define CV_CVTIMG_FLIP      1
#define CV_CVTIMG_SWAP_RB   2
/* utility function: convert one image to another with optional vertical flip */
CVAPI(void) cvConvertImage( const CvArr* src, CvArr* dst, int flags CV_DEFAULT(0));

/* wait for key event infinitely (delay<=0) or for "delay" milliseconds */
CVAPI(int) cvWaitKey(int delay CV_DEFAULT(0));



/****************************************************************************************\
*                              Obsolete functions/synonyms                               *
\****************************************************************************************/

#ifndef HIGHGUI_NO_BACKWARD_COMPATIBILITY
    #define HIGHGUI_BACKWARD_COMPATIBILITY
#endif

#ifdef HIGHGUI_BACKWARD_COMPATIBILITY

#define cvCaptureFromFile cvCreateFileCapture
#define cvCaptureFromCAM cvCreateCameraCapture
#define cvCaptureFromAVI cvCaptureFromFile
#define cvCreateAVIWriter cvCreateVideoWriter
#define cvWriteToAVI cvWriteFrame
#define cvAddSearchPath(path)
#define cvvInitSystem cvInitSystem
#define cvvNamedWindow cvNamedWindow
#define cvvResizeWindow cvResizeWindow
#define cvvDestroyWindow cvDestroyWindow
#define cvvCreateTrackbar cvCreateTrackbar
#define cvvLoadImage(name) cvLoadImage((name),1)
#define cvvSaveImage cvSaveImage
#define cvvAddSearchPath cvAddSearchPath
#define cvvWaitKey(name) cvWaitKey(0)
#define cvvWaitKeyEx(name,delay) cvWaitKey(delay)
#define cvvConvertImage cvConvertImage
#define HG_AUTOSIZE CV_WINDOW_AUTOSIZE
#define set_preprocess_func cvSetPreprocessFuncWin32
#define set_postprocess_func cvSetPostprocessFuncWin32

#ifdef WIN32

typedef int (CV_CDECL * CvWin32WindowCallback)(HWND, UINT, WPARAM, LPARAM, int*);
CVAPI(void) cvSetPreprocessFuncWin32( CvWin32WindowCallback on_preprocess );
CVAPI(void) cvSetPostprocessFuncWin32( CvWin32WindowCallback on_postprocess );

CV_INLINE int iplWidth( const IplImage* img );
CV_INLINE int iplWidth( const IplImage* img )
{
    return !img ? 0 : !img->roi ? img->width : img->roi->width;
}

CV_INLINE int iplHeight( const IplImage* img );
CV_INLINE int iplHeight( const IplImage* img )
{
    return !img ? 0 : !img->roi ? img->height : img->roi->height;
}

#endif

#endif /* obsolete functions */

/* For use with Win32 */
#ifdef WIN32

CV_INLINE RECT NormalizeRect( RECT r );
CV_INLINE RECT NormalizeRect( RECT r )
{
    int t;

    if( r.left > r.right )
    {
        t = r.left;
        r.left = r.right;
        r.right = t;
    }

    if( r.top > r.bottom )
    {
        t = r.top;
        r.top = r.bottom;
        r.bottom = t;
    }

    return r;
}

CV_INLINE CvRect RectToCvRect( RECT sr );
CV_INLINE CvRect RectToCvRect( RECT sr )
{
    sr = NormalizeRect( sr );
    return cvRect( sr.left, sr.top, sr.right - sr.left, sr.bottom - sr.top );
}

CV_INLINE RECT CvRectToRect( CvRect sr );
CV_INLINE RECT CvRectToRect( CvRect sr )
{
    RECT dr;
    dr.left = sr.x;
    dr.top = sr.y;
    dr.right = sr.x + sr.width;
    dr.bottom = sr.y + sr.height;

    return dr;
}

CV_INLINE IplROI RectToROI( RECT r );
CV_INLINE IplROI RectToROI( RECT r )
{
    IplROI roi;
    r = NormalizeRect( r );
    roi.xOffset = r.left;
    roi.yOffset = r.top;
    roi.width = r.right - r.left;
    roi.height = r.bottom - r.top;
    roi.coi = 0;

    return roi;
}

#endif /* WIN32 */

#ifdef __cplusplus
}  /* end of extern "C" */
#endif /* __cplusplus */


#if defined __cplusplus && (!defined WIN32 || !defined (__GNUC__)) && !defined CV_NO_CVV_IMAGE

#define CImage CvvImage

/* CvvImage class definition */
class CV_EXPORTS CvvImage
{
public:
    CvvImage();
    virtual ~CvvImage();

    /* Create image (BGR or grayscale) */
    virtual bool  Create( int width, int height, int bits_per_pixel, int image_origin = 0 );

    /* Load image from specified file */
    virtual bool  Load( const char* filename, int desired_color = 1 );

    /* Load rectangle from the file */
    virtual bool  LoadRect( const char* filename,
                            int desired_color, CvRect r );

#ifdef WIN32
    virtual bool  LoadRect( const char* filename,
                            int desired_color, RECT r )
    {
        return LoadRect( filename, desired_color,
                         cvRect( r.left, r.top, r.right - r.left, r.bottom - r.top ));
    }
#endif

    /* Save entire image to specified file. */
    virtual bool  Save( const char* filename );

    /* Get copy of input image ROI */
    virtual void  CopyOf( CvvImage& image, int desired_color = -1 );
    virtual void  CopyOf( IplImage* img, int desired_color = -1 );

    IplImage* GetImage() { return m_img; };
    virtual void  Destroy(void);

    /* width and height of ROI */
    int Width() { return !m_img ? 0 : !m_img->roi ? m_img->width : m_img->roi->width; };
    int Height() { return !m_img ? 0 : !m_img->roi ? m_img->height : m_img->roi->height;};
    int Bpp() { return m_img ? (m_img->depth & 255)*m_img->nChannels : 0; };

    virtual void  Fill( int color );


protected:

    IplImage*  m_img;
};

#endif /* __cplusplus */

#endif /* _HIGH_GUI_ */
