/*
 * OpenCV for Android NDK
 * Copyright (c) 2006-2009 SIProp Project http://www.siprop.org/
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from the use of this software.
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it freely,
 * subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 */

package org.siprop.opencv;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Rect;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.ImageView;

import java.util.concurrent.SynchronousQueue;

/**
 * This activity emulates video in the Android emulator by using camera images
 * served over a socket connection. It performs an OpenCV action on every frame
 * of video and displays the result in near real-time.
 *
 * @author bill
 */
public class VideoEmulation extends Activity {

    private static final String TAG = "VideoEmulation";

    private static final String CASCADE_PATH = "/data/data/org.siprop.opencv/files/haarcascade_frontalface_alt.xml";

    private final int FP = ViewGroup.LayoutParams.FILL_PARENT;

    // THESE MUST MATCH THE WIDTH AND HEIGHT OF THE WEBCAMBROADCASTER!
    public static final int DEFAULT_WIDTH = 300;

    public static final int DEFAULT_HEIGHT = 300;

    private SocketCamera mSocketCamera;

    private OpenCV mOpenCV;

    private String mRemoteCameraAddress;

    private String mOpenCVAction;

    private String mCameraOption;

    private String mRemoteCameraPort;

    private boolean mPreview;

    private ImageView mImageView;

    private SynchronousQueue<Bitmap> mSyncQueue;

    private ActivityManager mActivityMgr;

    private ActivityManager.MemoryInfo mActivityMgrMemInfo;

    @Override
    public void onCreate(Bundle bundle) {
        super.onCreate(bundle);

        Log.d(TAG, "onCreate");

        mOpenCV = new OpenCV();
        mSyncQueue = new SynchronousQueue<Bitmap>();

        // Initialize Camera.
        mPreview = false;
        setContentView(R.layout.main);
        mImageView = new ImageView(this);

        Bundle extras = getIntent().getExtras();
        mRemoteCameraAddress = extras
                .getString(VideoEmulationConfiguration.EXTRA_REMOTE_CAMERA_ADDRESS);
        mRemoteCameraPort = extras.getString(VideoEmulationConfiguration.EXTRA_REMOTE_CAMERA_PORT);

        mOpenCVAction = extras.getString(VideoEmulationConfiguration.EXTRA_OPENCV_ACTION);
        Log.d(TAG, "mOpenCVAction = " + mOpenCVAction);

        mCameraOption = extras.getString(VideoEmulationConfiguration.EXTRA_CAMERA_OPTION);

        // Let's monitor for memory leaks.
        mActivityMgr = (ActivityManager)super.getSystemService(Context.ACTIVITY_SERVICE);
        mActivityMgrMemInfo = new ActivityManager.MemoryInfo();
    }

    @Override
    public void onResume() {
        Log.d(TAG, "onResume");
        super.onResume();

        Log.d(TAG, "initFaceDetect");
        if (!mOpenCV.initFaceDetection(CASCADE_PATH)) {
            Log.d(TAG, "Failed to initialize face detection!");
            return;
        }

        if (mCameraOption.equals(VideoEmulationConfiguration.C_SOCKET_CAMERA)) {
            if (!mOpenCV.createSocketCapture(mRemoteCameraAddress, mRemoteCameraPort, DEFAULT_WIDTH, DEFAULT_HEIGHT)) {
                Log.d(TAG, "Failed to create socket capture!");
                return;
            }
        } else {
            mSocketCamera = new SocketCamera(mRemoteCameraAddress, Short.parseShort(mRemoteCameraPort));
        }

        mPreview = true;
        mPreviewThread.start();
    }

    @Override
    public void onPause() {
        Log.d(TAG, "onPause");
        mPreview = false;
        try {
            mPreviewThread.join();
        } catch (InterruptedException e) {
            Log.d(TAG, "onPause");
        }

        mSocketCamera = null;
        mOpenCV.releaseFaceDetection();
        if (mCameraOption.equals(VideoEmulationConfiguration.C_SOCKET_CAMERA)) {
            mOpenCV.releaseSocketCapture();
        }

        super.onPause();
    }

