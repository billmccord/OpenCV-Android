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

package org.siprop.opencv;

import android.app.Activity;
import android.app.Dialog;
import android.app.ProgressDialog;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.hardware.Camera;
import android.hardware.Camera.PreviewCallback;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.view.KeyEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.view.ViewGroup.LayoutParams;
import android.widget.ImageView;

public class OpenCVSample extends Activity implements View.OnClickListener, SurfaceHolder.Callback {

    public static final int CROP_MSG = 1;

    public static final int KEEP = 2;

    public static final int RESTART_PREVIEW = 3;

    public static final int CLEAR_SCREEN_DELAY = 4;

    public static final int SHOW_LOADING = 5;

    public static final int HIDE_LOADING = 6;

    public static final int MEDIA_LOADING = 7;

    public static final int TAKE_PICT = 8;

    public static final int DEFAULT_SOCKET_CAMERA_IMAGE_WIDTH = 320;

    public static final int DEFAULT_SOCKET_CAMERA_IMAGE_HEIGHT = 240;

    private CameraIF mCameraDevice;

    private PreviewCallback mPreviewCallback = new JpegPreviewCallback();

    private SurfaceView mSurfaceView = null;

    private Handler mHandler = new MainHandler();

    public OpenCV opencv = new OpenCV();

    private Bitmap faceDetectBitmap;

    private final int FP = ViewGroup.LayoutParams.FILL_PARENT;

    private boolean mUseRemoteCamera = false;

    private String mRemoteCameraAddress = null;

    private int mRemoteCameraPort = 0;

    private void loadMedia(Integer mode) {
        changeView(mode);
    }

    private void changeView(int mode) {
        switch (mode) {
            case 1: {
                ImageView imageView = new ImageView(this);
                imageView.setImageBitmap(faceDetectBitmap);

                setContentView(imageView, new LayoutParams(FP, FP));

                Message msg = new Message();
                msg.what = HIDE_LOADING;
                msg.obj = 1;
                mHandler.sendMessage(msg);
                break;
            }
        }
    }

    // ----------------------- Override Methods ------------------------

