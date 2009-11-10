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
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.RadioButton;

/**
 * Simple class for configuring which camera to use.
 *
 * @author Bill McCord
 */
public final class VideoEmulationConfiguration extends Activity {

    public static final String EXTRA_IS_REMOTE_CAMERA = "IS_REMOTE_CAMERA";

    public static final String EXTRA_REMOTE_CAMERA_ADDRESS = "REMOTE_CAMERA_ADDRESS";

    public static final String EXTRA_REMOTE_CAMERA_PORT = "REMOTE_CAMERA_PORT";

    public static final String EXTRA_OPENCV_ACTION = "OPENCV_ACTION";

    public static final String EXTRA_CAMERA_OPTION = "CAMERA_OPTION";

    public static final String FIND_CONTOURS = "FIND_CONTOURS";

    public static final String TRACK_ALL_FACES = "TRACK_ALL_FACES";

    public static final String TRACK_SINGLE_FACE = "TRACK_SINGLE_FACE";

    public static final String C_SOCKET_CAMERA = "C_SOCKET_CAMERA";

    public static final String JAVA_SOCKET_CAMERA = "JAVA_SOCKET_CAMERA";

    private static final String DEFAULT_REMOTE_CAMERA = "192.168.0.102:9889";

    private static final String TAG = "VideoEmulationConfiguration";

    public void onCreate(Bundle bundle) {
        super.onCreate(bundle);

        // init Camera.
        setContentView(R.layout.config);

        Button restoreDefaultCameraButton = (Button)findViewById(R.id.ResotreDefaultCameraButton);
        restoreDefaultCameraButton.setOnClickListener(onRestoreDefaultCameraClick);

        Button doneButton = (Button)findViewById(R.id.DoneButton);
        doneButton.setOnClickListener(onDoneClick);
    }

    private OnClickListener onRestoreDefaultCameraClick = new OnClickListener() {
        public void onClick(View arg0) {
            ((EditText)findViewById(R.id.RemoteSourceEditText)).setText(DEFAULT_REMOTE_CAMERA);
        }
    };

    private OnClickListener onDoneClick = new OnClickListener() {
        public void onClick(View arg0) {
            boolean skipActivity = false;
            String address = null;
            String port = null;
            String remoteCameraStr = ((EditText)findViewById(R.id.RemoteSourceEditText)).getText()
                    .toString();
            String[] remoteCameraStrArr = remoteCameraStr.split(":");
            if (remoteCameraStrArr.length == 2) {
                address = remoteCameraStrArr[0];
                port = remoteCameraStrArr[1];
            } else {
                skipActivity = true;
            }

            if (!skipActivity) {
                Intent invoker = new Intent();
                invoker.putExtra(EXTRA_REMOTE_CAMERA_ADDRESS, address);
                invoker.putExtra(EXTRA_REMOTE_CAMERA_PORT, port);

                RadioButton trackAllFacesRadioButton = (RadioButton)findViewById(R.id.TrackAllFacesRadioButton);
                RadioButton findContoursRadioButton = (RadioButton)findViewById(R.id.FindContoursRadioButton);

                if (trackAllFacesRadioButton.isChecked()) {
                    Log.v(TAG, "Track All Faces");
                    invoker.putExtra(EXTRA_OPENCV_ACTION, TRACK_ALL_FACES);
                } else if (findContoursRadioButton.isChecked()) {
                    Log.v(TAG, "Find Contours");
                    invoker.putExtra(EXTRA_OPENCV_ACTION, FIND_CONTOURS);
                } else {
                    Log.v(TAG, "Track Single Face");
                    invoker.putExtra(EXTRA_OPENCV_ACTION, TRACK_SINGLE_FACE);
                }

                CheckBox useCSocketCamera = (CheckBox)findViewById(R.id.UseCSocketCapture);
                if (useCSocketCamera.isChecked()) {
                    invoker.putExtra(EXTRA_CAMERA_OPTION, C_SOCKET_CAMERA);
                } else {
                    invoker.putExtra(EXTRA_CAMERA_OPTION, JAVA_SOCKET_CAMERA);
                }

                // Invoke the VideoEmulation activity after the camera
                // is successfully configured.
                invoker.setClass(getBaseContext(), VideoEmulation.class);
                startActivity(invoker);

                finish();
            }
        }
    };
}
