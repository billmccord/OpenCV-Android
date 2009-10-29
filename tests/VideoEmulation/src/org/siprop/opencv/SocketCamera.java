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

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.util.Log;

import java.io.IOException;
import java.io.InputStream;
import java.net.InetSocketAddress;
import java.net.Socket;

/**
 * This simple socket camera reads images from a socket connection. It is
 * assumed that the server providing the socket is streaming video from an
 * actual camera.
 *
 * @author Bill McCord
 */
public class SocketCamera {

    private static final String TAG = "SocketCamera";

    private static final int SOCKET_TIMEOUT = 1000;

    private final String mAddress;

    private final int mPort;

    /**
     * Create a new SocketCamera.
     *
     * @param address the ip address of the camera server
     * @param port the port that the camera server has opened a socket on
     */
    public SocketCamera(String address, int port) {
        Log.d(TAG, "instance");

        mAddress = address;
        mPort = port;
    }

    /**
     * Capture a bitmap from the camera server over a socket.
     *
     * @return the bitmap captured
     */
    public Bitmap captureBitmap() {
        Log.d(TAG, "Start captureBitmap.");

        Socket socket = null;
        Bitmap bitmap = null;
        try {
            Log.d(TAG, "Making socket connection");
            socket = new Socket();
            socket.bind(null);
            socket.setSoTimeout(SOCKET_TIMEOUT);
            socket.connect(new InetSocketAddress(mAddress, mPort), SOCKET_TIMEOUT);

            // obtain the bitmap
            Log.d(TAG, "Getting InputStream");
            InputStream in = socket.getInputStream();
            Log.d(TAG, "Decode bitmap");
            bitmap = BitmapFactory.decodeStream(in);

        } catch (IOException e) {
            Log.i(TAG, "Failed to obtain image over network", e);
        } finally {
            try {
                socket.close();
            } catch (IOException e) {
                /* ignore */
            }
        }

        Log.d(TAG, "End captureBitmap");
        return bitmap;
    }
}
