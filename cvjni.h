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
#include <stdlib.h>
#include <string.h>
#include <jni.h>
#include <android/log.h>

#include "cv.h"
#include "cxcore.h"
#include "cvaux.h"
#include "highgui.h"
#include "ml.h"
#include "utils.h"
#include "WLNonFileByteStream.h"
#include "grfmt_bmp.h"

#define LOGV(...) __android_log_print(ANDROID_LOG_SILENT, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
//ANDROID_LOG_UNKNOWN, ANDROID_LOG_DEFAULT, ANDROID_LOG_VERBOSE, ANDROID_LOG_DEBUG, ANDROID_LOG_INFO, ANDROID_LOG_WARN, ANDROID_LOG_ERROR, ANDROID_LOG_FATAL, ANDROID_LOG_SILENT
//LOGV(ANDROID_LOG_DEBUG, "JNI", "");

#define ANDROID_LOG_VERBOSE ANDROID_LOG_DEBUG
#define LOG_TAG "CVJNI"
#define INVALID_ARGUMENT -18456

#define		SAFE_DELETE(p)			{ if(p){ delete (p); (p)=0; } }
#define		SAFE_DELETE_ARRAY(p)	{ if(p){ delete [](p); (p)=0; } }


#define IMAGE( i, x, y, n )   *(( unsigned char * )(( i )->imageData      \
                                    + ( x ) * sizeof( unsigned char ) * 3 \
                                    + ( y ) * ( i )->widthStep ) + ( n ))

// CV Objects
static const char* fmtSignBmp = "BM";

CvCapture *m_capture = 0;
CvHaarClassifierCascade *m_cascade = 0;
IplImage *m_sourceImage = 0;
IplImage *m_grayImage = 0;
IplImage *m_smallImage = 0;
CvMemStorage *m_storage = 0;
CvSeq *m_facesFound = 0;
CvRect m_faceCropArea;
CvSize m_smallestFaceSize;


#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT
jboolean
JNICALL
Java_org_siprop_opencv_OpenCV_createSocketCapture(JNIEnv* env,
												  jobject thiz,
												  jstring address_str,
												  jstring port_str,
												  jint width,
												  jint height);

JNIEXPORT
void
JNICALL
Java_org_siprop_opencv_OpenCV_releaseSocketCapture(JNIEnv* env,
												   jobject thiz);

JNIEXPORT
jboolean
JNICALL
Java_org_siprop_opencv_OpenCV_grabSourceImageFromCapture(JNIEnv* env,
														 jobject thiz);
														
JNIEXPORT
jbooleanArray
JNICALL
Java_org_siprop_opencv_OpenCV_getSourceImage(JNIEnv* env,
											 jobject thiz);

JNIEXPORT
jboolean
JNICALL
Java_org_siprop_opencv_OpenCV_setSourceImage(JNIEnv* env,
									    	 jobject thiz,
											 jintArray photo_data,
											 jint width,
											 jint height);

JNIEXPORT
jbooleanArray
JNICALL
Java_org_siprop_opencv_OpenCV_findContours(JNIEnv* env,
										jobject thiz,
										jint width,
										jint height);

JNIEXPORT
jboolean
JNICALL
Java_org_siprop_opencv_OpenCV_initFaceDetection(JNIEnv* env,
												jobject thiz,
												jstring cascade_path_str);
												
JNIEXPORT
void
JNICALL
Java_org_siprop_opencv_OpenCV_releaseFaceDetection(JNIEnv* env,
									       	  	   jobject thiz);

JNIEXPORT
jboolean
JNICALL
Java_org_siprop_opencv_OpenCV_highlightFaces(JNIEnv* env,
											 jobject thiz);

JNIEXPORT
jobjectArray
JNICALL
Java_org_siprop_opencv_OpenCV_findAllFaces(JNIEnv* env,
									       jobject thiz);
									
JNIEXPORT
jobject
JNICALL
Java_org_siprop_opencv_OpenCV_findSingleFace(JNIEnv* env,
											 jobject thiz);

#ifdef __cplusplus
}
#endif



IplImage* loadPixels(int* pixels, int width, int height) {

	int x, y;
	IplImage *img = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 3);

	for ( y = 0; y < height; y++ ) {
        for ( x = 0; x < width; x++ ) {
            // blue
            IMAGE( img, x, y, 0 ) = pixels[x+y*width] & 0xFF;
            // green
            IMAGE( img, x, y, 1 ) = pixels[x+y*width] >> 8 & 0xFF;
            // red
            IMAGE( img, x, y, 2 ) = pixels[x+y*width] >> 16 & 0xFF;
        }
    }

	return img;
}


