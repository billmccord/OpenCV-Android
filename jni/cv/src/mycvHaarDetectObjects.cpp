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

/* Haar features calculation */

#include "_cv.h"
#include <stdio.h>

/* these settings affect the quality of detection: change with care */
#define CV_ADJUST_FEATURES 1
#define CV_ADJUST_WEIGHTS  1

typedef int sumtype;
typedef double sqsumtype;

typedef struct MyCvHidHaarFeature
	{
		struct
		{
			sumtype *p0, *p1, *p2, *p3;
			int weight;
		}
		rect[CV_HAAR_FEATURE_MAX];
	}
	MyCvHidHaarFeature;


typedef struct MyCvHidHaarTreeNode
	{
		MyCvHidHaarFeature feature;
		int threshold;
		int left;
		int right;
	}
	MyCvHidHaarTreeNode;


typedef struct MyCvHidHaarClassifier
	{
		int count;
		//CvHaarFeature* orig_feature;
		MyCvHidHaarTreeNode* node;
		float* alpha;
	}
	MyCvHidHaarClassifier;


typedef struct MyCvHidHaarStageClassifier
	{
		int  count;
		float threshold;
		MyCvHidHaarClassifier* classifier;
		int two_rects;
		
		struct MyCvHidHaarStageClassifier* next;
		struct MyCvHidHaarStageClassifier* child;
		struct MyCvHidHaarStageClassifier* parent;
	}
	MyCvHidHaarStageClassifier;


struct MyCvHidHaarClassifierCascade
{
    int  count;
    int  is_stump_based;
    int  has_tilted_features;
    int  is_tree;
    double inv_window_area;
    CvMat sum, sqsum, tilted;
    MyCvHidHaarStageClassifier* stage_classifier;
    sqsumtype *pq0, *pq1, *pq2, *pq3;
    sumtype *p0, *p1, *p2, *p3;
	
    void** ipp_stages;
};


const int icv_object_win_border = 1;
const float icv_stage_threshold_bias = 0.0001f;

static int myis_equal( const void* _r1, const void* _r2, void* )
{
    const CvRect* r1 = (const CvRect*)_r1;
    const CvRect* r2 = (const CvRect*)_r2;
    int distance = cvRound(r1->width*0.2);
	
    return r2->x <= r1->x + distance &&
	r2->x >= r1->x - distance &&
	r2->y <= r1->y + distance &&
	r2->y >= r1->y - distance &&
	r2->width <= cvRound( r1->width * 1.2 ) &&
	cvRound( r2->width * 1.2 ) >= r1->width;
}

static void
myicvReleaseHidHaarClassifierCascade( MyCvHidHaarClassifierCascade** _cascade )
{
    if( _cascade && *_cascade )
    {
        /*CvHidHaarClassifierCascade* cascade = *_cascade;
		 if( cascade->ipp_stages && icvHaarClassifierFree_32f_p )
		 {
		 int i;
		 for( i = 0; i < cascade->count; i++ )
		 {
		 if( cascade->ipp_stages[i] )
		 icvHaarClassifierFree_32f_p( cascade->ipp_stages[i] );
		 }
		 }
		 cvFree( &cascade->ipp_stages );*/
        cvFree( _cascade );
    }
}

