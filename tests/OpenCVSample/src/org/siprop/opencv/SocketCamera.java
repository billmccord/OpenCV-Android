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

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Bitmap.CompressFormat;
import android.hardware.Camera;
import android.hardware.Camera.Parameters;
import android.hardware.Camera.PictureCallback;
import android.hardware.Camera.PreviewCallback;
import android.hardware.Camera.ShutterCallback;
import android.location.Location;
import android.location.LocationManager;
import android.location.LocationProvider;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.preference.PreferenceManager;
import android.util.Config;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.ViewGroup;
import android.widget.Toast;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.InetSocketAddress;
import java.net.Socket;


/**
 * It is implementing of CameraIF for G1.
 *
 * @author Bill McCord
 *
 */
public class SocketCamera implements CameraIF {

	private static final String TAG = "G1Camera";

	private PreviewCallback cb = null;
    private long mRawPictureCallbackTime;


    public static final int SCREEN_DELAY = 2 * 60 * 1000;

    public static final String ACTION_IMAGE_CAPTURE = "ACTION_IMAGE_CAPTURE";



    private SharedPreferences mPreferences;

    public static final int IDLE = 1;
    public static final int SNAPSHOT_IN_PROGRESS = 2;
    public static final int SNAPSHOT_COMPLETED = 3;

    private int mStatus = IDLE;

    private OpenCVSample mMainActivity;
    private SurfaceView mSurfaceView;
    private SurfaceHolder mSurfaceHolder = null;

	private int mViewFinderWidth, mViewFinderHeight;
	private boolean mPreviewing = false;

	private ImageCapture mCaptureObject;
	private ImageCapture mImageCapture = null;


	private boolean mPausing = false;

	private LocationManager mLocationManager = null;

    private JpegPictureCallback mJpegPictureCallback = new JpegPictureCallback();

    private boolean mKeepAndRestartPreview;

    private Handler mHandler = null;

    public enum DataLocation { NONE, INTERNAL, EXTERNAL, ALL }


    private static final int SOCKET_TIMEOUT = 1000;

    private final String address;
    private final int port;



    private LocationListener [] mLocationListeners = new LocationListener[] {
            new LocationListener(LocationManager.GPS_PROVIDER),
            new LocationListener(LocationManager.NETWORK_PROVIDER)
    };

	private JpegShutterCallback shutterCallback = new JpegShutterCallback();
    private final class JpegShutterCallback implements ShutterCallback {
		public void onShutter() {
			restartPreview();
		}
    }


	public SocketCamera(OpenCVSample mMainActivity, SurfaceView mSurfaceView, String address, int port, int width, int height) {
		Log.d("G1Camera", "instance");

		this.mMainActivity = mMainActivity;
        mPreferences = PreferenceManager.getDefaultSharedPreferences(mMainActivity);
        mLocationManager = (LocationManager) mMainActivity.getSystemService(Context.LOCATION_SERVICE);

	    this.mSurfaceView = mSurfaceView;
        SurfaceHolder holder = mSurfaceView.getHolder();
        holder.addCallback(mMainActivity);
        holder.setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);

        // resize the SurfaceView to the aspect-ratio of the still image
        // and so that we can see the full image that was taken
        ViewGroup.LayoutParams lp = mSurfaceView.getLayoutParams();
        if (width*height < width*height) {
            lp.width = (width * height) / height;
        } else {
            lp.height = (height * width) / width;
        }
        mSurfaceView.requestLayout();

        this.address = address;
        this.port = port;

