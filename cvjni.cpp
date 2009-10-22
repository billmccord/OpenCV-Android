/*
OpenCV for Android NDK
Copyright (c) 2006-2009 SIProp Project http://www.siprop.org/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it freely,
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/
#include "cvjni.h"


#define THRESHOLD	10
#define THRESHOLD_MAX_VALUE	255

#define CONTOUR_MAX_LEVEL	1
#define LINE_THICKNESS	2
#define LINE_TYPE	8


JNIEXPORT
jbooleanArray
JNICALL
Java_org_siprop_opencv_OpenCV_findContours(JNIEnv* env,
										jobject thiz,
										jintArray photo_data,
										jint width,
										jint height) {

	// Load Image
	IplImage *sourceImage;
	int* pixels = env->GetIntArrayElements(photo_data, NULL);
	sourceImage = loadPixels(pixels, width, height);
	if(sourceImage == NULL) {
		LOGV("Error loadPixels.");
		return NULL;
	}


	IplImage *grayImage = cvCreateImage( cvGetSize(sourceImage), IPL_DEPTH_8U, 1 );		//	グレースケール画像用IplImage
	IplImage *binaryImage = cvCreateImage( cvGetSize(sourceImage), IPL_DEPTH_8U, 1 );	//	2値画像用IplImage
	IplImage *contourImage = cvCreateImage( cvGetSize(sourceImage), IPL_DEPTH_8U, 3 );	//	輪郭画像用IplImage

	//	BGRからグレースケールに変換する
	cvCvtColor( sourceImage, grayImage, CV_BGR2GRAY );

	//	グレースケールから2値に変換する
	cvThreshold( grayImage, binaryImage, THRESHOLD, THRESHOLD_MAX_VALUE, CV_THRESH_BINARY );

	//	輪郭抽出用のメモリを確保する
	CvMemStorage* storage = cvCreateMemStorage( 0 );	//	抽出された輪郭を保存する領域
	CvSeq* find_contour = NULL;		//	輪郭へのポインタ           

	//	2値画像中の輪郭を見つけ、その数を返す
	int find_contour_num = cvFindContours( 
		binaryImage,			//	入力画像(８ビットシングルチャンネル）
		storage,				//	抽出された輪郭を保存する領域
		&find_contour,			//	一番外側の輪郭へのポインタへのポインタ
		sizeof( CvContour ),	//	シーケンスヘッダのサイズ
		CV_RETR_LIST,			//	抽出モード 
		CV_CHAIN_APPROX_NONE,	//	推定手法
		cvPoint( 0, 0 )			//	オフセット
	);

	//	物体の輪郭を赤色で描画する
	CvScalar red = CV_RGB( 255, 0, 0 );
	cvDrawContours( 
		sourceImage,			//	輪郭を描画する画像
		find_contour,			//	最初の輪郭へのポインタ
		red,					//	外側輪郭線の色
		red,					//	内側輪郭線（穴）の色
		CONTOUR_MAX_LEVEL,		//	描画される輪郭の最大レベル
		LINE_THICKNESS,			//	描画される輪郭線の太さ
		LINE_TYPE,				//	線の種類
		cvPoint( 0, 0 )			//	オフセット
	);   


	jbooleanArray res_array;
	int imageSize;

	CvMat stub, *mat_image;
    int channels, ipl_depth;
    mat_image = cvGetMat( sourceImage, &stub );
    channels = CV_MAT_CN( mat_image->type );

    ipl_depth = cvCvToIplDepth(mat_image->type);

	WLNonFileByteStream* m_strm = new WLNonFileByteStream();
    loadImageBytes(mat_image->data.ptr, mat_image->step, mat_image->width,
                             mat_image->height, ipl_depth, channels, m_strm);
	LOGV("Load loadImageBytes.");


	imageSize = m_strm->GetSize();
	res_array = env->NewBooleanArray(imageSize);
	LOGV("Load NewByteArray.");
    if (res_array == NULL) {
        return NULL;
    }
    env->SetBooleanArrayRegion(res_array, 0, imageSize, (jboolean*)m_strm->GetByte());
	LOGV("Load SetBooleanArrayRegion.");



	cvReleaseImage( &sourceImage );
	cvReleaseImage( &binaryImage );
	cvReleaseImage( &grayImage );
	m_strm->Close();
	SAFE_DELETE(m_strm);


	return res_array;

}


#if 0

JNIEXPORT
jbooleanArray
JNICALL
Java_org_siprop_opencv_OpenCV_faceDetect(JNIEnv* env,
										jobject thiz,
										jintArray photo_data1,
										jintArray photo_data2,
										jint width,
										jint height) {
	LOGV("Load desp.");

	int i, x, y;
	int* pixels;
	IplImage *frameImage;
	
	IplImage *backgroundImage = cvCreateImage( cvSize(width, height), IPL_DEPTH_8U, 1 );
	IplImage *grayImage = cvCreateImage( cvSize(width, height), IPL_DEPTH_8U, 1 );
	IplImage *differenceImage = cvCreateImage( cvSize(width, height), IPL_DEPTH_8U, 1 );
	
	IplImage *hsvImage = cvCreateImage( cvSize(width, height), IPL_DEPTH_8U, 3 );
	IplImage *hueImage = cvCreateImage( cvSize(width, height), IPL_DEPTH_8U, 1 );
	IplImage *saturationImage = cvCreateImage( cvSize(width, height), IPL_DEPTH_8U, 1 );
	IplImage *valueImage = cvCreateImage( cvSize(width, height), IPL_DEPTH_8U, 1 );
	IplImage *thresholdImage1 = cvCreateImage( cvSize(width, height), IPL_DEPTH_8U, 1 );
	IplImage *thresholdImage2 = cvCreateImage( cvSize(width, height), IPL_DEPTH_8U, 1 );
	IplImage *thresholdImage3 = cvCreateImage( cvSize(width, height), IPL_DEPTH_8U, 1 );
	IplImage *faceImage = cvCreateImage( cvSize(width, height), IPL_DEPTH_8U, 1 );
	
	CvMoments moment;
	double m_00;
	double m_10;
	double m_01;
	int gravityX;
	int gravityY;

	jbooleanArray res_array;
	int imageSize;



	// Load Image
	pixels = env->GetIntArrayElements(photo_data1, NULL);
	frameImage = loadPixels(pixels, width, height);
	if(frameImage == NULL) {
		LOGV("Error loadPixels.");
		return NULL;
	}
	
	
	cvCvtColor( frameImage, backgroundImage, CV_BGR2GRAY );
	
	
	pixels = env->GetIntArrayElements(photo_data2, NULL);
	frameImage = loadPixels(pixels, width, height);
	if(frameImage == NULL) {
		LOGV("Error loadPixels.");
		return NULL;
	}
	cvCvtColor( frameImage, grayImage, CV_BGR2GRAY );
	cvAbsDiff( grayImage, backgroundImage, differenceImage );
	
	cvCvtColor( frameImage, hsvImage, CV_BGR2HSV );
	LOGV("Load cvCvtColor.");
	cvSplit( hsvImage, hueImage, saturationImage, valueImage, NULL );
	LOGV("Load cvSplit.");
	cvThreshold( hueImage, thresholdImage1, THRESH_BOTTOM, THRESHOLD_MAX_VALUE, CV_THRESH_BINARY );
	cvThreshold( hueImage, thresholdImage2, THRESH_TOP, THRESHOLD_MAX_VALUE, CV_THRESH_BINARY_INV );
	cvAnd( thresholdImage1, thresholdImage2, thresholdImage3, NULL );
	LOGV("Load cvAnd.");
	
	cvAnd( differenceImage, thresholdImage3, faceImage, NULL );
	
	cvMoments( faceImage, &moment, 0 );
	m_00 = cvGetSpatialMoment( &moment, 0, 0 );
	m_10 = cvGetSpatialMoment( &moment, 1, 0 );
	m_01 = cvGetSpatialMoment( &moment, 0, 1 );
	gravityX = m_10 / m_00;
	gravityY = m_01 / m_00;
	LOGV("Load cvMoments.");


	cvCircle( frameImage, cvPoint( gravityX, gravityY ), CIRCLE_RADIUS,
		 CV_RGB( 255, 0, 0 ), LINE_THICKNESS, LINE_TYPE, 0 );




	CvMat stub, *mat_image;
    int channels, ipl_depth;
    mat_image = cvGetMat( frameImage, &stub );
    channels = CV_MAT_CN( mat_image->type );

    ipl_depth = cvCvToIplDepth(mat_image->type);

	WLNonFileByteStream* m_strm = new WLNonFileByteStream();
    loadImageBytes(mat_image->data.ptr, mat_image->step, mat_image->width,
                             mat_image->height, ipl_depth, channels, m_strm);
	LOGV("Load loadImageBytes.");


	imageSize = m_strm->GetSize();
	res_array = env->NewBooleanArray(imageSize);
	LOGV("Load NewByteArray.");
    if (res_array == NULL) {
        return NULL;
    }
    env->SetBooleanArrayRegion(res_array, 0, imageSize, (jboolean*)m_strm->GetByte());
	LOGV("Load SetBooleanArrayRegion.");




	cvReleaseImage( &backgroundImage );
	cvReleaseImage( &grayImage );
	cvReleaseImage( &differenceImage );
	cvReleaseImage( &hsvImage );
	cvReleaseImage( &hueImage );
	cvReleaseImage( &saturationImage );
	cvReleaseImage( &valueImage );
	cvReleaseImage( &thresholdImage1 );
	cvReleaseImage( &thresholdImage2 );
	cvReleaseImage( &thresholdImage3 );
	cvReleaseImage( &faceImage );
	cvReleaseImage( &frameImage );
	m_strm->Close();
	SAFE_DELETE(m_strm);

	return res_array;

}
#endif

