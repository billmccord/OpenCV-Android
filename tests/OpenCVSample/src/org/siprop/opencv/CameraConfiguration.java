
package org.siprop.opencv;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.CompoundButton.OnCheckedChangeListener;

/**
 * Simple class for configuring which camera to use.
 *
 * @author Bill McCord
 *
 */
public final class CameraConfiguration extends Activity {

    public static final String EXTRA_IS_REMOTE_CAMERA = "IS_REMOTE_CAMERA";

    public static final String EXTRA_REMOTE_CAMERA_ADDRESS = "REMOTE_CAMERA_ADDRESS";

    public static final String EXTRA_REMOTE_CAMERA_PORT = "REMOTE_CAMERA_PORT";

    private static final String DEFAULT_REMOTE_CAMERA = "192.168.0.102:9889";

    private boolean mUseRemoteCamera = false;

    public void onCreate(Bundle bundle) {
        super.onCreate(bundle);

        // init Camera.
        setContentView(R.layout.config);

        CheckBox remoteCameraCheck = (CheckBox)findViewById(R.id.RemoteCameraCheckBox);
        remoteCameraCheck.setOnCheckedChangeListener(onRemoteCameraCheckChange);

        Button restoreDefaultCameraButton = (Button)findViewById(R.id.ResotreDefaultCameraButton);
        restoreDefaultCameraButton.setOnClickListener(onRestoreDefaultCameraClick);

        Button doneButton = (Button)findViewById(R.id.DoneButton);
        doneButton.setOnClickListener(onDoneClick);
    }

    private OnCheckedChangeListener onRemoteCameraCheckChange = new OnCheckedChangeListener() {
        public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
            mUseRemoteCamera = isChecked;
        }
    };

    private OnClickListener onRestoreDefaultCameraClick = new OnClickListener() {
        public void onClick(View arg0) {
            ((EditText)findViewById(R.id.RemoteSourceEditText)).setText(DEFAULT_REMOTE_CAMERA);
        }
    };

    private OnClickListener onDoneClick = new OnClickListener() {
        public void onClick(View arg0) {
            boolean skipActivity = false;
            String address = null;
            int port = 0;
            if (mUseRemoteCamera) {
                String remoteCameraStr = ((EditText)findViewById(R.id.RemoteSourceEditText))
                        .getText().toString();
                String[] remoteCameraStrArr = remoteCameraStr.split(":");
                if (remoteCameraStrArr.length == 2) {
                    address = remoteCameraStrArr[0];
                    port = Integer.parseInt(remoteCameraStrArr[1]);
                } else {
                    skipActivity = true;
                }
            }

            if (!skipActivity) {
                Intent invoker = new Intent();
                invoker.putExtra(EXTRA_IS_REMOTE_CAMERA, mUseRemoteCamera);

                if (mUseRemoteCamera) {
                    invoker.putExtra(EXTRA_REMOTE_CAMERA_ADDRESS, address);
                    invoker.putExtra(EXTRA_REMOTE_CAMERA_PORT, port);
                }

                invoker.setClass(getBaseContext(), OpenCVSample.class);
                startActivity(invoker);

                finish();
            }
        }
    };
}
