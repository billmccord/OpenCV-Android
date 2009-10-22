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
import android.hardware.Camera;
import android.hardware.Camera.Parameters;
import android.hardware.Camera.PictureCallback;
import android.hardware.Camera.PreviewCallback;
import android.hardware.Camera.ShutterCallback;
import android.hardware.Camera.Size;
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


/**
 * It is implementing of CameraIF for G1. 
 * 
 * @author noritsuna
 *
 */
public class Dev1Camera implements CameraIF {
	
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

    private Camera mCameraDevice;
    
    private OpenCVSample mMainActivity;
    private SurfaceView mSurfaceView;
    private SurfaceHolder mSurfaceHolder = null;

	private int mViewFinderWidth, mViewFinderHeight;
	private boolean mPreviewing = false;

	private ImageCapture mCaptureObject;
	private ImageCapture mImageCapture = null;

	
	private boolean mPausing = false;

	private boolean mIsFocusing = false;
	private boolean mIsFocused = false;
	private boolean mCaptureOnFocus = false;

	private LocationManager mLocationManager = null;

    private JpegPictureCallback mJpegPictureCallback = new JpegPictureCallback();
    private AutoFocusCallback mAutoFocusCallback = new AutoFocusCallback();

    private boolean mKeepAndRestartPreview;

    private Handler mHandler = null; 
    
    public enum DataLocation { NONE, INTERNAL, EXTERNAL, ALL }

    
    
    
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


	public Dev1Camera(OpenCVSample mMainActivity, SurfaceView mSurfaceView) {
		Log.d("G1Camera", "instance");
		
		this.mMainActivity = mMainActivity;
        mPreferences = PreferenceManager.getDefaultSharedPreferences(mMainActivity);
        mLocationManager = (LocationManager) mMainActivity.getSystemService(Context.LOCATION_SERVICE);

	    this.mSurfaceView = mSurfaceView;
        SurfaceHolder holder = mSurfaceView.getHolder();
        holder.addCallback(mMainActivity);
        holder.setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);
		