/* create more efficient internal representation of haar classifier cascade */
static MyCvHidHaarClassifierCascade*
myicvCreateHidHaarClassifierCascade( CvHaarClassifierCascade* cascade )
{
    CvRect* ipp_features = 0;
    float *ipp_weights = 0, *ipp_thresholds = 0, *ipp_val1 = 0, *ipp_val2 = 0;
    int* ipp_counts = 0;
	
    MyCvHidHaarClassifierCascade* out = 0;
	
    CV_FUNCNAME( "icvCreateHidHaarClassifierCascade" );
	
    __BEGIN__;
	
    int i, j, k, l;
    int datasize;
    int total_classifiers = 0;
    int total_nodes = 0;
    char errorstr[100];
    MyCvHidHaarClassifier* haar_classifier_ptr;
    MyCvHidHaarTreeNode* haar_node_ptr;
    CvSize orig_window_size;
    int has_tilted_features = 0;
    int max_count = 0;
	
    if( !CV_IS_HAAR_CLASSIFIER(cascade) )
        CV_ERROR( !cascade ? CV_StsNullPtr : CV_StsBadArg, "Invalid classifier pointer" );
	
    if( cascade->hid_cascade )
        CV_ERROR( CV_StsError, "hid_cascade has been already created" );
	
    if( !cascade->stage_classifier )
        CV_ERROR( CV_StsNullPtr, "" );
	
    if( cascade->count <= 0 )
        CV_ERROR( CV_StsOutOfRange, "Negative number of cascade stages" );
	
    orig_window_size = cascade->orig_window_size;
	
    /* check input structure correctness and calculate total memory size needed for
	 internal representation of the classifier cascade */
    for( i = 0; i < cascade->count; i++ )
    {
        CvHaarStageClassifier* stage_classifier = cascade->stage_classifier + i;
		
        if( !stage_classifier->classifier ||
		   stage_classifier->count <= 0 )
        {
            sprintf( errorstr, "header of the stage classifier #%d is invalid "
					"(has null pointers or non-positive classfier count)", i );
            CV_ERROR( CV_StsError, errorstr );
        }
		
        max_count = MAX( max_count, stage_classifier->count );
        total_classifiers += stage_classifier->count;
		
        for( j = 0; j < stage_classifier->count; j++ )
        {
            CvHaarClassifier* classifier = stage_classifier->classifier + j;
			
            total_nodes += classifier->count;
            for( l = 0; l < classifier->count; l++ )
            {
                for( k = 0; k < CV_HAAR_FEATURE_MAX; k++ )
                {
                    if( classifier->haar_feature[l].rect[k].r.width )
                    {
                        CvRect r = classifier->haar_feature[l].rect[k].r;
                        int tilted = classifier->haar_feature[l].tilted;
                        has_tilted_features |= tilted != 0;
                        if( r.width < 0 || r.height < 0 || r.y < 0 ||
						   r.x + r.width > orig_window_size.width
						   ||
						   (!tilted &&
                            (r.x < 0 || r.y + r.height > orig_window_size.height))
						   ||
						   (tilted && (r.x - r.height < 0 ||
									   r.y + r.width + r.height > orig_window_size.height)))
                        {
                            sprintf( errorstr, "rectangle #%d of the classifier #%d of "
									"the stage classifier #%d is not inside "
									"the reference (original) cascade window", k, j, i );
                            CV_ERROR( CV_StsNullPtr, errorstr );
                        }
                    }
                }
            }
        }
    }
	
    // this is an upper boundary for the whole hidden cascade size
    datasize = sizeof(MyCvHidHaarClassifierCascade) +
	sizeof(MyCvHidHaarStageClassifier)*cascade->count +
	sizeof(MyCvHidHaarClassifier) * total_classifiers +
	sizeof(MyCvHidHaarTreeNode) * total_nodes +
	sizeof(void*)*(total_nodes + total_classifiers);
	
    CV_CALL( out = (MyCvHidHaarClassifierCascade*)cvAlloc( datasize ));
    memset( out, 0, sizeof(*out) );
	
    /* init header */
    out->count = cascade->count;
    out->stage_classifier = (MyCvHidHaarStageClassifier*)(out + 1);
    haar_classifier_ptr = (MyCvHidHaarClassifier*)(out->stage_classifier + cascade->count);
    haar_node_ptr = (MyCvHidHaarTreeNode*)(haar_classifier_ptr + total_classifiers);
	
    out->is_stump_based = 1;
    out->has_tilted_features = has_tilted_features;
    out->is_tree = 0;
	
    /* initialize internal representation */
    for( i = 0; i < cascade->count; i++ )
    {
        CvHaarStageClassifier* stage_classifier = cascade->stage_classifier + i;
        MyCvHidHaarStageClassifier* hid_stage_classifier = out->stage_classifier + i;
		
        hid_stage_classifier->count = stage_classifier->count;
        hid_stage_classifier->threshold = stage_classifier->threshold - icv_stage_threshold_bias;
        hid_stage_classifier->classifier = haar_classifier_ptr;
        hid_stage_classifier->two_rects = 1;
        haar_classifier_ptr += stage_classifier->count;
		
        hid_stage_classifier->parent = (stage_classifier->parent == -1)
		? NULL : out->stage_classifier + stage_classifier->parent;
        hid_stage_classifier->next = (stage_classifier->next == -1)
		? NULL : out->stage_classifier + stage_classifier->next;
        hid_stage_classifier->child = (stage_classifier->child == -1)
		? NULL : out->stage_classifier + stage_classifier->child;
		
        out->is_tree |= hid_stage_classifier->next != NULL;
		
        for( j = 0; j < stage_classifier->count; j++ )
        {
            CvHaarClassifier* classifier = stage_classifier->classifier + j;
            MyCvHidHaarClassifier* hid_classifier = hid_stage_classifier->classifier + j;
            int node_count = classifier->count;
            float* alpha_ptr = (float*)(haar_node_ptr + node_count);
			
            hid_classifier->count = node_count;
            hid_classifier->node = haar_node_ptr;
            hid_classifier->alpha = alpha_ptr;
			
            for( l = 0; l < node_count; l++ )
            {
                MyCvHidHaarTreeNode* node = hid_classifier->node + l;
                CvHaarFeature* feature = classifier->haar_feature + l;
                memset( node, -1, sizeof(*node) );
                node->threshold = (int)((classifier->threshold[l]) * 65536.0);
                node->left = classifier->left[l];
                node->right = classifier->right[l];
				
                if( fabs(feature->rect[2].weight) < DBL_EPSILON ||
				   feature->rect[2].r.width == 0 ||
				   feature->rect[2].r.height == 0 )
                    memset( &(node->feature.rect[2]), 0, sizeof(node->feature.rect[2]) );
                else
                    hid_stage_classifier->two_rects = 0;
            }
			
            memcpy( alpha_ptr, classifier->alpha, (node_count+1)*sizeof(alpha_ptr[0]));
            haar_node_ptr =
			(MyCvHidHaarTreeNode*)cvAlignPtr(alpha_ptr+node_count+1, sizeof(void*));
			
            out->is_stump_based &= node_count == 1;
        }
    }
	
    /*{
	 int can_use_ipp = icvHaarClassifierInitAlloc_32f_p != 0 &&
	 icvHaarClassifierFree_32f_p != 0 &&
	 icvApplyHaarClassifier_32f_C1R_p != 0 &&
	 icvRectStdDev_32f_C1R_p != 0 &&
	 !out->has_tilted_features && !out->is_tree && out->is_stump_based;
	 
	 if( can_use_ipp )
	 {
	 int ipp_datasize = cascade->count*sizeof(out->ipp_stages[0]);
	 float ipp_weight_scale=(float)(1./((orig_window_size.width-icv_object_win_border*2)*
	 (orig_window_size.height-icv_object_win_border*2)));
	 
	 CV_CALL( out->ipp_stages = (void**)cvAlloc( ipp_datasize ));
	 memset( out->ipp_stages, 0, ipp_datasize );
	 
	 CV_CALL( ipp_features = (CvRect*)cvAlloc( max_count*3*sizeof(ipp_features[0]) ));
	 CV_CALL( ipp_weights = (float*)cvAlloc( max_count*3*sizeof(ipp_weights[0]) ));
	 CV_CALL( ipp_thresholds = (float*)cvAlloc( max_count*sizeof(ipp_thresholds[0]) ));
	 CV_CALL( ipp_val1 = (float*)cvAlloc( max_count*sizeof(ipp_val1[0]) ));
	 CV_CALL( ipp_val2 = (float*)cvAlloc( max_count*sizeof(ipp_val2[0]) ));
	 CV_CALL( ipp_counts = (int*)cvAlloc( max_count*sizeof(ipp_counts[0]) ));
	 
	 for( i = 0; i < cascade->count; i++ )
	 {
	 CvHaarStageClassifier* stage_classifier = cascade->stage_classifier + i;
	 for( j = 0, k = 0; j < stage_classifier->count; j++ )
	 {
	 CvHaarClassifier* classifier = stage_classifier->classifier + j;
	 int rect_count = 2 + (classifier->haar_feature->rect[2].r.width != 0);
	 
	 ipp_thresholds[j] = classifier->threshold[0];
	 ipp_val1[j] = classifier->alpha[0];
	 ipp_val2[j] = classifier->alpha[1];
	 ipp_counts[j] = rect_count;
	 
	 for( l = 0; l < rect_count; l++, k++ )
	 {
	 ipp_features[k] = classifier->haar_feature->rect[l].r;
	 //ipp_features[k].y = orig_window_size.height - ipp_features[k].y - ipp_features[k].height;
	 ipp_weights[k] = classifier->haar_feature->rect[l].weight*ipp_weight_scale;
	 }
	 }
	 
	 if( icvHaarClassifierInitAlloc_32f_p( &out->ipp_stages[i],
	 ipp_features, ipp_weights, ipp_thresholds,
	 ipp_val1, ipp_val2, ipp_counts, stage_classifier->count ) < 0 )
	 break;
	 }
	 
	 if( i < cascade->count )
	 {
	 for( j = 0; j < i; j++ )
	 if( icvHaarClassifierFree_32f_p && out->ipp_stages[i] )
	 icvHaarClassifierFree_32f_p( out->ipp_stages[i] );
	 cvFree( &out->ipp_stages );
	 }
	 }
	 }*/
	
    cascade->hid_cascade = (CvHidHaarClassifierCascade*)out;
    assert( (char*)haar_node_ptr - (char*)out <= datasize );
	
    __END__;
	
    if( cvGetErrStatus() < 0 )
        myicvReleaseHidHaarClassifierCascade( &out );
	
    cvFree( &ipp_features );
    cvFree( &ipp_weights );
    cvFree( &ipp_thresholds );
    cvFree( &ipp_val1 );
    cvFree( &ipp_val2 );
    cvFree( &ipp_counts );
	
    return out;
}

#define calc_sum(rect,offset) \
((rect).p0[offset] - (rect).p1[offset] - (rect).p2[offset] + (rect).p3[offset])


CV_INLINE
double myicvEvalHidHaarClassifier( MyCvHidHaarClassifier* classifier,
								double variance_norm_factor,
								size_t p_offset )
{
    int idx = 0;
    do
    {
        MyCvHidHaarTreeNode* node = classifier->node + idx;
        double t = node->threshold * variance_norm_factor;
		
        double sum = calc_sum(node->feature.rect[0],p_offset) * node->feature.rect[0].weight;
        sum += calc_sum(node->feature.rect[1],p_offset) * node->feature.rect[1].weight;
		
        if( node->feature.rect[2].p0 )
            sum += calc_sum(node->feature.rect[2],p_offset) * node->feature.rect[2].weight;
		
        idx = sum < t ? node->left : node->right;
    }
    while( idx > 0 );
    return classifier->alpha[-idx];
}

/*********************** Special integer sqrt **************************/