    /** Called with the activity is first created. */
    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);

        // init Camera.
        setContentView(R.layout.main);
        mSurfaceView = (SurfaceView)findViewById(R.id.camera_preview);

        Bundle extras = getIntent().getExtras();
        mUseRemoteCamera = extras.getBoolean(CameraConfiguration.EXTRA_IS_REMOTE_CAMERA);
        if (!mUseRemoteCamera) {
            mCameraDevice = new Dev1Camera(this, mSurfaceView);
        } else {
            mRemoteCameraAddress = extras
                    .getString(CameraConfiguration.EXTRA_REMOTE_CAMERA_ADDRESS);
            mRemoteCameraPort = extras.getInt(CameraConfiguration.EXTRA_REMOTE_CAMERA_PORT);
            mCameraDevice = new SocketCamera(this, mSurfaceView, mRemoteCameraAddress,
                    mRemoteCameraPort, DEFAULT_SOCKET_CAMERA_IMAGE_WIDTH,
                    DEFAULT_SOCKET_CAMERA_IMAGE_HEIGHT);
        }

        mCameraDevice.setPreviewCallback(mPreviewCallback);

    }

    @Override
    public void onStart() {
        super.onStart();
        mCameraDevice.onStart();
    }

    @Override
    public void onResume() {
        super.onResume();
        Log.d("Main", "onResume");

        mCameraDevice.onResume();
    }

    @Override
    public void onStop() {
        mCameraDevice.onStop();
        super.onStop();
    }

    @Override
    protected void onPause() {
        mCameraDevice.onPause();
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        mCameraDevice.onDestroy();
        super.onDestroy();
    }

    public void onClick(View v) {
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {

        if (keyCode == KeyEvent.KEYCODE_CAMERA || keyCode == KeyEvent.KEYCODE_DPAD_CENTER) {
            takePicture();
        }
        return super.onKeyDown(keyCode, event);
    }

    public void takePicture() {
        mCameraDevice.takePicture();
    }

    public void surfaceChanged(SurfaceHolder holder, int format, int w, int h) {
        Log.d("Main", "surfaceChanged");
        mCameraDevice.surfaceChanged(holder, format, w, h);
    }

    public void surfaceCreated(SurfaceHolder holder) {
        mCameraDevice.surfaceCreated(holder);
    }

    public void surfaceDestroyed(SurfaceHolder holder) {
        mCameraDevice.surfaceDestroyed(holder);
    }

    // ---------------------------- Callback classes
    // ---------------------------------

    private final class JpegPreviewCallback implements PreviewCallback {

        private boolean isProcessing = false;

        public void onPreviewFrame(byte[] jpegData, Camera camera) {
            Log.d("JpegPreviewCallback", "in JpegPreviewCallback...");

            if (isProcessing) {
                Log.d("JpegPreviewCallback", "isProcessing..");
                return;
            }
            isProcessing = true;
            {
                mCameraDevice.onStop();
                Message msg = new Message();
                msg.what = SHOW_LOADING;
                msg.obj = 1;
                mHandler.sendMessage(msg);
            }

            try {
                BitmapFactory.Options options = new BitmapFactory.Options();
                options.inSampleSize = 4;
                Bitmap bitmap = BitmapFactory
                        .decodeByteArray(jpegData, 0, jpegData.length, options);
                int w = bitmap.getWidth();
                int h = bitmap.getHeight();
                int[] pixels = new int[w * h];
                bitmap.getPixels(pixels, 0, w, 0, 0, w, h);

                byte[] data = opencv.findContours(pixels, w, h);
                faceDetectBitmap = BitmapFactory.decodeByteArray(data, 0, data.length);
                if (faceDetectBitmap == null) {
                    Log.d("JpegPreviewCallback", "bitmapNew is null..");
                    isProcessing = false;
                    return;
                }
                Log.d("JpegPreviewCallback", "bitmapNew is " + faceDetectBitmap.getHeight());

                Message msg = new Message();
                msg.what = MEDIA_LOADING;
                msg.obj = 1;
                mHandler.sendMessage(msg);

            } catch (Exception e) {
                e.printStackTrace();
            }

            isProcessing = false;
        }

    };

    // ---------------------------- Handler classes
    // ---------------------------------

    /**
     * This Handler is used to post message back onto the main thread of the
     * application
     */
    private class MainHandler extends Handler {

        public void handleMessage(Message msg) {
            mCameraDevice.handleMessage(msg);
            switch (msg.what) {
                case KEEP: {
                    if (msg.obj != null) {
                        mHandler.post((Runnable)msg.obj);
                    }
                    break;
                }

                case CLEAR_SCREEN_DELAY: {
                    getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                    break;
                }
                case SHOW_LOADING: {
                    showDialog(DIALOG_LOADING);
                    break;
                }
                case HIDE_LOADING: {
                    try {
                        dismissDialog(DIALOG_LOADING);
                        removeDialog(DIALOG_LOADING);
                    } catch (IllegalArgumentException e) {
                    }
                    break;
                }
                case MEDIA_LOADING: {
                    loadMedia((Integer)msg.obj);
                    break;
                }
            }
        }
    }

    public Handler getMessageHandler() {
        return this.mHandler;
    }

    private static final int DIALOG_LOADING = 0;

    @Override
    protected Dialog onCreateDialog(int id) {
        switch (id) {
            case DIALOG_LOADING: {
                ProgressDialog dialog = new ProgressDialog(this);
                dialog.setMessage("Loading ...");
                // dialog.setIndeterminate(true);
                dialog.setCancelable(false);
                dialog.getWindow().setFlags(WindowManager.LayoutParams.FLAG_BLUR_BEHIND,
                        WindowManager.LayoutParams.FLAG_BLUR_BEHIND);
                return dialog;
            }
            default:
                return super.onCreateDialog(id);
        }
    }

}