		mHandler = mMainActivity.getMessageHandler();
	}
	

	public Parameters getParameters() {
		return mCameraDevice.getParameters();
	}

	public void setParameters(Parameters params) {
		mCameraDevice.setParameters(params);
	}

	public void setPreviewCallback(PreviewCallback cb) {
		this.cb = cb;
		
	}

	

	
	public void onStart() {
	}

	public void onDestroy() {
		Log.d("AttachedCamera", "call stopPreview");
	}		

	public void resetPreviewSize(int width, int height) {
		// TODO Auto-generated method stub
		
	}
	
	
    public void onResume() {
		Log.d("onResume", "in onResume.");
		
        mHandler.sendEmptyMessageDelayed(OpenCVSample.CLEAR_SCREEN_DELAY, SCREEN_DELAY);
        
        mPausing = false;

        ensureCameraDevice();
        mImageCapture = new ImageCapture(mMainActivity, mHandler, mCameraDevice);

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
    	startPreview();

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
    
    private boolean autoFocus() {
        if (!mIsFocusing) {
            if (mCameraDevice != null) {
                mIsFocusing = true;
                mIsFocused = false;
                mCameraDevice.autoFocus(mAutoFocusCallback);
                return true;
            }
        }
        return false;
    }
    
    private void clearFocus() {
        mIsFocusing = false;
        mIsFocused = false;
    }

    
    private void closeCamera() {
        if (mCameraDevice != null) {
            mCameraDevice.release();
            mCameraDevice = null;
            mPreviewing = false;
        }
    }
    
    private boolean ensureCameraDevice() {
        if (mCameraDevice == null) {
            mCameraDevice = Camera.open();
        }
        return mCameraDevice != null;
    }
    
    public void restartPreview() {
		Log.d("restartPreview", "in restartPreview.");

        SurfaceView surfaceView = mSurfaceView;
        if (surfaceView == null || 
                surfaceView.getWidth() == 0 || surfaceView.getHeight() == 0) {
    		Log.d("restartPreview", "surfaceView = null");
            return;
        }
        // make sure the surfaceview fills the whole screen when previewing
        ViewGroup.LayoutParams lp = surfaceView.getLayoutParams();
        lp.width = ViewGroup.LayoutParams.FILL_PARENT;
        lp.height = ViewGroup.LayoutParams.FILL_PARENT;
        surfaceView.requestLayout();
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
        
        // this blanks the screen if the surface changed, no-op otherwise
        try {
        	mCameraDevice.setPreviewDisplay(mSurfaceHolder);
		} catch (Exception e) {
			e.printStackTrace();
            stopPreview();
            return;
		}
        

        // request the preview size, the hardware may not honor it,
        // if we depended on it we would have to query the size again        
        Parameters parameters = mCameraDevice.getParameters();
        parameters.setPreviewSize(w, h);
        parameters.set("jpeg-quality", 100);
        
        mCameraDevice.setParameters(parameters);
        
        
//        final long wallTimeStart = SystemClock.elapsedRealtime();
//        final Object watchDogSync = new Object();
//        Thread watchDog = new WatchDogThread(watchDogSync, wallTimeStart);
//        watchDog.start();

		Log.d("setViewFinder", "start mCameraDevice.startPreview().");
        mCameraDevice.startPreview();
        mPreviewing = true;    	
        this.startPreview();
        
//        synchronized (watchDogSync) {
//            watchDogSync.notify();
//        }
    }

    
    public void takePicture() {
		// Take picture.
		if (mIsFocused || !mPreviewing) {
			Log.d("CaptureThread", "in onSnap");
			mCaptureObject.snap(mImageCapture);
		    clearFocus();
		} else {
			Log.d("CaptureThread", "in autoFocus");
		    mCaptureOnFocus = true;
		    autoFocus();
		    clearFocus();
		}
    }
    

    
    
    private void startPreview() {
    	Log.d("startPreview", "in startPreview.");
    	Log.d("startPreview", "out startPreview.");
    }
    private void stopPreview() {
    	Log.d("stopPreview", "in stopPreview.");
        if (mCameraDevice != null && mPreviewing) {
            mCameraDevice.stopPreview();
        }
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
    
    private final class AutoFocusCallback implements android.hardware.Camera.AutoFocusCallback {
        public void onAutoFocus(boolean focused, android.hardware.Camera camera) {
            mIsFocusing = false;
            mIsFocused = focused;
            Log.d("AutoFocusCallback", "focused:" + mIsFocused);
            Log.d("AutoFocusCallback", "mCaptureOnFocus:" + mCaptureOnFocus);
            if (focused) {
                if (mCaptureOnFocus && mCaptureObject != null) {
                    // No need to play the AF sound if we're about to play the shutter sound
                    if(mCaptureObject.snap(mImageCapture)) {
                        clearFocus();
                        mCaptureOnFocus = false;
                        return;
                    }
                    clearFocus();
                }
                mCaptureOnFocus = false;
            }
        }
    };

    
        
// ---------------------- ImageCapture Class ---------------------------	
	
	public class ImageCapture {
		
		private OpenCVSample arActivity;	
		private Camera mCameraDevice;
		private Handler mHandler;
		
		
	    public ImageCapture(OpenCVSample arActivity, Handler mHandler, Camera mCameraDevice) {
	    	this.arActivity = arActivity;
	    	this.mHandler = mHandler;
	    	this.mCameraDevice = mCameraDevice;
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
			Log.d("ImageCap", "mCameraDevice ID:" + mCameraDevice);
	        if (mCameraDevice == null) {
	            return false;
	        }
	        
	        mCapturing = true;

	        return capture();
	    }
	  
	    private boolean capture() {
			Log.d("ImageCap", "in capture.");
	    	mPreviewing = false;
	        
	        //final int latchedOrientation = NyARToolkitAndroidActivity.roundOrientation(arActivity.getLastOrientation() + 90 );

	        Boolean recordLocation = mPreferences.getBoolean("pref_camera_recordlocation_key", false);
	        Location loc = recordLocation ? getCurrentLocation() : null;
	        Camera.Parameters parameters = mCameraDevice.getParameters();
	        // Quality 75 has visible artifacts, and quality 90 looks great but the files begin to
	        // get large. 85 is a good compromise between the two.
	        parameters.set("jpeg-quality", 100);
	        parameters.set("rotation", 90);
	        //parameters.set("rotation", latchedOrientation);
	        if (loc != null) {
	            parameters.set("gps-latitude",  String.valueOf(loc.getLatitude()));
	            parameters.set("gps-longitude", String.valueOf(loc.getLongitude()));
	            parameters.set("gps-altitude",  String.valueOf(loc.getAltitude()));
	            parameters.set("gps-timestamp", String.valueOf(loc.getTime()));
	        } else {
	            parameters.remove("gps-latitude");
	            parameters.remove("gps-longitude");
	            parameters.remove("gps-altitude");
	            parameters.remove("gps-timestamp");
	        }

	        Size pictureSize = parameters.getPictureSize();
	        Size previewSize = parameters.getPreviewSize();

	        // resize the SurfaceView to the aspect-ratio of the still image
	        // and so that we can see the full image that was taken
	        ViewGroup.LayoutParams lp = mSurfaceView.getLayoutParams();
	        if (pictureSize.width*previewSize.height < previewSize.width*pictureSize.height) {
	            lp.width = (pictureSize.width * previewSize.height) / pictureSize.height; 
	        } else {
	            lp.height = (pictureSize.height * previewSize.width) / pictureSize.width; 
	        }
	        mSurfaceView.requestLayout();

	        mCameraDevice.setParameters(parameters);
	   
			Log.d("ImageCap", "exec takePicture.");
//	        mCameraDevice.takePicture(shutterCallback, mJpegPictureCallback, null);
	        mCameraDevice.takePicture(shutterCallback, null, mJpegPictureCallback);
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