    /*
     * The preview thread will continuously get images from the socket camera,
     * perform some OpenCV function on them, and then send them to the UIThread
     * via a sync queue and message handler. Using a sync queue ensures that we
     * only produce another modified image from the socket camera after the
     * current image has been consumed.
     */
    private Thread mPreviewThread = new Thread() {

        public void run() {
            Log.d(TAG, "Run PreviewThread");

            while (mPreview) {
                Log.d(TAG, "PreviewThread captureBitmap");
                Log.d(TAG, "mOpenCVAction in PreviewThread = " + mOpenCVAction);
                int[] pixels = null;
                int width = DEFAULT_WIDTH;
                int height = DEFAULT_HEIGHT;
                boolean grabFailed = false;
                Bitmap faceDetectBitmap = null;
                if (!mCameraOption.equals(VideoEmulationConfiguration.C_SOCKET_CAMERA)) {
                    faceDetectBitmap = mSocketCamera.captureBitmap();
                    width = faceDetectBitmap.getWidth();
                    height = faceDetectBitmap.getHeight();

                    pixels = new int[width * height];
                    faceDetectBitmap.getPixels(pixels, 0, width, 0, 0, width, height);
                    if (!mOpenCV.setSourceImage(pixels, width, height)) {
                        grabFailed = true;
                        Log.d(TAG, "Error occurred while setting the source image pixels");
                    }
                } else {
                    if (!mOpenCV.grabSourceImageFromCapture()) {
                        grabFailed = true;
                        Log.d(TAG, "Error occurred while grabbing the source image from capture");
                    }
                }

                byte[] data = null;
                if (!grabFailed) {
                    // Perform a findContours, but this could be any OpenCV function
                    // exposed through the JNI.
                    Log.d(TAG, "PreviewThread findFaces");
                    if (mOpenCVAction.equals(VideoEmulationConfiguration.FIND_CONTOURS)) {
                        Log.d(TAG, "Finding Contours");
                        data = mOpenCV.findContours(pixels, width, height);
                    }
                    else {
                        if (mOpenCVAction.equals(VideoEmulationConfiguration.TRACK_ALL_FACES)) {
                            Log.d(TAG, "Tracking All Faces");
                            Rect[] faces = mOpenCV.findAllFaces();
                            if (faces != null && faces.length > 0) {
                                for (int i = 0; i < faces.length; i++) {
                                    Log.d(TAG, "Face #" + i + ": " + faces[i]);
                                }

                                if (!mOpenCV.highlightFaces()) {
                                    Log.d(TAG, "Error occurred while highlighting the detected faces");
                                }
                            } else {
                                Log.d(TAG, "No faces were detected");
                            }
                        } else {
                            Log.d(TAG, "Tracking Single Face");
                            Rect face = mOpenCV.findSingleFace();
                            if (face != null) {
                                Log.d(TAG, "Single Face: " + face);

                                if (!mOpenCV.highlightFaces()) {
                                    Log.d(TAG, "Error occurred while highlighting the detected faces");
                                }
                            } else {
                                Log.d(TAG, "No faces were detected");
                            }
                        }

                        // We grab the source image regardless of faces are found or not.
                        data = mOpenCV.getSourceImage();
                    }
                } else {
                    Log.d(TAG, "Error occurred, no image was grabbed!");
                }

                // Since this is quite a lengthy process, make sure that
                // mPreview is still true before posting the bitmap to the
                // message handler.
                if (!mPreview) {
                    break;
                }

                if (data != null && data.length > 0) {
                    faceDetectBitmap = BitmapFactory.decodeByteArray(data, 0, data.length);
                }

                // If no modified image was generated then just display the
                // original.
                Bitmap bitmapToSend = null;
                if (faceDetectBitmap != null) {
                    Log.d(TAG, "PreviewThread setImageBitmap(faceDetectBitmap)");
                    bitmapToSend = faceDetectBitmap;
                }

                if (bitmapToSend != null) {
                    // Notify the handler that another modified image is on the way.
                    mHandler.sendMessage(mHandler.obtainMessage());

                    try {
                        Log.d(TAG, "PreviewThread Put Bitmap");
                        // Wait here until the image is grabbed.
                        mSyncQueue.put(bitmapToSend);
                    } catch (InterruptedException e1) {
                        Log.d(TAG, "PreviewThread InterruptedException");
                    }
                }
            }
        }
    };

    private Handler mHandler = new Handler() {

        public void handleMessage(Message msg) {
            Bitmap bitmap = null;
            try {
                // Grab the image from the queue or wait until one is available.
                Log.d(TAG, "Handler Take Bitmap");
                bitmap = mSyncQueue.take();
            } catch (InterruptedException e) {
                Log.d(TAG, "Handler InterruptedException");
            }

            // Display the image to the user.
            if (bitmap != null) {
                mImageView.setImageBitmap(bitmap);

                Log.d(TAG, "Handler setContentView");
                setContentView(mImageView, new LayoutParams(FP, FP));
            }

            mActivityMgr.getMemoryInfo(mActivityMgrMemInfo);
            Log.d(TAG, "Available Memory: " + String.valueOf(mActivityMgrMemInfo.availMem));
        }
    };
}
