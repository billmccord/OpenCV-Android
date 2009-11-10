import java.awt.image.BufferedImage;
import java.io.BufferedOutputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.net.ServerSocket;
import java.net.Socket;

import javax.imageio.ImageIO;

/**
 * A disposable class that uses JMF to serve a still sequence captured from a
 * webcam over a socket connection. It doesn't use TCP, it just blindly captures
 * a still, JPEG compresses it, and pumps it out over any incoming socket
 * connection.
 *
 * @author Tom Gibara Modified this to use the more generic QTVideoCapture.
 * @author Bill McCord
 */

public class WebcamBroadcaster {

    public static boolean RAW = true;

    public static void main(String[] args) throws Exception {
        int[] values = new int[args.length];
        for (int i = 0; i < values.length; i++) {
            values[i] = Integer.parseInt(args[i]);
        }

        WebcamBroadcaster wb;
        if (values.length == 0) {
            wb = new WebcamBroadcaster();
        } else if (values.length == 1) {
            wb = new WebcamBroadcaster(values[0]);
        } else if (values.length == 2) {
            wb = new WebcamBroadcaster(values[0], values[1]);
        } else {
            wb = new WebcamBroadcaster(values[0], values[1], values[2]);
        }

        wb.start();
    }

    public static final int DEFAULT_PORT = 9889;

    public static final int DEFAULT_WIDTH = 300;

    public static final int DEFAULT_HEIGHT = 300;

    private final Object lock = new Object();

    private final int width;

    private final int height;

    private final int port;

    private boolean running;

    private QTVideoCapture videoCapture;

    private boolean stopping;

    private Worker worker;

    public WebcamBroadcaster(int width, int height, int port) {
        this.width = width;
        this.height = height;
        this.port = port;
    }

    public WebcamBroadcaster(int width, int height) {
        this(width, height, DEFAULT_PORT);
    }

    public WebcamBroadcaster(int port) {
        this(DEFAULT_WIDTH, DEFAULT_HEIGHT, port);
    }

    public WebcamBroadcaster() {
        this(DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_PORT);
    }

    public void start() throws Exception {
        synchronized (lock) {
            if (running)
                return;
            videoCapture = new QTVideoCapture(width, height);
            worker = new Worker();
            worker.start();
            running = true;
        }
    }

    public void stop() throws InterruptedException {
        synchronized (lock) {
            if (!running)
                return;
            if (videoCapture != null) {
                videoCapture.dispose();
                videoCapture = null;
            }
            stopping = true;
            running = false;
            worker = null;
        }
        try {
            worker.join();
        } finally {
            stopping = false;
        }
    }

    private class Worker extends Thread {

        private final int[] data = new int[width * height];

        @Override
        public void run() {
            ServerSocket ss;
            try {
                ss = new ServerSocket(port);

            } catch (IOException e) {
                e.printStackTrace();
                return;
            }

            while (true) {
                synchronized (lock) {
                    if (stopping) {
                        break;
                    }
                }
                Socket socket = null;
                try {
                    socket = ss.accept();

                    BufferedImage image = videoCapture.getNextImage();

                    if (image != null) {
                        OutputStream out = socket.getOutputStream();
                        if (RAW) {
                            image.getWritableTile(0, 0).getDataElements(0, 0, width, height, data);
                            image.releaseWritableTile(0, 0);
                            DataOutputStream dout = new DataOutputStream(new BufferedOutputStream(
                                    out));
                            for (int i = 0; i < data.length; i++) {
                                dout.writeInt(data[i]);
                            }
                            dout.close();
                        } else {
                            ImageIO.write(image, "JPEG", out);
                        }
                    }

                    socket.close();
                    socket = null;
                } catch (IOException e) {
                    e.printStackTrace();
                } catch (Exception e) {
                    // TODO Auto-generated catch block
                    e.printStackTrace();
                } finally {
                    if (socket != null)
                        try {
                            socket.close();
                        } catch (IOException e) {
                            /* ignore */
                        }
                }

            }

            try {
                ss.close();
            } catch (IOException e) {
                e.printStackTrace();
            }
        }

    }

}