		mHandler = mMainActivity.getMessageHandler();
	}


	public Parameters getParameters() {
        // Do Nothing
		return null;
	}

	public void setParameters(Parameters params) {
		// Do Nothing
	}

	public void setPreviewCallback(PreviewCallback cb) {
		this.cb = cb;

	}

	public void onStart() {
        // Do Nothing
	}

	public void onDestroy() {
        // Do Nothing
	}

	public void resetPreviewSize(int width, int height) {
        // Do Nothing
	}

    public void onResume() {
		Log.d("onResume", "in onResume.");

        mHandler.sendEmptyMessageDelayed(OpenCVSample.CLEAR_SCREEN_DELAY, SCREEN_DELAY);

        mPausing = false;

        ensureCameraDevice();
        mImageCapture = new ImageCapture(mMainActivity, mHandler);

        restartPreview();

        if (mPreferences.getBoolean("pref_camera_recordlocation_key", false))
            startReceivingLocationUpdates();
    }

    public void onStop() {
        keep();
        stopPreview();
        closeCamera();
        mHandler.removeMessages(OpenCVSample.CLEAR_SCREEN_DELAY);
    }

    public void onPause() {
        keep();

        mPausing = true;
        stopPreview();

        if (!mImageCapture.getCapturing()) {
            closeCamera();
        }
        stopReceivingLocationUpdates();

    }

    public void surfaceChanged(SurfaceHolder holder, int format, int w, int h) {
        // if we're creating the surface, start the preview as well.
        boolean preview = holder.isCreating();
        setViewFinder(w, h, preview);
        mCaptureObject = mImageCapture;
    }

    public void surfaceCreated(SurfaceHolder holder) {
        mSurfaceHolder = holder;
    }

    public void surfaceDestroyed(SurfaceHolder holder) {
        stopPreview();
        mSurfaceHolder = null;
    }


	public void handleMessage(Message msg) {
        switch (msg.what) {
            case OpenCVSample.KEEP: {
                keep();

                mKeepAndRestartPreview = true;

                if (msg.obj != null) {
                    mHandler.post((Runnable)msg.obj);
                }
                break;
            }

            case OpenCVSample.RESTART_PREVIEW: {
                if (mStatus == SNAPSHOT_IN_PROGRESS) {
                    // We are still in the processing of taking the picture, wait.
                    // This is is strange.  Why are we polling?
                    // TODO remove polling
                    mHandler.sendEmptyMessageDelayed(OpenCVSample.RESTART_PREVIEW, 100);
                } else if (mStatus == SNAPSHOT_COMPLETED){
                    mCaptureObject.dismissFreezeFrame(true);
                }
                break;
            }
        }
	}