int
isqrt(int x)
{
	/*
	 *	Logically, these are unsigned. We need the sign bit to test
	 *	whether (op - res - one) underflowed.
	 */
	
	register int op, res, one;
	
	op = x;
	res = 0;
	
	/* "one" starts at the highest power of four <= than the argument. */
	
	one = 1 << 30;	/* second-to-top bit set */
	while (one > op) one >>= 2;
		
		while (one != 0) {
			if (op >= res + one) {
				op = op - (res + one);
				res = res +  2 * one;
			}
			res /= 2;
			one /= 4;
		}
	return(res);
}

#define NEXT(n, i)  (((n) + (i)/(n)) >> 1)

unsigned int isqrt1(int number) {
	unsigned int n  = 1;
	unsigned int n1 = NEXT(n, number);
	
	while(abs(n1 - n) > 1) {
		n  = n1;
		n1 = NEXT(n, number);
	}
	while((n1*n1) > number) {
		n1 -= 1;
	}
	return n1;
}
/***********************************************************************/

CV_IMPL int
mycvRunHaarClassifierCascade( CvHaarClassifierCascade* _cascade,
						   CvPoint pt, int start_stage )
{
    int result = -1;
    CV_FUNCNAME("mycvRunHaarClassifierCascade");
	
    __BEGIN__;
	
    int p_offset, pq_offset;
	int pq0, pq1, pq2, pq3;
    int i, j;
    double mean;
	int variance_norm_factor;
    MyCvHidHaarClassifierCascade* cascade;
	
    if( !CV_IS_HAAR_CLASSIFIER(_cascade) )
        CV_ERROR( !_cascade ? CV_StsNullPtr : CV_StsBadArg, "Invalid cascade pointer" );
	
    cascade = (MyCvHidHaarClassifierCascade*)_cascade->hid_cascade;
    if( !cascade )
        CV_ERROR( CV_StsNullPtr, "Hidden cascade has not been created.\n"
				 "Use cvSetImagesForHaarClassifierCascade" );
	
    if( pt.x < 0 || pt.y < 0 ||
	   pt.x + _cascade->real_window_size.width >= cascade->sum.width-2 ||
	   pt.y + _cascade->real_window_size.height >= cascade->sum.height-2 )
        EXIT;
	
    p_offset = pt.y * (cascade->sum.step/sizeof(sumtype)) + pt.x;
    pq_offset = pt.y * (cascade->sqsum.step/sizeof(sqsumtype)) + pt.x;
    mean = calc_sum(*cascade,p_offset) * cascade->inv_window_area;
	pq0 = cascade->pq0[pq_offset];
	pq1 = cascade->pq1[pq_offset];
	pq2 = cascade->pq2[pq_offset];
	pq3 = cascade->pq3[pq_offset];
    variance_norm_factor = pq0 - pq1 - pq2 + pq3;
    variance_norm_factor = variance_norm_factor * cascade->inv_window_area - mean * mean;
    if( variance_norm_factor >= 0. )
        variance_norm_factor = sqrt(variance_norm_factor);
    else
        variance_norm_factor = 1.;
	
//    if( cascade->is_tree )
//    {
//        MyCvHidHaarStageClassifier* ptr;
//        assert( start_stage == 0 );
//		
//        result = 1;
//        ptr = cascade->stage_classifier;
//		
//        while( ptr )
//        {
//            double stage_sum = 0;
//			
//            for( j = 0; j < ptr->count; j++ )
//            {
//                stage_sum += myicvEvalHidHaarClassifier( ptr->classifier + j,
//													  variance_norm_factor, p_offset );
//            }
//			
//            if( stage_sum >= ptr->threshold )
//            {
//                ptr = ptr->child;
//            }
//            else
//            {
//                while( ptr && ptr->next == NULL ) ptr = ptr->parent;
//                if( ptr == NULL )
//                {
//                    result = 0;
//                    EXIT;
//                }
//                ptr = ptr->next;
//            }
//        }
//    }
//    else if( cascade->is_stump_based )
    {
        for( i = start_stage; i < cascade->count; i++ )
        {
            double stage_sum = 0;
			
            if( cascade->stage_classifier[i].two_rects )
            {
                for( j = 0; j < cascade->stage_classifier[i].count; j++ )
                {
                    MyCvHidHaarClassifier* classifier = cascade->stage_classifier[i].classifier + j;
                    MyCvHidHaarTreeNode* node = classifier->node;
                    int t = node->threshold * variance_norm_factor;
                    int sum = calc_sum(node->feature.rect[0],p_offset) * node->feature.rect[0].weight;
                    sum += calc_sum(node->feature.rect[1],p_offset) * node->feature.rect[1].weight;
                    stage_sum += classifier->alpha[sum >= t];
                }
            }
            else
            {
                for( j = 0; j < cascade->stage_classifier[i].count; j++ )
                {
                    MyCvHidHaarClassifier* classifier = cascade->stage_classifier[i].classifier + j;
                    MyCvHidHaarTreeNode* node = classifier->node;
                    int t = node->threshold * variance_norm_factor;
                    int sum = calc_sum(node->feature.rect[0],p_offset) * node->feature.rect[0].weight;
                    sum += calc_sum(node->feature.rect[1],p_offset) * node->feature.rect[1].weight;
                    if( node->feature.rect[2].p0 )
                        sum += calc_sum(node->feature.rect[2],p_offset) * node->feature.rect[2].weight;
                    
                    stage_sum += classifier->alpha[sum >= t];
                }
            }
			
            if( stage_sum < cascade->stage_classifier[i].threshold )
            {
                result = -i;
                EXIT;
            }
        }
    }
//    else
//    {
//        for( i = start_stage; i < cascade->count; i++ )
//        {
//            double stage_sum = 0;
//			
//            for( j = 0; j < cascade->stage_classifier[i].count; j++ )
//            {
//                stage_sum += myicvEvalHidHaarClassifier(
//													  cascade->stage_classifier[i].classifier + j,
//													  variance_norm_factor, p_offset );
//            }
//			
//            if( stage_sum < cascade->stage_classifier[i].threshold )
//            {
//                result = -i;
//                EXIT;
//            }
//        }
//    }
	
    result = 1;
	
    __END__;
	
    return result;
}

#define sum_elem_ptr(sum,row,col)  \
((sumtype*)CV_MAT_ELEM_PTR_FAST((sum),(row),(col),sizeof(sumtype)))

#define sqsum_elem_ptr(sqsum,row,col)  \
((sqsumtype*)CV_MAT_ELEM_PTR_FAST((sqsum),(row),(col),sizeof(sqsumtype)))


