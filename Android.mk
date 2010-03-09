LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := cxcore
LOCAL_C_INCLUDES := \
        $(LOCAL_PATH)/cxcore/include 
LOCAL_CFLAGS := $(LOCAL_C_INCLUDES:%=-I%)
LOCAL_LDLIBS := -L$(SYSROOT)/usr/lib -ldl

LOCAL_SRC_FILES := \
        cxcore/src/cxalloc.cpp \
        cxcore/src/cxarithm.cpp \
        cxcore/src/cxarray.cpp \
        cxcore/src/cxcmp.cpp \
        cxcore/src/cxconvert.cpp \
        cxcore/src/cxcopy.cpp \
        cxcore/src/cxdatastructs.cpp \
        cxcore/src/cxdrawing.cpp \
        cxcore/src/cxdxt.cpp \
        cxcore/src/cxerror.cpp \
        cxcore/src/cximage.cpp \
        cxcore/src/cxjacobieigens.cpp \
        cxcore/src/cxlogic.cpp \
        cxcore/src/cxlut.cpp \
        cxcore/src/cxmathfuncs.cpp \
        cxcore/src/cxmatmul.cpp \
        cxcore/src/cxmatrix.cpp \
        cxcore/src/cxmean.cpp \
        cxcore/src/cxmeansdv.cpp \
        cxcore/src/cxminmaxloc.cpp \
        cxcore/src/cxnorm.cpp \
        cxcore/src/cxouttext.cpp \
        cxcore/src/cxpersistence.cpp \
        cxcore/src/cxprecomp.cpp \
        cxcore/src/cxrand.cpp \
        cxcore/src/cxsumpixels.cpp \
        cxcore/src/cxsvd.cpp \
        cxcore/src/cxswitcher.cpp \
        cxcore/src/cxtables.cpp \
        cxcore/src/cxutils.cpp

include $(BUILD_STATIC_LIBRARY)



include $(CLEAR_VARS)

LOCAL_MODULE    := cv
LOCAL_C_INCLUDES := \
        $(LOCAL_PATH)/cxcore/include \
        $(LOCAL_PATH)/cxcore/src \
        $(LOCAL_PATH)/cv/include 
LOCAL_CFLAGS := $(LOCAL_C_INCLUDES:%=-I%)
LOCAL_LDLIBS := -L$(SYSROOT)/usr/lib -ldl

LOCAL_SRC_FILES := \
        cv/src/cvaccum.cpp \
        cv/src/cvadapthresh.cpp \
        cv/src/cvapprox.cpp \
        cv/src/cvcalccontrasthistogram.cpp \
        cv/src/cvcalcimagehomography.cpp \
        cv/src/cvcalibinit.cpp \
        cv/src/cvcalibration.cpp \
        cv/src/cvcamshift.cpp \
        cv/src/cvcanny.cpp \
        cv/src/cvcolor.cpp \
        cv/src/cvcondens.cpp \
        cv/src/cvcontours.cpp \
        cv/src/cvcontourtree.cpp \
        cv/src/cvconvhull.cpp \
        cv/src/cvcorner.cpp \
        cv/src/cvcornersubpix.cpp \
        cv/src/cvderiv.cpp \
        cv/src/cvdistransform.cpp \
        cv/src/cvdominants.cpp \
        cv/src/cvemd.cpp \
        cv/src/cvfeatureselect.cpp \
        cv/src/cvfilter.cpp \
        cv/src/cvfloodfill.cpp \
        cv/src/cvfundam.cpp \
        cv/src/cvgeometry.cpp \
        cv/src/cvhaar.cpp \
        cv/src/cvhistogram.cpp \
        cv/src/cvhough.cpp \
        cv/src/cvimgwarp.cpp \
        cv/src/cvinpaint.cpp \
        cv/src/cvkalman.cpp \
        cv/src/cvlinefit.cpp \
        cv/src/cvlkpyramid.cpp \
        cv/src/cvmatchcontours.cpp \
        cv/src/cvmoments.cpp \
        cv/src/cvmorph.cpp \
        cv/src/cvmotempl.cpp \
        cv/src/cvoptflowbm.cpp \
        cv/src/cvoptflowhs.cpp \
        cv/src/cvoptflowlk.cpp \
        cv/src/cvpgh.cpp \
        cv/src/cvposit.cpp \
        cv/src/cvprecomp.cpp \
        cv/src/cvpyramids.cpp \
        cv/src/cvpyrsegmentation.cpp \
        cv/src/cvrotcalipers.cpp \
        cv/src/cvsamplers.cpp \
        cv/src/cvsegmentation.cpp \
        cv/src/cvshapedescr.cpp \
        cv/src/cvsmooth.cpp \
        cv/src/cvsnakes.cpp \
        cv/src/cvstereobm.cpp \
        cv/src/cvstereogc.cpp \
        cv/src/cvsubdivision2d.cpp \
        cv/src/cvsumpixels.cpp \
        cv/src/cvsurf.cpp \
        cv/src/cvswitcher.cpp \
        cv/src/cvtables.cpp \
        cv/src/cvtemplmatch.cpp \
        cv/src/cvthresh.cpp \
        cv/src/cvundistort.cpp \
        cv/src/cvutils.cpp \
        cv/src/mycvHaarDetectObjects.cpp