//----------------------- Camera's method ------------------------

    private void closeCamera() {
        // Do Nothing
    }

    private boolean ensureCameraDevice() {
        return true;
    }

    public void restartPreview() {
		Log.d("restartPreview", "in restartPreview.");

        SurfaceView surfaceView = mSurfaceView;
        if (surfaceView == null ||
                surfaceView.getWidth() == 0 || surfaceView.getHeight() == 0) {
    		Log.d("restartPreview", "surfaceView = null");
            return;
        }
        setViewFinder(mViewFinderWidth, mViewFinderHeight, true);
        mStatus = IDLE;
    }

    private void setViewFinder(int w, int h, boolean startPreview) {
		Log.d("setViewFinder", "in setViewFinder.");
		Log.d("setViewFinder", "w=" + w + "  h=" + h);
        if (mPausing) {
    		Log.d("setViewFinder", "in mPausing.");
            return;
        }

        if (mPreviewing &&
                w == mViewFinderWidth &&
                h == mViewFinderHeight) {
    		Log.d("setViewFinder", "in no change size.");
            return;
        }

        if (!ensureCameraDevice()) {
    		Log.d("setViewFinder", "in ensureCameraDevice.");
            return;
        }

        if (mSurfaceHolder == null) {
    		Log.d("setViewFinder", "in mSurfaceHolder is null.");
            return;
        }

        if (mMainActivity.isFinishing()) {
    		Log.d("setViewFinder", "in fihishing.");
            return;
        }

        if (mPausing) {
    		Log.d("setViewFinder", "in mPausing.");
            return;
        }

        // remember view finder size
        mViewFinderWidth = w;
        mViewFinderHeight = h;

        if (startPreview == false) {
    		Log.d("setViewFinder", "in startPreview = false.");
            return;
        }

        /*
         * start the preview if we're asked to...
         */

        // we want to start the preview and we're previewing already,
        // stop the preview first (this will blank the screen).
        if (mPreviewing)
            stopPreview();

//        final long wallTimeStart = SystemClock.elapsedRealtime();
//        final Object watchDogSync = new Object();
//        Thread watchDog = new WatchDogThread(watchDogSync, wallTimeStart);
//        watchDog.start();

		Log.d("setViewFinder", "start mCameraDevice.startPreview().");
		mPreviewing = true;
		startPreview();

//        synchronized (watchDogSync) {
//            watchDogSync.notify();
//        }
    }

    public void takePicture() {
		// Take picture.
		if (!mPreviewing) {
			Log.d("CaptureThread", "in onSnap");
			mCaptureObject.snap(mImageCapture);
		}
    }

    private void startPreview() {
        Log.d("startPreview", "in startPreview.");
        new Thread() {
            public void run() {
                while (mPreviewing) {
                    mImageCapture.capture();
                }
            }
        }.start();
        Log.d("startPreview", "out startPreview.");
    }

    private void stopPreview() {
    	Log.d("stopPreview", "in stopPreview.");
        mPreviewing = false;
    	Log.d("startPreview", "out stopPreview.");
    }

    void keep() {
        if (mCaptureObject != null) {
            mCaptureObject.dismissFreezeFrame(true);
        }
    };



    private void startReceivingLocationUpdates() {
        if (mLocationManager != null) {
            try {
                mLocationManager.requestLocationUpdates(
                        LocationManager.NETWORK_PROVIDER,
                        1000,
                        0F,
                        mLocationListeners[1]);
            } catch (java.lang.SecurityException ex) {
                // ok
            } catch (IllegalArgumentException ex) {
                if (Config.LOGD) {
                    Log.d(TAG, "provider does not exist " + ex.getMessage());
                }
            }
            try {
                mLocationManager.requestLocationUpdates(
                        LocationManager.GPS_PROVIDER,
                        1000,
                        0F,
                        mLocationListeners[0]);
            } catch (java.lang.SecurityException ex) {
                // ok
            } catch (IllegalArgumentException ex) {
                if (Config.LOGD) {
                    Log.d(TAG, "provider does not exist " + ex.getMessage());
                }
            }
        }
    }

    private void stopReceivingLocationUpdates() {
        if (mLocationManager != null) {
            for (int i = 0; i < mLocationListeners.length; i++) {
                try {
                    mLocationManager.removeUpdates(mLocationListeners[i]);
                } catch (Exception ex) {
                    // ok
                }
            }
        }
    }

    public Location getCurrentLocation() {
        Location l = null;

        // go in worst to best order
        for (int i = 0; i < mLocationListeners.length && l == null; i++) {
            l = mLocationListeners[i].current();
        }
        return l;
    }
    private class LocationListener implements android.location.LocationListener {
    	private Location mLastLocation;
    	private boolean mValid = false;
    	private String mProvider;

        public LocationListener(String provider) {
            mProvider = provider;
            mLastLocation = new Location(mProvider);
        }

        public void onLocationChanged(Location newLocation) {
            if (newLocation.getLatitude() == 0.0 && newLocation.getLongitude() == 0.0) {
                // Hack to filter out 0.0,0.0 locations
                return;
            }
            mLastLocation.set(newLocation);
            mValid = true;
        }

        public void onProviderEnabled(String provider) {
        }

        public void onProviderDisabled(String provider) {
            mValid = false;
        }

        public void onStatusChanged(String provider, int status, Bundle extras) {
            if (status == LocationProvider.OUT_OF_SERVICE) {
                mValid = false;
            }
        }

        public Location current() {
            return mValid ? mLastLocation : null;
        }
    };


// ---------------------------- Callback classes ---------------------------------

    private final class JpegPictureCallback implements PictureCallback {
        public void onPictureTaken(byte [] jpegData, Camera camera) {
            Log.d("JpegPictureCallback", "in JpegPictureCallback...");

            mStatus = SNAPSHOT_COMPLETED;

            if (!mPreferences.getBoolean("pref_camera_postpicturemenu_key", true)) {
                if (mKeepAndRestartPreview) {
                    long delay = 1500 - (System.currentTimeMillis() - mRawPictureCallbackTime);
                    mHandler.sendEmptyMessageDelayed(OpenCVSample.RESTART_PREVIEW, Math.max(delay, 0));
                }
                Log.d("JpegPictureCallback", "end...pref_camera_postpicturemenu_key:");
                return;
            }

            if (mKeepAndRestartPreview) {
                mKeepAndRestartPreview = false;

                // Post this message so that we can finish processing the request. This also
                // prevents the preview from showing up before mPostPictureAlert is dismissed.
                mHandler.sendEmptyMessage(OpenCVSample.RESTART_PREVIEW);
            }


        	if(jpegData != null) {
				Log.d("JpegPictureCallback", "data= OK");
				cb.onPreviewFrame(jpegData, null);
        	} else {
				try {
					// The measure against over load.
					Thread.sleep(500);
				} catch (InterruptedException e) {
					;
				}
        	}
        }
    };


