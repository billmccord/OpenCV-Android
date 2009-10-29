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

    private static final String TAG = "EmulatorOnlySample";

    private final int FP = ViewGroup.LayoutParams.FILL_PARENT;

    private SocketCamera mSocketCamera;

    private OpenCV mOpenCV = new OpenCV();

    private String mRemoteCameraAddress = null;

    private int mRemoteCameraPort = 0;

    private boolean mPreview = false;

    private ImageView mImageView;

    private SynchronousQueue<Bitmap> mSyncQueue = new SynchronousQueue<Bitmap>();

    private ActivityManager mActivityMgr;

    private ActivityManager.MemoryInfo mActivityMgrMemInfo;

    @Override
    public void onCreate(Bundle bundle) {
        super.onCreate(bundle);

        Log.d(TAG, "onCreate");

        // Initialize Camera.
        setContentView(R.layout.main);
        mImageView = new ImageView(this);

        Bundle extras = getIntent().getExtras();
        mRemoteCameraAddress = extras
                .getString(SocketCameraConfiguration.EXTRA_REMOTE_CAMERA_ADDRESS);
        mRemoteCameraPort = extras.getInt(SocketCameraConfiguration.EXTRA_REMOTE_CAMERA_PORT);
        mSocketCamera = new SocketCamera(mRemoteCameraAddress, mRemoteCameraPort);

        // Let's monitor for memory leaks.
        mActivityMgr = (ActivityManager)super.getSystemService(Context.ACTIVITY_SERVICE);
        mActivityMgrMemInfo = new ActivityManager.MemoryInfo();
    }

    @Override
    public void onStart() {
        super.onStart();

        Log.d(TAG, "onStart");
        mPreview = true;
        mPreviewThread.start();
    }

    @Override
    public void onStop() {
        Log.d(TAG, "onStop");
        mPreview = false;
        try {
            mPreviewThread.join();
        } catch (InterruptedException e) {
            Log.d(TAG, "onStop");
        }

        super.onStop();
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
                Bitmap bitmap = mSocketCamera.captureBitmap();
                int width = bitmap.getWidth();
                int height = bitmap.getHeight();

                int[] pixels = new int[width * height];
                bitmap.getPixels(pixels, 0, width, 0, 0, width, height);

                // Perform a findContours, but this could be any OpenCV function
                // exposed through the JNI.
                Log.d(TAG, "PreviewThread findContours");
                // byte[] data = mOpenCV.findContours(pixels, width, height);
                byte[] data = mOpenCV.findFaces(
                        "/data/data/org.siprop.opencv/files/haarcascade_frontalface_alt.xml",
                        pixels, width, height);
                Bitmap faceDetectBitmap = null;
                if (data != null && data.length > 0) {
                    faceDetectBitmap = BitmapFactory.decodeByteArray(data, 0, data.length);
                }

                // If no modified image was generated then just display the
                // original.
                Bitmap bitmapToSend;
                if (faceDetectBitmap != null) {
                    Log.d(TAG, "PreviewThread setImageBitmap(faceDetectBitmap)");
                    bitmapToSend = faceDetectBitmap;
                } else {
                    Log.d(TAG, "PreviewThread setImageBitmap(bitmap)");
                    bitmapToSend = bitmap;
                }

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
