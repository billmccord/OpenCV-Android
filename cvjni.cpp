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
#include <time.h>


#define THRESHOLD	10
#define THRESHOLD_MAX_VALUE	255

#define CONTOUR_MAX_LEVEL	1
#define LINE_THICKNESS	2
#define LINE_TYPE	8

#define HAAR_SCALE (1.4)
#define IMAGE_SCALE (2)
#define MIN_NEIGHBORS (2)
#define HAAR_FLAGS (0 | CV_HAAR_FIND_BIGGEST_OBJECT | CV_HAAR_DO_ROUGH_SEARCH)
// Other options we dropped out:
// CV_HAAR_DO_CANNY_PRUNING | CV_HAAR_SCALE_IMAGE
#define MIN_SIZE_WIDTH (20)
#define MIN_SIZE_HEIGHT (20)
#define PAD_FACE (40)
#define PAD_FACE_2 (PAD_FACE * 2)

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
	env->ReleaseIntArrayElements(photo_data, pixels, 0);
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

	int imageSize;
	CvMat stub, *mat_image;
    int channels, ipl_depth;
    mat_image = cvGetMat( sourceImage, &stub );
    channels = CV_MAT_CN( mat_image->type );

    ipl_depth = cvCvToIplDepth(mat_image->type);

	LOGV("Load loadImageBytes.");
	WLNonFileByteStream* m_strm = new WLNonFileByteStream();
    loadImageBytes(mat_image->data.ptr, mat_image->step, mat_image->width,
                             mat_image->height, ipl_depth, channels, m_strm);

	imageSize = m_strm->GetSize();
	jbooleanArray res_array = env->NewBooleanArray(imageSize);
	LOGV("Load NewBooleanArray.");
    if (res_array == NULL) {
        return NULL;
    }
    env->SetBooleanArrayRegion(res_array, 0, imageSize, (jboolean*)m_strm->GetByte());
	LOGV("Load SetBooleanArrayRegion.");

	LOGV("Release sourceImage");
	cvReleaseImage( &sourceImage );
	LOGV("Release binaryImage");
	cvReleaseImage( &binaryImage );
	LOGV("Release grayImage");
	cvReleaseImage( &grayImage );
	LOGV("Release contourImage");
	cvReleaseImage( &contourImage );
	LOGV("Release storage");
	cvReleaseMemStorage( &storage );
	LOGV("Delete m_strm");
	m_strm->Close();
	SAFE_DELETE(m_strm);

	return res_array;
}

JNIEXPORT
void
JNICALL
Java_org_siprop_opencv_OpenCV_initFindFaces(JNIEnv* env,
										    jobject thiz,
										    jstring cascade_path_str) {
	char buffer[100];
	clock_t total_time_start = clock();
	
	if (m_cascade == NULL) {
		LOGV("Loading Haar Classifier Cascade");
		const char *cascade_path_chars = env->GetStringUTFChars(cascade_path_str, NULL);
		if (cascade_path_chars == NULL) {
			LOGV("Error loading cascade XML.");
			return;
		}
		
		m_cascade = (CvHaarClassifierCascade*)cvLoad(cascade_path_chars);
		env->ReleaseStringUTFChars(cascade_path_str, cascade_path_chars);
		if (m_cascade == NULL) {
			LOGV("Error loading cascade.");
			return;
		}
	}
	
	m_storage = cvCreateMemStorage(0);
	
	m_grayImage = NULL;
	m_smallImage = NULL;
	
	clock_t total_time_finish = clock() - total_time_start;
	sprintf(buffer, "Total Time to init: %f", (double)total_time_finish / (double)CLOCKS_PER_SEC);
	LOGV(buffer);
}

JNIEXPORT
void
JNICALL
Java_org_siprop_opencv_OpenCV_releaseFindFaces(JNIEnv* env,
										       jobject thiz) {
	if (m_cascade != NULL) {
		LOGV("Release cascade");
		cvReleaseHaarClassifierCascade(&m_cascade);
		m_cascade = NULL;
	}
	
	if (m_grayImage != NULL) {
		LOGV("Release grayImage");
		cvReleaseImage(&m_grayImage);
		m_grayImage = NULL;
	}
	
	if (m_smallImage != NULL) {
		LOGV("Release smallImage");
		cvReleaseImage(&m_smallImage);
		m_smallImage = NULL;
	}
	
	if (m_storage != NULL) {
		LOGV("Release storage");
		cvReleaseMemStorage( &m_storage );
		m_storage = NULL;
	}
}