// ---------------------- ImageCapture Class ---------------------------

	public class ImageCapture {

		private OpenCVSample arActivity;
		private Handler mHandler;


	    public ImageCapture(OpenCVSample arActivity, Handler mHandler) {
	    	this.arActivity = arActivity;
	    	this.mHandler = mHandler;
	    }

	    private boolean mCapturing = false;
	    public boolean getCapturing() {
	    	return mCapturing;
	    }

	    /**
	     * This method sets whether or not we are capturing a picture. This method must be called
	     * with the ImageCapture.this lock held.
	     */
	    public void setCapturingLocked(boolean capturing) {
	        mCapturing = capturing;
	    }

	    public void dismissFreezeFrame(boolean keep) {
	        if (!keep) {
	            Toast.makeText(arActivity, "R.string.camera_tossing", Toast.LENGTH_SHORT).show();
	        }

	        if (mStatus == SNAPSHOT_IN_PROGRESS) {
	            // If we are still in the process of taking a picture, then just post a message.
	            mHandler.sendEmptyMessage(OpenCVSample.RESTART_PREVIEW);
	        } else {
	        	restartPreview();
	        }
	    }

	    /*
	     * Initiate the capture of an image.
	     */
	    public boolean initiate() {
			Log.d("ImageCap", "in initiate.");

	        mCapturing = true;

	        return capture();
	    }

	    private boolean capture() {
			Log.d("ImageCap", "in capture.");
	    	mPreviewing = false;

	    	shutterCallback.onShutter();
	        Socket socket = null;
	        try {
	            Log.d("SocketCamera", "Making socket connection");
	            socket = new Socket();
	            socket.bind(null);
	            socket.setSoTimeout(SOCKET_TIMEOUT);
	            socket.connect(new InetSocketAddress(address, port), SOCKET_TIMEOUT);

	            //obtain the bitmap
	            Log.d("SocketCamera", "Getting InputStream");
	            InputStream in = socket.getInputStream();
	            Bitmap bitmap = BitmapFactory.decodeStream(in);
	            Log.d("ImageCap", "exec takePicture.");
	            ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream();
	            bitmap.compress(CompressFormat.JPEG, 100, byteArrayOutputStream);
	            mJpegPictureCallback.onPictureTaken(byteArrayOutputStream.toByteArray(), null);

	        } catch (RuntimeException e) {
	            Log.i("SocketCamera", "Failed to obtain image over network", e);
	            return false;
	        } catch (IOException e) {
	            Log.i("SocketCamera", "Failed to obtain image over network", e);
	            return false;
	        } finally {
	            try {
	                socket.close();
	            } catch (IOException e) {
	                /* ignore */
	            }
	        }

	        Log.d("SocketCamera", "Returning");

	        return true;
	    }

	    public boolean snap(ImageCapture realCapure) {
			Log.d("ImageCap", "in onSnap");
	        // If we are already in the middle of taking a snapshot then we should just save
	        // the image after we have returned from the camera service.
	        if (mStatus == SNAPSHOT_IN_PROGRESS
	        		|| mStatus == SNAPSHOT_COMPLETED) {
	    		Log.d("ImageCap", "in SNAPSHOT_IN_PROGRESS.");
	            mKeepAndRestartPreview = true;
	            mHandler.sendEmptyMessage(OpenCVSample.RESTART_PREVIEW);
	            return false;
	        }

	        mStatus = SNAPSHOT_IN_PROGRESS;

	        mKeepAndRestartPreview = !mPreferences.getBoolean("pref_camera_postpicturemenu_key", true);

	        return realCapure.initiate();
	    }
	}

}