#        cv/src/cvkdtree.cpp \

include $(BUILD_STATIC_LIBRARY)



include $(CLEAR_VARS)

LOCAL_MODULE    := cvaux
LOCAL_C_INCLUDES := \
        $(LOCAL_PATH)/cv/src \
        $(LOCAL_PATH)/cv/include \
        $(LOCAL_PATH)/cxcore/include \
        $(LOCAL_PATH)/cvaux/include 
LOCAL_CFLAGS := $(LOCAL_C_INCLUDES:%=-I%)
LOCAL_LDLIBS := -L$(SYSROOT)/usr/lib -ldl

LOCAL_SRC_FILES := \
        cvaux/src/camshift.cpp \
        cvaux/src/cvaux.cpp \
        cvaux/src/cvauxutils.cpp \
        cvaux/src/cvbgfg_acmmm2003.cpp \
        cvaux/src/cvbgfg_codebook.cpp \
        cvaux/src/cvbgfg_common.cpp \
        cvaux/src/cvbgfg_gaussmix.cpp \
        cvaux/src/cvcalibfilter.cpp \
        cvaux/src/cvclique.cpp \
        cvaux/src/cvcorrespond.cpp \
        cvaux/src/cvcorrimages.cpp \
        cvaux/src/cvcreatehandmask.cpp \
        cvaux/src/cvdpstereo.cpp \
        cvaux/src/cveigenobjects.cpp \
        cvaux/src/cvepilines.cpp \
        cvaux/src/cvface.cpp \
        cvaux/src/cvfacedetection.cpp \
        cvaux/src/cvfacetemplate.cpp \
        cvaux/src/cvfindface.cpp \
        cvaux/src/cvfindhandregion.cpp \
        cvaux/src/cvhmm.cpp \
        cvaux/src/cvhmm1d.cpp \
        cvaux/src/cvhmmobs.cpp \
        cvaux/src/cvlcm.cpp \
        cvaux/src/cvlee.cpp \
        cvaux/src/cvlevmar.cpp \
        cvaux/src/cvlevmarprojbandle.cpp \
        cvaux/src/cvlevmartrif.cpp \
        cvaux/src/cvlines.cpp \
        cvaux/src/cvlmeds.cpp \
        cvaux/src/cvmat.cpp \
        cvaux/src/cvmorphcontours.cpp \
        cvaux/src/cvmorphing.cpp \
        cvaux/src/cvprewarp.cpp \
        cvaux/src/cvscanlines.cpp \
        cvaux/src/cvsegment.cpp \
        cvaux/src/cvsubdiv2.cpp \
        cvaux/src/cvtexture.cpp \
        cvaux/src/cvtrifocal.cpp \
        cvaux/src/cvvecfacetracking.cpp \
        cvaux/src/cvvideo.cpp \
        cvaux/src/decomppoly.cpp \
        cvaux/src/enmin.cpp \
        cvaux/src/extendededges.cpp \
        cvaux/src/precomp.cpp