// Given an integer array of image data, load an IplImage.
// It is the responsibility of the caller to release the IplImage.
IplImage* getIplImageFromIntArray(JNIEnv* env, jintArray array_data, 
								  jint width, jint height) {
	// Load Image
	int *pixels = env->GetIntArrayElements(array_data, NULL);
	if (pixels == NULL) {
		LOGV("Error getting int array of pixels.");
		return NULL;
	}
	
	IplImage *image = loadPixels(pixels, width, height);
	env->ReleaseIntArrayElements(array_data, pixels, 0);
	if(image == NULL) {
		LOGV("Error loadPixels.");
		return NULL;
	}
	
	return image;
}

// Initalize the small image and the gray image using the input source image.
// If a previous face was specified, we will limit the ROI to that face.
void initImages(IplImage *sourceImage, double scale = 1.0) {
	if (m_grayImage == NULL) {
		m_grayImage = cvCreateImage(cvGetSize(sourceImage), IPL_DEPTH_8U, 1);
	}
	
	if (m_smallImage == NULL) {
		m_smallImage = cvCreateImage(cvSize(cvRound(sourceImage->width / scale), 
			cvRound(sourceImage->height / scale)), IPL_DEPTH_8U, 1);
	}
	
	if(m_prevFace.width > 0 && m_prevFace.height > 0) {
		cvSetImageROI(m_smallImage, m_prevFace);
	
		CvRect tPrev = cvRect(m_prevFace.x * scale, m_prevFace.y * scale, 
			m_prevFace.width * scale, m_prevFace.height * scale);
		cvSetImageROI(sourceImage, tPrev);
		cvSetImageROI(m_grayImage, tPrev);
	} else {
		cvResetImageROI(m_smallImage);
		cvResetImageROI(m_grayImage);
	}
	
    cvCvtColor(sourceImage, m_grayImage, CV_BGR2GRAY);
    cvResize(m_grayImage, m_smallImage, CV_INTER_LINEAR);
    cvEqualizeHist(m_smallImage, m_smallImage);
	cvClearMemStorage(m_storage);
	
	cvResetImageROI(sourceImage);
}

// Draw a rectangle on the source image around the specified face rectangle.
// Scale the face area to the draw area based on the specified scale.
void highlightFace(IplImage *sourceImage, CvRect *face, double scale = 1.0) {
	char buffer[100];
	sprintf(buffer, "Face Rectangle: (x: %d, y: %d) to (w: %d, h: %d)", 
		face->x, face->y, face->width, face->height);
	LOGV(buffer);
	CvPoint pt1 = cvPoint(int(face->x * scale), int(face->y * scale));
	CvPoint pt2 = cvPoint(int((face->x + face->width) * scale), 
		int((face->y + face->height) * scale));
	sprintf(buffer, "Draw Rectangle: (%d, %d) to (%d, %d)", pt1.x, pt1.y, pt2.x, pt2.y);
	LOGV(buffer);
    cvRectangle(sourceImage, pt1, pt2, CV_RGB(255, 0, 0), 3, 8, 0);
}

// Draw rectangles on the source image around each face that was found.
// Scale the face area to the draw area based on the specified scale.
// Return true if at least one face was highlighted and false otherwise.
bool highlightFaces(IplImage *sourceImage, CvSeq *faces, double scale = 1.0) {
	if (faces == NULL || faces->total <= 0) {
		LOGV("No faces were highlighted!");
		return false;
	} else {
		LOGV("Drawing rectangles on each face");
		int count;
		CvRect* face;
		for (int i = 0; i < faces->total; i++) {
			face = (CvRect*)cvGetSeqElem(faces, i);
			highlightFace(sourceImage, face, scale);
		}
	}
	
	return true;
}

// Store the previous face found in the scene.
void storePreviousFace(CvRect* face) {
	char buffer[100];
	if (m_prevFace.width > 0 && m_prevFace.height > 0) {
		face->x += m_prevFace.x;
		face->y += m_prevFace.y;
		sprintf(buffer, "Face rect + m_prevFace: (%d, %d) to (%d, %d)", face->x, face->y, 
			face->x + face->width, face->y + face->height);
		LOGV(buffer);
	}
	
	int startX = MAX(face->x - PAD_FACE, 0);
	int startY = MAX(face->y - PAD_FACE, 0);
	int w = m_smallImage->width - startX - face->width - PAD_FACE_2;
	int h = m_smallImage->height - startY - face->height - PAD_FACE_2;
	int sw = face->x - PAD_FACE, sh = face->y - PAD_FACE;
	m_prevFace = cvRect(startX, startY, 
		face->width + PAD_FACE_2 + ((w < 0) ? w : 0) + ((sw < 0) ? sw : 0),
		face->height + PAD_FACE_2 + ((h < 0) ? h : 0) + ((sh < 0) ? sh : 0));
	sprintf(buffer, "m_prevFace: (%d, %d) to (%d, %d)", m_prevFace.x, m_prevFace.y, 
		m_prevFace.x + m_prevFace.width, m_prevFace.y + m_prevFace.height);
	LOGV(buffer);
}