CV_IMPL void
mycvSetImagesForHaarClassifierCascade( CvHaarClassifierCascade* _cascade,
									const CvArr* _sum,
									const CvArr* _sqsum,
									const CvArr* _tilted_sum,
									double scale )
{
    CV_FUNCNAME("cvSetImagesForHaarClassifierCascade");
	
    __BEGIN__;
		
    CvMat sum_stub, *sum = (CvMat*)_sum;
    CvMat sqsum_stub, *sqsum = (CvMat*)_sqsum;
    CvMat tilted_stub, *tilted = (CvMat*)_tilted_sum;
    MyCvHidHaarClassifierCascade* cascade;
    int coi0 = 0, coi1 = 0;
    int i;
    CvRect equ_rect;
    double weight_scale;
	
    if( !CV_IS_HAAR_CLASSIFIER(_cascade) )
        CV_ERROR( !_cascade ? CV_StsNullPtr : CV_StsBadArg, "Invalid classifier pointer" );
	
    if( scale <= 0 )
        CV_ERROR( CV_StsOutOfRange, "Scale must be positive" );
	
    CV_CALL( sum = cvGetMat( sum, &sum_stub, &coi0 ));
    CV_CALL( sqsum = cvGetMat( sqsum, &sqsum_stub, &coi1 ));
	
    if( coi0 || coi1 )
        CV_ERROR( CV_BadCOI, "COI is not supported" );
	
    if( !CV_ARE_SIZES_EQ( sum, sqsum ))
        CV_ERROR( CV_StsUnmatchedSizes, "All integral images must have the same size" );
	
    if( CV_MAT_TYPE(sqsum->type) != CV_64FC1 ||
	   CV_MAT_TYPE(sum->type) != CV_32SC1 )
        CV_ERROR( CV_StsUnsupportedFormat,
				 "Only (32s, 64f, 32s) combination of (sum,sqsum,tilted_sum) formats is allowed" );
	
    if( !_cascade->hid_cascade )
        CV_CALL( myicvCreateHidHaarClassifierCascade(_cascade) );
	
    cascade = (MyCvHidHaarClassifierCascade*)_cascade->hid_cascade;
	
    if( cascade->has_tilted_features )
    {
        CV_CALL( tilted = cvGetMat( tilted, &tilted_stub, &coi1 ));
		
        if( CV_MAT_TYPE(tilted->type) != CV_32SC1 )
            CV_ERROR( CV_StsUnsupportedFormat,
					 "Only (32s, 64f, 32s) combination of (sum,sqsum,tilted_sum) formats is allowed" );
		
        if( sum->step != tilted->step )
            CV_ERROR( CV_StsUnmatchedSizes,
					 "Sum and tilted_sum must have the same stride (step, widthStep)" );
		
        if( !CV_ARE_SIZES_EQ( sum, tilted ))
            CV_ERROR( CV_StsUnmatchedSizes, "All integral images must have the same size" );
        cascade->tilted = *tilted;
    }
	
    _cascade->scale = scale;
    _cascade->real_window_size.width = cvRound( _cascade->orig_window_size.width * scale );
    _cascade->real_window_size.height = cvRound( _cascade->orig_window_size.height * scale );
	
    cascade->sum = *sum;
    cascade->sqsum = *sqsum;
	
    equ_rect.x = equ_rect.y = cvRound(scale);
    equ_rect.width = cvRound((_cascade->orig_window_size.width-2)*scale);
    equ_rect.height = cvRound((_cascade->orig_window_size.height-2)*scale);
    weight_scale = 1./(equ_rect.width*equ_rect.height);
    cascade->inv_window_area = weight_scale;
	
    cascade->p0 = sum_elem_ptr(*sum, equ_rect.y, equ_rect.x);
    cascade->p1 = sum_elem_ptr(*sum, equ_rect.y, equ_rect.x + equ_rect.width );
    cascade->p2 = sum_elem_ptr(*sum, equ_rect.y + equ_rect.height, equ_rect.x );
    cascade->p3 = sum_elem_ptr(*sum, equ_rect.y + equ_rect.height,
							   equ_rect.x + equ_rect.width );
	
    cascade->pq0 = sqsum_elem_ptr(*sqsum, equ_rect.y, equ_rect.x);
    cascade->pq1 = sqsum_elem_ptr(*sqsum, equ_rect.y, equ_rect.x + equ_rect.width );
    cascade->pq2 = sqsum_elem_ptr(*sqsum, equ_rect.y + equ_rect.height, equ_rect.x );
    cascade->pq3 = sqsum_elem_ptr(*sqsum, equ_rect.y + equ_rect.height,
								  equ_rect.x + equ_rect.width );
	
    /* init pointers in haar features according to real window size and
	 given image pointers */
    {
#ifdef _OPENMP
		int max_threads = cvGetNumThreads();
#pragma omp parallel for num_threads(max_threads) schedule(dynamic)
#endif // _OPENMP
		for( i = 0; i < _cascade->count; i++ )
		{
			int j, k, l;
			for( j = 0; j < cascade->stage_classifier[i].count; j++ )
			{
				for( l = 0; l < cascade->stage_classifier[i].classifier[j].count; l++ )
				{
					CvHaarFeature* feature =
                    &_cascade->stage_classifier[i].classifier[j].haar_feature[l];
					/* CvHidHaarClassifier* classifier =
					 cascade->stage_classifier[i].classifier + j; */
					MyCvHidHaarFeature* hidfeature =
                    &cascade->stage_classifier[i].classifier[j].node[l].feature;
					double sum0 = 0, area0 = 0;
					CvRect r[3];
#if CV_ADJUST_FEATURES
					int base_w = -1, base_h = -1;
					int new_base_w = 0, new_base_h = 0;
					int kx, ky;
					int flagx = 0, flagy = 0;
					int x0 = 0, y0 = 0;
#endif
					int nr;
					
					/* align blocks */
					for( k = 0; k < CV_HAAR_FEATURE_MAX; k++ )
					{
						if( !hidfeature->rect[k].p0 )
							break;
#if CV_ADJUST_FEATURES
						r[k] = feature->rect[k].r;
						base_w = (int)CV_IMIN( (unsigned)base_w, (unsigned)(r[k].width-1) );
						base_w = (int)CV_IMIN( (unsigned)base_w, (unsigned)(r[k].x - r[0].x-1) );
						base_h = (int)CV_IMIN( (unsigned)base_h, (unsigned)(r[k].height-1) );
						base_h = (int)CV_IMIN( (unsigned)base_h, (unsigned)(r[k].y - r[0].y-1) );
#endif
					}
					
					nr = k;
					
#if CV_ADJUST_FEATURES
					base_w += 1;
					base_h += 1;
					kx = r[0].width / base_w;
					ky = r[0].height / base_h;
					
					if( kx <= 0 )
					{
						flagx = 1;
						new_base_w = cvRound( r[0].width * scale ) / kx;
						x0 = cvRound( r[0].x * scale );
					}
					
					if( ky <= 0 )
					{
						flagy = 1;
						new_base_h = cvRound( r[0].height * scale ) / ky;
						y0 = cvRound( r[0].y * scale );
					}
#endif
					
					float tmpweight[3] = {0};
					
					for( k = 0; k < nr; k++ )
					{
						CvRect tr;
						double correction_ratio;
						
#if CV_ADJUST_FEATURES
						if( flagx )
						{
							tr.x = (r[k].x - r[0].x) * new_base_w / base_w + x0;
							tr.width = r[k].width * new_base_w / base_w;
						}
						else
#endif
						{
							tr.x = cvRound( r[k].x * scale );
							tr.width = cvRound( r[k].width * scale );
						}
						
#if CV_ADJUST_FEATURES
						if( flagy )
						{
							tr.y = (r[k].y - r[0].y) * new_base_h / base_h + y0;
							tr.height = r[k].height * new_base_h / base_h;
						}
						else
#endif
						{
							tr.y = cvRound( r[k].y * scale );
							tr.height = cvRound( r[k].height * scale );
						}
						
#if CV_ADJUST_WEIGHTS
						{
							// RAINER START
							const float orig_feature_size =  (float)(feature->rect[k].r.width)*feature->rect[k].r.height;
							const float orig_norm_size = (float)(_cascade->orig_window_size.width)*(_cascade->orig_window_size.height);
							const float feature_size = float(tr.width*tr.height);
							//const float normSize    = float(equ_rect.width*equ_rect.height);
							float target_ratio = orig_feature_size / orig_norm_size;
							//float isRatio = featureSize / normSize;
							//correctionRatio = targetRatio / isRatio / normSize;
							correction_ratio = target_ratio / feature_size;
							// RAINER END
						}
#else
						correction_ratio = weight_scale * (!feature->tilted ? 1 : 0.5);
#endif
						
						if( !feature->tilted )
						{
							hidfeature->rect[k].p0 = sum_elem_ptr(*sum, tr.y, tr.x);
							hidfeature->rect[k].p1 = sum_elem_ptr(*sum, tr.y, tr.x + tr.width);
							hidfeature->rect[k].p2 = sum_elem_ptr(*sum, tr.y + tr.height, tr.x);
							hidfeature->rect[k].p3 = sum_elem_ptr(*sum, tr.y + tr.height, tr.x + tr.width);
						}
						else
						{
							hidfeature->rect[k].p2 = sum_elem_ptr(*tilted, tr.y + tr.width, tr.x + tr.width);
							hidfeature->rect[k].p3 = sum_elem_ptr(*tilted, tr.y + tr.width + tr.height,
																  tr.x + tr.width - tr.height);
							hidfeature->rect[k].p0 = sum_elem_ptr(*tilted, tr.y, tr.x);
							hidfeature->rect[k].p1 = sum_elem_ptr(*tilted, tr.y + tr.height, tr.x - tr.height);
						}
						
//						hidfeature->rect[k].weight = (float)(feature->rect[k].weight * correction_ratio);
						tmpweight[k] = (float)(feature->rect[k].weight * correction_ratio);
						
						if( k == 0 )
							area0 = tr.width * tr.height;
						else
//							sum0 += hidfeature->rect[k].weight * tr.width * tr.height;
							sum0 += tmpweight[k] * tr.width * tr.height;
					}
					
					tmpweight[0] = (float)(-sum0/area0);
					
					for(int ii = 0; ii < nr; hidfeature->rect[ii].weight = (int)(tmpweight[ii] * 65536.0), ii++);
				} /* l */
			} /* j */
		}
    }
	
    __END__;
}