void loadImageBytes(const uchar* data, 
                    int step,
                    int width, 
                    int height, 
                    int depth, 
                    int channels, 
                    WLNonFileByteStream* m_strm) {

    int fileStep = (width*channels + 3) & -4;
    uchar zeropad[] = "\0\0\0\0";
    char log_str[100];


    assert( data && width > 0 && height > 0 && step >= fileStep );

    int  bitmapHeaderSize = 40;
    int  paletteSize = channels > 1 ? 0 : 1024;
    int  headerSize = 14 /* fileheader */ + bitmapHeaderSize + paletteSize;
    PaletteEntry palette[256];
    
    int testSize = fileStep*height + headerSize;
    m_strm->Open(testSize);
	sprintf(log_str, "fileStep*height + headerSize=%i", testSize);
	LOGV(log_str);
    
    // write signature 'BM'
    m_strm->PutBytes( fmtSignBmp, (int)strlen(fmtSignBmp) );

    // write file header
    m_strm->PutDWord( fileStep*height + headerSize ); // file size
    m_strm->PutDWord( 0 );
    m_strm->PutDWord( headerSize );

    // write bitmap header
    m_strm->PutDWord( bitmapHeaderSize );
    m_strm->PutDWord( width );
    m_strm->PutDWord( height );
    m_strm->PutWord( 1 );
    m_strm->PutWord( channels << 3 );
    m_strm->PutDWord( BMP_RGB );
    m_strm->PutDWord( 0 );
    m_strm->PutDWord( 0 );
    m_strm->PutDWord( 0 );
    m_strm->PutDWord( 0 );
    m_strm->PutDWord( 0 );

    if( channels == 1 )
    {
        FillGrayPalette( palette, 8 );
        m_strm->PutBytes( palette, sizeof(palette));
    }

    width *= channels;
    data += step*(height - 1);
    for( ; height--; data -= step )
    {
        m_strm->PutBytes( data, width );
        if( fileStep > width )
            m_strm->PutBytes( zeropad, fileStep - width );
    }
}




bool is_NULL_field_JavaObj(JNIEnv* env, jobject java_obj, const char* field_name, const char* field_type) {


	LOGV("in is_NULL_field_JavaObj!");
	jclass clazz = env->GetObjectClass(java_obj);


	// get field
	jfieldID fid = env->GetFieldID(clazz, field_name, field_type);

	jobject obj = env->GetObjectField(java_obj, fid);
	if(obj == 0) {
		LOGV("Object is NULL!");
		return true;
	}
	return false;
}

bool is_NULL_vec_field_JavaObj(JNIEnv* env, jobject java_obj, const char* field_name) {
	return is_NULL_field_JavaObj(env, java_obj, field_name, "Lorg/siprop/opencv/util/Vector3;");
}

bool is_NULL_point_field_JavaObj(JNIEnv* env, jobject java_obj, const char* field_name) {
	return is_NULL_field_JavaObj(env, java_obj, field_name, "Lorg/siprop/opencv/util/Point3;");
}

bool is_NULL_axis_field_JavaObj(JNIEnv* env, jobject java_obj, const char* field_name) {
	return is_NULL_field_JavaObj(env, java_obj, field_name, "Lorg/siprop/opencv/util/Axis;");
}

bool is_NULL_pivot_field_JavaObj(JNIEnv* env, jobject java_obj, const char* field_name) {
	return is_NULL_field_JavaObj(env, java_obj, field_name, "Lorg/siprop/opencv/util/Pivot3;");
}

bool is_NULL_quat_field_JavaObj(JNIEnv* env, jobject java_obj, const char* field_name) {
	return is_NULL_field_JavaObj(env, java_obj, field_name, "Lorg/siprop/opencv/util/Quaternion;");
}

bool is_NULL_mat3x3_field_JavaObj(JNIEnv* env, jobject java_obj, const char* field_name) {
	return is_NULL_field_JavaObj(env, java_obj, field_name, "Lorg/siprop/opencv/util/Matrix3x3;");
}
bool is_NULL_mat3x1_field_JavaObj(JNIEnv* env, jobject java_obj, const char* field_name) {
	return is_NULL_field_JavaObj(env, java_obj, field_name, "Lorg/siprop/opencv/util/Matrix3x1;");
}




void set_JavaObj_int(JNIEnv* env, jobject java_obj, const char* field_name, jint val) {

	LOGV("in set_JavaObj_int!");

	jclass clazz = env->GetObjectClass(java_obj);
	jfieldID fid = env->GetFieldID(clazz, field_name, "I");

	env->SetIntField(java_obj, fid, val);

}

int get_id_by_JavaObj(JNIEnv* env, jobject java_obj) {

	LOGV("in get_id_by_JavaObj!");

	jclass method_clazz = env->GetObjectClass(java_obj);
	jmethodID get_type_mid = env->GetMethodID(method_clazz, "getID", "()I");
	return env->CallIntMethod(java_obj, get_type_mid);

}

int get_type_by_JavaObj(JNIEnv* env, jobject java_obj) {

	LOGV("in get_type_by_JavaObj!");

	jclass method_clazz = env->GetObjectClass(java_obj);
	jmethodID get_type_mid = env->GetMethodID(method_clazz, "getType", "()I");
	return env->CallIntMethod(java_obj, get_type_mid);

}


int get_int_by_JavaObj(JNIEnv* env, jobject java_obj, const char* field_name) {
	
	LOGV("in get_int_by_JavaObj!");

	jclass clazz = env->GetObjectClass(java_obj);
	jfieldID int_fid = env->GetFieldID(clazz, field_name, "I");
	return env->GetIntField(java_obj, int_fid);

}


float get_float_by_JavaObj(JNIEnv* env, jobject java_obj, const char* field_name) {
	
	LOGV("in get_float_by_JavaObj!");

	jclass clazz = env->GetObjectClass(java_obj);
	jfieldID float_fid = env->GetFieldID(clazz, field_name, "F");
	return env->GetFloatField(java_obj, float_fid);

}


jobject get_obj_by_JavaObj(JNIEnv* env, jobject java_obj, const char* field_name, const char* obj_type) {
	
	LOGV("in get_obj_by_JavaObj!");

	jclass clazz = env->GetObjectClass(java_obj);
	jfieldID obj_fid = env->GetFieldID(clazz, field_name, obj_type);
	return env->GetObjectField(java_obj, obj_fid);

}