// Given an image, generate and return a boolean array.
jbooleanArray imageToBooleanArray(JNIEnv* env, IplImage *sourceImage) {
	CvMat stub;
    CvMat *mat_image = cvGetMat(sourceImage, &stub);
    int channels = CV_MAT_CN( mat_image->type );
    int ipl_depth = cvCvToIplDepth(mat_image->type);

	LOGV("Load loadImageBytes.");
	WLNonFileByteStream *strm = new WLNonFileByteStream();
    loadImageBytes(mat_image->data.ptr, mat_image->step, mat_image->width,
		mat_image->height, ipl_depth, channels, strm);
	
	int imageSize = strm->GetSize();
	jbooleanArray res_array = env->NewBooleanArray(imageSize);
	LOGV("Load NewBooleanArray.");
    if (res_array == NULL) {
        return NULL;
    }
    env->SetBooleanArrayRegion(res_array, 0, imageSize, (jboolean*)strm->GetByte());
	LOGV("Load SetBooleanArrayRegion.");
	
	LOGV("Delete m_strm");
	strm->Close();
	SAFE_DELETE(strm);
	
	return res_array;
}

JNIEXPORT
jbooleanArray
JNICALL
Java_org_siprop_opencv_OpenCV_findFaces(JNIEnv* env,
									    jobject thiz,
										jintArray photo_data,
										jint width,
										jint height) {
	char buffer[100];
	clock_t total_time_start = clock();
	
	if (m_cascade == NULL || m_storage == NULL) {
		LOGV("Error find faces was not initialized.");
		return NULL;
	}
	
	IplImage *sourceImage = getIplImageFromIntArray(env, photo_data, width, height);
	if (sourceImage == NULL) {
		return NULL;
	}
	
	clock_t total_critical_time_start = clock();
	initImages(sourceImage, IMAGE_SCALE);

	LOGV("Haar Detect Objects");
	clock_t haar_detect_time_start = clock();
    CvSeq *faces = mycvHaarDetectObjects(m_smallImage, m_cascade, m_storage, HAAR_SCALE, 
		MIN_NEIGHBORS, HAAR_FLAGS, cvSize(MIN_SIZE_WIDTH, MIN_SIZE_HEIGHT));
		
	clock_t haar_detect_time_finish = clock() - haar_detect_time_start;
	sprintf(buffer, "Total Time to cvHaarDetectObjects: %f", (double)haar_detect_time_finish / (double)CLOCKS_PER_SEC);
	LOGV(buffer);
	
	if (faces == NULL || faces->total <= 0) {
		LOGV("FACES_DETECTED 0");
		LOGV("No faces were detected!");
		m_prevFace.width = m_prevFace.height = 0;
	} else if (faces->total == 1) {
		LOGV("FACES_DETECTED 1");
		CvRect* face = (CvRect*)cvGetSeqElem(faces, 0);
		sprintf(buffer, "Inital face rect: (%d, %d) to (%d, %d)", face->x, face->y, 
			face->x + face->width, face->y + face->height);
		LOGV(buffer);
		storePreviousFace(face);
		highlightFace(sourceImage, face, IMAGE_SCALE);
	} else {
		LOGV("FACES_DETECTED MANY");
		highlightFaces(sourceImage, faces, IMAGE_SCALE);
	}
	
	clock_t total_critical_time_finish = clock() - total_critical_time_start;
	sprintf(buffer, "Total Critical Time to findFaces: %f", (double)total_critical_time_finish / (double)CLOCKS_PER_SEC);
	LOGV(buffer);
	
	jbooleanArray res_array = imageToBooleanArray(env, sourceImage);
	
	LOGV("Release sourceImage");
	cvReleaseImage(&sourceImage);
	
	clock_t total_time_finish = clock() - total_time_start;
	sprintf(buffer, "Total Time to findFaces: %f", (double)total_time_finish / (double)CLOCKS_PER_SEC);
	LOGV(buffer);
	
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

