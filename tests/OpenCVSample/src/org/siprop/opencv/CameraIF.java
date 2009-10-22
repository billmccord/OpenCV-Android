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

import android.hardware.Camera.Parameters;
import android.hardware.Camera.PreviewCallback;
import android.os.Message;
import android.view.SurfaceHolder;

/**
 * It is an interface for cameras.
 *
 * @author noritsuna
 *
 */
public interface CameraIF {

    public void setPreviewCallback(PreviewCallback cb);

    public void setParameters(Parameters params);
    public Parameters getParameters();

    public void resetPreviewSize(int width, int height);


    public void onStart();
    public void onResume();
    public void onStop();
    public void onPause();
	public void onDestroy();


	public void takePicture();

	public void handleMessage(Message msg);

	public void surfaceChanged(SurfaceHolder holder, int format, int w, int h);

	public void surfaceCreated(SurfaceHolder holder);

    public void surfaceDestroyed(SurfaceHolder holder);
}