CvMat *temp = 0, *sum = 0, *sqsum = 0;
double tickFreqTimes1000 = ((double)cvGetTickFrequency()*1000.);

CV_IMPL CvSeq*
mycvHaarDetectObjects( const CvArr* _img,
					CvHaarClassifierCascade* cascade,
					CvMemStorage* storage, double scale_factor,
					int min_neighbors, int flags, CvSize min_size )
{
    int split_stage = 2;
	
    CvMat stub, *img = (CvMat*)_img;
    CvMat  *tilted = 0, *norm_img = 0, *sumcanny = 0, *img_small = 0;
    CvSeq* result_seq = 0;
    CvMemStorage* temp_storage = 0;
    CvAvgComp* comps = 0;
    CvSeq* seq_thread[CV_MAX_THREADS] = {0};
    int i, max_threads = 0;
	double t1;
	
    CV_FUNCNAME( "cvHaarDetectObjects" );
	
    __BEGIN__;
	
	double t = (double)cvGetTickCount();
	
    CvSeq *seq = 0, *seq2 = 0, *idx_seq = 0, *big_seq = 0;
    CvAvgComp result_comp = {{0,0,0,0},0};
    double factor;
    int npass = 2, coi;
    bool do_canny_pruning = (flags & CV_HAAR_DO_CANNY_PRUNING) != 0;
    bool find_biggest_object = (flags & CV_HAAR_FIND_BIGGEST_OBJECT) != 0;
    bool rough_search = (flags & CV_HAAR_DO_ROUGH_SEARCH) != 0;
	
    if( !CV_IS_HAAR_CLASSIFIER(cascade) )
        CV_ERROR( !cascade ? CV_StsNullPtr : CV_StsBadArg, "Invalid classifier cascade" );
	
    if( !storage )
        CV_ERROR( CV_StsNullPtr, "Null storage pointer" );
	
    CV_CALL( img = cvGetMat( img, &stub, &coi ));
    if( coi )
        CV_ERROR( CV_BadCOI, "COI is not supported" );
	
    if( CV_MAT_DEPTH(img->type) != CV_8U )
        CV_ERROR( CV_StsUnsupportedFormat, "Only 8-bit images are supported" );
    
    if( scale_factor <= 1 )
        CV_ERROR( CV_StsOutOfRange, "scale factor must be > 1" );
	
    if( find_biggest_object )
        flags &= ~CV_HAAR_SCALE_IMAGE;
	
	if(!temp) {
		CV_CALL( temp = cvCreateMat( img->rows, img->cols, CV_8UC1 ));
	}
	if(!sum) {
		CV_CALL( sum = cvCreateMat( img->rows + 1, img->cols + 1, CV_32SC1 ));
	}
	if(!sqsum) {
		CV_CALL( sqsum = cvCreateMat( img->rows + 1, img->cols + 1, CV_64FC1 ));
	}
    CV_CALL( temp_storage = cvCreateChildMemStorage( storage ));
	
    if( !cascade->hid_cascade )
        CV_CALL( myicvCreateHidHaarClassifierCascade(cascade) );
	
    if( ((MyCvHidHaarClassifierCascade*)cascade->hid_cascade)->has_tilted_features )
        tilted = cvCreateMat( img->rows + 1, img->cols + 1, CV_32SC1 );
	
    seq = cvCreateSeq( 0, sizeof(CvSeq), sizeof(CvRect), temp_storage );
    seq2 = cvCreateSeq( 0, sizeof(CvSeq), sizeof(CvAvgComp), temp_storage );
    result_seq = cvCreateSeq( 0, sizeof(CvSeq), sizeof(CvAvgComp), storage );
	
    max_threads = cvGetNumThreads();
    if( max_threads > 1 )
        for( i = 0; i < max_threads; i++ )
        {
            CvMemStorage* temp_storage_thread;
            CV_CALL( temp_storage_thread = cvCreateMemStorage(0));
            CV_CALL( seq_thread[i] = cvCreateSeq( 0, sizeof(CvSeq),
												 sizeof(CvRect), temp_storage_thread ));
        }
    else
        seq_thread[0] = seq;
	
    if( CV_MAT_CN(img->type) > 1 )
    {
        cvCvtColor( img, temp, CV_BGR2GRAY );
        img = temp;
    }
	
    if( flags & CV_HAAR_FIND_BIGGEST_OBJECT )
        flags &= ~(CV_HAAR_SCALE_IMAGE|CV_HAAR_DO_CANNY_PRUNING);
	
//    if( flags & CV_HAAR_SCALE_IMAGE )
//    {
//        CvSize win_size0 = cascade->orig_window_size;
//        /*int use_ipp = cascade->hid_cascade->ipp_stages != 0 &&
//		 icvApplyHaarClassifier_32f_C1R_p != 0;
//		 
//		 if( use_ipp )
//		 CV_CALL( norm_img = cvCreateMat( img->rows, img->cols, CV_32FC1 ));*/
//        CV_CALL( img_small = cvCreateMat( img->rows + 1, img->cols + 1, CV_8UC1 ));
//		
//        for( factor = 1; ; factor *= scale_factor )
//        {
//            int strip_count, strip_size;
//            int ystep = factor > 2. ? 1 : 2;
//            CvSize win_size = { cvRound(win_size0.width*factor),
//			cvRound(win_size0.height*factor) };
//            CvSize sz = { cvRound( img->cols/factor ), cvRound( img->rows/factor ) };
//            CvSize sz1 = { sz.width - win_size0.width, sz.height - win_size0.height };
//            /*CvRect equ_rect = { icv_object_win_border, icv_object_win_border,
//			 win_size0.width - icv_object_win_border*2,
//			 win_size0.height - icv_object_win_border*2 };*/
//            CvMat img1, sum1, sqsum1, norm1, tilted1, mask1;
//            CvMat* _tilted = 0;
//			
//            if( sz1.width <= 0 || sz1.height <= 0 )
//                break;
//            if( win_size.width < min_size.width || win_size.height < min_size.height )
//                continue;
//			
//            img1 = cvMat( sz.height, sz.width, CV_8UC1, img_small->data.ptr );
//            sum1 = cvMat( sz.height+1, sz.width+1, CV_32SC1, sum->data.ptr );
//            sqsum1 = cvMat( sz.height+1, sz.width+1, CV_64FC1, sqsum->data.ptr );
//            if( tilted )
//            {
//                tilted1 = cvMat( sz.height+1, sz.width+1, CV_32SC1, tilted->data.ptr );
//                _tilted = &tilted1;
//            }
//            norm1 = cvMat( sz1.height, sz1.width, CV_32FC1, norm_img ? norm_img->data.ptr : 0 );
//            mask1 = cvMat( sz1.height, sz1.width, CV_8UC1, temp->data.ptr );
//			
//            cvResize( img, &img1, CV_INTER_LINEAR );
//            cvIntegral( &img1, &sum1, &sqsum1, _tilted );
//			
//            if( max_threads > 1 )
//            {
//                strip_count = MAX(MIN(sz1.height/ystep, max_threads*3), 1);
//                strip_size = (sz1.height + strip_count - 1)/strip_count;
//                strip_size = (strip_size / ystep)*ystep;
//            }
//            else
//            {
//                strip_count = 1;
//                strip_size = sz1.height;
//            }
//			
//            //if( !use_ipp )
//			cvSetImagesForHaarClassifierCascade( cascade, &sum1, &sqsum1, 0, 1. );
//            /*else
//			 {
//			 for( i = 0; i <= sz.height; i++ )
//			 {
//			 const int* isum = (int*)(sum1.data.ptr + sum1.step*i);
//			 float* fsum = (float*)isum;
//			 const int FLT_DELTA = -(1 << 24);
//			 int j;
//			 for( j = 0; j <= sz.width; j++ )
//			 fsum[j] = (float)(isum[j] + FLT_DELTA);
//			 }
//			 }*/
//			
//#ifdef _OPENMP
//#pragma omp parallel for num_threads(max_threads) schedule(dynamic)
//#endif
//            for( i = 0; i < strip_count; i++ )
//            {
//                int thread_id = cvGetThreadNum();
//                int positive = 0;
//                int y1 = i*strip_size, y2 = (i+1)*strip_size/* - ystep + 1*/;
//                CvSize ssz;
//                int x, y;
//                if( i == strip_count - 1 || y2 > sz1.height )
//                    y2 = sz1.height;
//                ssz = cvSize(sz1.width, y2 - y1);
//				
//                /*if( use_ipp )
//				 {
//				 icvRectStdDev_32f_C1R_p(
//				 (float*)(sum1.data.ptr + y1*sum1.step), sum1.step,
//				 (double*)(sqsum1.data.ptr + y1*sqsum1.step), sqsum1.step,
//				 (float*)(norm1.data.ptr + y1*norm1.step), norm1.step, ssz, equ_rect );
//				 
//				 positive = (ssz.width/ystep)*((ssz.height + ystep-1)/ystep);
//				 memset( mask1.data.ptr + y1*mask1.step, ystep == 1, mask1.height*mask1.step);
//				 
//				 if( ystep > 1 )
//				 {
//				 for( y = y1, positive = 0; y < y2; y += ystep )
//				 for( x = 0; x < ssz.width; x += ystep )
//				 mask1.data.ptr[mask1.step*y + x] = (uchar)1;
//				 }
//				 
//				 for( int j = 0; j < cascade->count; j++ )
//				 {
//				 if( icvApplyHaarClassifier_32f_C1R_p(
//				 (float*)(sum1.data.ptr + y1*sum1.step), sum1.step,
//				 (float*)(norm1.data.ptr + y1*norm1.step), norm1.step,
//				 mask1.data.ptr + y1*mask1.step, mask1.step, ssz, &positive,
//				 cascade->hid_cascade->stage_classifier[j].threshold,
//				 cascade->hid_cascade->ipp_stages[j]) < 0 )
//				 {
//				 positive = 0;
//				 break;
//				 }
//				 if( positive <= 0 )
//				 break;
//				 }
//				 }
//				 else*/
//                {
//                    for( y = y1, positive = 0; y < y2; y += ystep )
//                        for( x = 0; x < ssz.width; x += ystep )
//                        {
//                            mask1.data.ptr[mask1.step*y + x] =
//							mycvRunHaarClassifierCascade( cascade, cvPoint(x,y), 0 ) > 0;
//                            positive += mask1.data.ptr[mask1.step*y + x];
//                        }
//                }
//				
//                if( positive > 0 )
//                {
//                    for( y = y1; y < y2; y += ystep )
//                        for( x = 0; x < ssz.width; x += ystep )
//                            if( mask1.data.ptr[mask1.step*y + x] != 0 )
//                            {
//                                CvRect obj_rect = { cvRound(x*factor), cvRound(y*factor),
//								win_size.width, win_size.height };
//                                cvSeqPush( seq_thread[thread_id], &obj_rect );
//                            }
//                }
//            }
//			
//            // gather the results
//            if( max_threads > 1 )
//                for( i = 0; i < max_threads; i++ )
//                {
//                    CvSeq* s = seq_thread[i];
//                    int j, total = s->total;
//                    CvSeqBlock* b = s->first;
//                    for( j = 0; j < total; j += b->count, b = b->next )
//                        cvSeqPushMulti( seq, b->data, b->count );
//                }
//        }
//    }
//    else
	t1 = (double)cvGetTickCount();
//	printf( "init time = %gms\n", (t1 - t)/tickFreqTimes1000);
	t = t1;
	
    {
        int n_factors = 0;
        CvRect scan_roi_rect = {0,0,0,0};
        bool is_found = false, scan_roi = false;
		
        cvIntegral( img, sum, sqsum, tilted );
		
//        if( do_canny_pruning )
//        {
//            sumcanny = cvCreateMat( img->rows + 1, img->cols + 1, CV_32SC1 );
//            cvCanny( img, temp, 0, 50, 3 );
//            cvIntegral( temp, sumcanny );
//        }
		
        if( (unsigned)split_stage >= (unsigned)cascade->count ||
		   ((MyCvHidHaarClassifierCascade*)cascade->hid_cascade)->is_tree )
        {
            split_stage = cascade->count;
            npass = 1;
        }
		
        for( n_factors = 0, factor = 1;
			factor*cascade->orig_window_size.width < img->cols - 10 &&
			factor*cascade->orig_window_size.height < img->rows - 10;
			n_factors++, factor *= scale_factor )
            ;
		
        if( find_biggest_object )
        {
            scale_factor = 1./scale_factor;
            factor *= scale_factor;
            big_seq = cvCreateSeq( 0, sizeof(CvSeq), sizeof(CvRect), temp_storage );
        }
        else
            factor = 1;
				
        for( ; n_factors-- > 0 && !is_found; factor *= scale_factor )
        {
            const double ystep = MAX( 2, factor );
            CvSize win_size = { cvRound( cascade->orig_window_size.width * factor ),
			cvRound( cascade->orig_window_size.height * factor )};
            CvRect equ_rect = { 0, 0, 0, 0 };
            int *p0 = 0, *p1 = 0, *p2 = 0, *p3 = 0;
            int *pq0 = 0, *pq1 = 0, *pq2 = 0, *pq3 = 0;
            int pass, stage_offset = 0;
            int start_x = 0, start_y = 0;
            int end_x = cvRound((img->cols - win_size.width) / ystep);
            int end_y = cvRound((img->rows - win_size.height) / ystep);
			
            if( win_size.width < min_size.width || win_size.height < min_size.height )
            {
                if( find_biggest_object )
                    break;
                continue;
            }
			
            mycvSetImagesForHaarClassifierCascade( cascade, sum, sqsum, tilted, factor );
            cvZero( temp );
			
//            if( do_canny_pruning )
//            {
//                equ_rect.x = cvRound(win_size.width*0.15);
//                equ_rect.y = cvRound(win_size.height*0.15);
//                equ_rect.width = cvRound(win_size.width*0.7);
//                equ_rect.height = cvRound(win_size.height*0.7);
//				
//                p0 = (int*)(sumcanny->data.ptr + equ_rect.y*sumcanny->step) + equ_rect.x;
//                p1 = (int*)(sumcanny->data.ptr + equ_rect.y*sumcanny->step)
//				+ equ_rect.x + equ_rect.width;
//                p2 = (int*)(sumcanny->data.ptr + (equ_rect.y + equ_rect.height)*sumcanny->step) + equ_rect.x;
//                p3 = (int*)(sumcanny->data.ptr + (equ_rect.y + equ_rect.height)*sumcanny->step)
//				+ equ_rect.x + equ_rect.width;
//				
//                pq0 = (int*)(sum->data.ptr + equ_rect.y*sum->step) + equ_rect.x;
//                pq1 = (int*)(sum->data.ptr + equ_rect.y*sum->step)
//				+ equ_rect.x + equ_rect.width;
//                pq2 = (int*)(sum->data.ptr + (equ_rect.y + equ_rect.height)*sum->step) + equ_rect.x;
//                pq3 = (int*)(sum->data.ptr + (equ_rect.y + equ_rect.height)*sum->step)
//				+ equ_rect.x + equ_rect.width;
//            }
			
            if( scan_roi )
            {
                //adjust start_height and stop_height
                start_y = cvRound(scan_roi_rect.y / ystep);
                end_y = cvRound((scan_roi_rect.y + scan_roi_rect.height - win_size.height) / ystep);
				
                start_x = cvRound(scan_roi_rect.x / ystep);
                end_x = cvRound((scan_roi_rect.x + scan_roi_rect.width - win_size.width) / ystep);
            }
			
            ((MyCvHidHaarClassifierCascade*)cascade->hid_cascade)->count = split_stage;
			
            for( pass = 0; pass < npass; pass++ )
            {
#ifdef _OPENMP
#pragma omp parallel for num_threads(max_threads) schedule(dynamic)
#endif
                for( int _iy = start_y; _iy < end_y; _iy++ )
                {
                    int thread_id = cvGetThreadNum();
                    int iy = cvRound(_iy*ystep);
                    int _ix, _xstep = 1;
                    uchar* mask_row = temp->data.ptr + temp->step * iy;
					
                    for( _ix = start_x; _ix < end_x; _ix += _xstep )
                    {
                        int ix = cvRound(_ix*ystep); // it really should be ystep
						
                        if( pass == 0 )
                        {
                            int result;
                            _xstep = 2;
							
//                            if( do_canny_pruning )
//                            {
//                                int offset;
//                                int s, sq;
//								
//                                offset = iy*(sum->step/sizeof(p0[0])) + ix;
//                                s = p0[offset] - p1[offset] - p2[offset] + p3[offset];
//                                sq = pq0[offset] - pq1[offset] - pq2[offset] + pq3[offset];
//                                if( s < 100 || sq < 20 )
//                                    continue;
//                            }
							
                            result = mycvRunHaarClassifierCascade( cascade, cvPoint(ix,iy), 0 );
                            if( result > 0 )
                            {
                                if( pass < npass - 1 )
                                    mask_row[ix] = 1;
                                else
                                {
                                    CvRect rect = cvRect(ix,iy,win_size.width,win_size.height);
                                    cvSeqPush( seq_thread[thread_id], &rect );
                                }
                            }
                            if( result < 0 )
                                _xstep = 1;
                        }
                        else if( mask_row[ix] )
                        {
                            int result = mycvRunHaarClassifierCascade( cascade, cvPoint(ix,iy),
																	stage_offset );
                            if( result > 0 )
                            {
                                if( pass == npass - 1 )
                                {
                                    CvRect rect = cvRect(ix,iy,win_size.width,win_size.height);
                                    cvSeqPush( seq_thread[thread_id], &rect );
                                }
                            }
                            else
                                mask_row[ix] = 0;
                        }
                    }
                }
                stage_offset = ((MyCvHidHaarClassifierCascade*)cascade->hid_cascade)->count;
                ((MyCvHidHaarClassifierCascade*)cascade->hid_cascade)->count = cascade->count;
            }
			
            // gather the results
            if( max_threads > 1 )
	            for( i = 0; i < max_threads; i++ )
	            {
		            CvSeq* s = seq_thread[i];
                    int j, total = s->total;
                    CvSeqBlock* b = s->first;
                    for( j = 0; j < total; j += b->count, b = b->next )
                        cvSeqPushMulti( seq, b->data, b->count );
	            }
			
            if( find_biggest_object )
            {
                CvSeq* bseq = min_neighbors > 0 ? big_seq : seq;
                
                if( min_neighbors > 0 && !scan_roi )
                {
                    // group retrieved rectangles in order to filter out noise
                    int ncomp = cvSeqPartition( seq, 0, &idx_seq, myis_equal, 0 );
                    CV_CALL( comps = (CvAvgComp*)cvAlloc( (ncomp+1)*sizeof(comps[0])));
                    memset( comps, 0, (ncomp+1)*sizeof(comps[0]));
					
#if VERY_ROUGH_SEARCH
                    if( rough_search )
                    {
                        for( i = 0; i < seq->total; i++ )
                        {
                            CvRect r1 = *(CvRect*)cvGetSeqElem( seq, i );
                            int idx = *(int*)cvGetSeqElem( idx_seq, i );
                            assert( (unsigned)idx < (unsigned)ncomp );
							
                            comps[idx].neighbors++;
                            comps[idx].rect.x += r1.x;
                            comps[idx].rect.y += r1.y;
                            comps[idx].rect.width += r1.width;
                            comps[idx].rect.height += r1.height;
                        }
						
                        // calculate average bounding box
                        for( i = 0; i < ncomp; i++ )
                        {
                            int n = comps[i].neighbors;
                            if( n >= min_neighbors )
                            {
                                CvAvgComp comp;
                                comp.rect.x = (comps[i].rect.x*2 + n)/(2*n);
                                comp.rect.y = (comps[i].rect.y*2 + n)/(2*n);
                                comp.rect.width = (comps[i].rect.width*2 + n)/(2*n);
                                comp.rect.height = (comps[i].rect.height*2 + n)/(2*n);
                                comp.neighbors = n;
                                cvSeqPush( bseq, &comp );
                            }
                        }
                    }
                    else
#endif
                    {
                        for( i = 0 ; i <= ncomp; i++ )
                            comps[i].rect.x = comps[i].rect.y = INT_MAX;
						
                        // count number of neighbors
                        for( i = 0; i < seq->total; i++ )
                        {
                            CvRect r1 = *(CvRect*)cvGetSeqElem( seq, i );
                            int idx = *(int*)cvGetSeqElem( idx_seq, i );
                            assert( (unsigned)idx < (unsigned)ncomp );
							
                            comps[idx].neighbors++;
							
                            // rect.width and rect.height will store coordinate of right-bottom corner
                            comps[idx].rect.x = MIN(comps[idx].rect.x, r1.x);
                            comps[idx].rect.y = MIN(comps[idx].rect.y, r1.y);
                            comps[idx].rect.width = MAX(comps[idx].rect.width, r1.x+r1.width-1);
                            comps[idx].rect.height = MAX(comps[idx].rect.height, r1.y+r1.height-1);
                        }
						
                        // calculate enclosing box
                        for( i = 0; i < ncomp; i++ )
                        {
                            int n = comps[i].neighbors;
                            if( n >= min_neighbors )
                            {
                                CvAvgComp comp;
                                int t;
                                double min_scale = rough_search ? 0.6 : 0.4;
                                comp.rect.x = comps[i].rect.x;
                                comp.rect.y = comps[i].rect.y;
                                comp.rect.width = comps[i].rect.width - comps[i].rect.x + 1;
                                comp.rect.height = comps[i].rect.height - comps[i].rect.y + 1;
								
                                // update min_size
                                t = cvRound( comp.rect.width*min_scale );
                                min_size.width = MAX( min_size.width, t );
								
                                t = cvRound( comp.rect.height*min_scale );
                                min_size.height = MAX( min_size.height, t );
								
                                //expand the box by 20% because we could miss some neighbours
                                //see 'is_equal' function
#if 1
                                int offset = cvRound(comp.rect.width * 0.2);
                                int right = MIN( img->cols-1, comp.rect.x+comp.rect.width-1 + offset );
                                int bottom = MIN( img->rows-1, comp.rect.y+comp.rect.height-1 + offset);
                                comp.rect.x = MAX( comp.rect.x - offset, 0 );
                                comp.rect.y = MAX( comp.rect.y - offset, 0 );
                                comp.rect.width = right - comp.rect.x + 1;
                                comp.rect.height = bottom - comp.rect.y + 1;
#endif
								
                                comp.neighbors = n;
                                cvSeqPush( bseq, &comp );
                            }
                        }
                    }
					
                    cvFree( &comps );
                }
				
                // extract the biggest rect
                if( bseq->total > 0 )
                {
                    int max_area = 0;
                    for( i = 0; i < bseq->total; i++ )
                    {
                        CvAvgComp* comp = (CvAvgComp*)cvGetSeqElem( bseq, i );
                        int area = comp->rect.width * comp->rect.height;
                        if( max_area < area )
                        {
                            max_area = area;
                            result_comp.rect = comp->rect;
                            result_comp.neighbors = bseq == seq ? 1 : comp->neighbors;
                        }
                    }
					
                    //Prepare information for further scanning inside the biggest rectangle
					
#if VERY_ROUGH_SEARCH
                    // change scan ranges to roi in case of required
                    if( !rough_search && !scan_roi )
                    {
                        scan_roi = true;
                        scan_roi_rect = result_comp.rect;
                        cvClearSeq(bseq);
                    }
                    else if( rough_search )
                        is_found = true;
#else
                    if( !scan_roi )
                    {
                        scan_roi = true;
                        scan_roi_rect = result_comp.rect;
                        cvClearSeq(bseq);
                    }
#endif
                }
            }
        }
    }
	
//	t1 = (double)cvGetTickCount();
//	printf( "factors time = %gms\n", (t1 - t)/tickFreqTimes1000);
//	t = t1;

    if( min_neighbors == 0 && !find_biggest_object )
    {
        for( i = 0; i < seq->total; i++ )
        {
            CvRect* rect = (CvRect*)cvGetSeqElem( seq, i );
            CvAvgComp comp;
            comp.rect = *rect;
            comp.neighbors = 1;
            cvSeqPush( result_seq, &comp );
        }
    }
	
    if( min_neighbors != 0
#if VERY_ROUGH_SEARCH        
	   && (!find_biggest_object || !rough_search)
#endif        
	   )
    {
        // group retrieved rectangles in order to filter out noise
        int ncomp = cvSeqPartition( seq, 0, &idx_seq, myis_equal, 0 );
        CV_CALL( comps = (CvAvgComp*)cvAlloc( (ncomp+1)*sizeof(comps[0])));
        memset( comps, 0, (ncomp+1)*sizeof(comps[0]));
		
        // count number of neighbors
        for( i = 0; i < seq->total; i++ )
        {
            CvRect r1 = *(CvRect*)cvGetSeqElem( seq, i );
            int idx = *(int*)cvGetSeqElem( idx_seq, i );
            assert( (unsigned)idx < (unsigned)ncomp );
			
            comps[idx].neighbors++;
			
            comps[idx].rect.x += r1.x;
            comps[idx].rect.y += r1.y;
            comps[idx].rect.width += r1.width;
            comps[idx].rect.height += r1.height;
        }
		
        // calculate average bounding box
        for( i = 0; i < ncomp; i++ )
        {
            int n = comps[i].neighbors;
            if( n >= min_neighbors )
            {
                CvAvgComp comp;
                comp.rect.x = (comps[i].rect.x*2 + n)/(2*n);
                comp.rect.y = (comps[i].rect.y*2 + n)/(2*n);
                comp.rect.width = (comps[i].rect.width*2 + n)/(2*n);
                comp.rect.height = (comps[i].rect.height*2 + n)/(2*n);
                comp.neighbors = comps[i].neighbors;
				
                cvSeqPush( seq2, &comp );
            }
        }
		
        if( !find_biggest_object )
        {
            // filter out small face rectangles inside large face rectangles
            for( i = 0; i < seq2->total; i++ )
            {
                CvAvgComp r1 = *(CvAvgComp*)cvGetSeqElem( seq2, i );
                int j, flag = 1;
				
                for( j = 0; j < seq2->total; j++ )
                {
                    CvAvgComp r2 = *(CvAvgComp*)cvGetSeqElem( seq2, j );
                    int distance = cvRound( r2.rect.width * 0.2 );
					
                    if( i != j &&
					   r1.rect.x >= r2.rect.x - distance &&
					   r1.rect.y >= r2.rect.y - distance &&
					   r1.rect.x + r1.rect.width <= r2.rect.x + r2.rect.width + distance &&
					   r1.rect.y + r1.rect.height <= r2.rect.y + r2.rect.height + distance &&
					   (r2.neighbors > MAX( 3, r1.neighbors ) || r1.neighbors < 3) )
                    {
                        flag = 0;
                        break;
                    }
                }
				
                if( flag )
                    cvSeqPush( result_seq, &r1 );
            }
        }
        else
        {
            int max_area = 0;
            for( i = 0; i < seq2->total; i++ )
            {
                CvAvgComp* comp = (CvAvgComp*)cvGetSeqElem( seq2, i );
                int area = comp->rect.width * comp->rect.height;
                if( max_area < area )
                {
                    max_area = area;
                    result_comp = *comp;
                }                
            }
        }
    }
	
	t1 = (double)cvGetTickCount();
//	printf( "results eval time = %gms\n", (t1 - t)/tickFreqTimes1000);
	t = t1;

    if( find_biggest_object && result_comp.rect.width > 0 )
        cvSeqPush( result_seq, &result_comp );
	
    __END__;
	
    if( max_threads > 1 )
	    for( i = 0; i < max_threads; i++ )
	    {
		    if( seq_thread[i] )
                cvReleaseMemStorage( &seq_thread[i]->storage );
	    }
	
    cvReleaseMemStorage( &temp_storage );
    cvReleaseMat( &sum );
    cvReleaseMat( &sqsum );
    cvReleaseMat( &tilted );
    cvReleaseMat( &temp );
    cvReleaseMat( &sumcanny );
    cvReleaseMat( &norm_img );
    cvReleaseMat( &img_small );
    cvFree( &comps );
	
    return result_seq;
}