#        cvaux/src/cv3dtracker.cpp \

include $(BUILD_STATIC_LIBRARY)



include $(CLEAR_VARS)

LOCAL_MODULE    := cvml
LOCAL_C_INCLUDES := \
        $(LOCAL_PATH)/cv/src \
        $(LOCAL_PATH)/cv/include \
        $(LOCAL_PATH)/cxcore/include \
        $(LOCAL_PATH)/ml/include
LOCAL_CFLAGS := $(LOCAL_C_INCLUDES:%=-I%)
LOCAL_LDLIBS := -L$(SYSROOT)/usr/lib -ldl

LOCAL_SRC_FILES := \
        ml/src/ml.cpp \
        ml/src/mlann_mlp.cpp \
        ml/src/mlboost.cpp \
        ml/src/mlcnn.cpp \
        ml/src/mlem.cpp \
        ml/src/mlestimate.cpp \
        ml/src/mlknearest.cpp \
        ml/src/mlnbayes.cpp \
        ml/src/mlrtrees.cpp \
        ml/src/mlsvm.cpp \
        ml/src/mltestset.cpp \
        ml/src/mltree.cpp \
        ml/src/ml_inner_functions.cpp

include $(BUILD_STATIC_LIBRARY)



include $(CLEAR_VARS)

LOCAL_MODULE    := cvhighgui
LOCAL_C_INCLUDES := \
        $(LOCAL_PATH)/cv/src \
        $(LOCAL_PATH)/cv/include \
        $(LOCAL_PATH)/cxcore/include \
        $(LOCAL_PATH)/otherlibs/highgui
LOCAL_CFLAGS := $(LOCAL_C_INCLUDES:%=-I%)
LOCAL_LDLIBS := -L$(SYSROOT)/usr/lib -ldl

LOCAL_SRC_FILES := \
        otherlibs/highgui/bitstrm.cpp \
        otherlibs/highgui/grfmt_base.cpp \
        otherlibs/highgui/grfmt_bmp.cpp \
        otherlibs/highgui/grfmt_exr.cpp \
        otherlibs/highgui/grfmt_imageio.cpp \
        otherlibs/highgui/grfmt_jpeg.cpp \
        otherlibs/highgui/grfmt_jpeg2000.cpp \
        otherlibs/highgui/grfmt_png.cpp \
        otherlibs/highgui/grfmt_pxm.cpp \
        otherlibs/highgui/grfmt_sunras.cpp \
        otherlibs/highgui/grfmt_tiff.cpp \
        otherlibs/highgui/image.cpp \
        otherlibs/highgui/loadsave.cpp \
        otherlibs/highgui/precomp.cpp \
        otherlibs/highgui/utils.cpp \
        otherlibs/highgui/cvcap.cpp \
        otherlibs/highgui/cvcap_socket.cpp

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE    := opencv
LOCAL_C_INCLUDES := \
        $(LOCAL_PATH)/cv/src \
        $(LOCAL_PATH)/cv/include \
        $(LOCAL_PATH)/cxcore/include \
        $(LOCAL_PATH)/cvaux/src \
        $(LOCAL_PATH)/cvaux/include \
        $(LOCAL_PATH)/ml/include \
        $(LOCAL_PATH)/otherlibs/highgui
LOCAL_CFLAGS := $(LOCAL_C_INCLUDES:%=-I%)
LOCAL_LDLIBS := -L$(SYSROOT)/usr/lib -ldl -llog \
                -L$(TARGET_OUT) -lcxcore -lcv -lcvaux -lcvml -lcvhighgui


LOCAL_SRC_FILES := \
        WLNonFileByteStream.cpp \
        cvjni.cpp


LOCAL_STATIC_LIBRARIES := cxcore cv cvaux cvml cvhighgui

include $(BUILD_SHARED_LIBRARY)

