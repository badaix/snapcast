package de.badaix.snapcast.control;

import android.util.Log;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.net.InetAddress;
import java.net.Socket;

/**
 * Created by johannes on 06.01.16.
 */
public class TcpClient {

    private static final String TAG = "TCP";
    private String mServerMessage;
    // sends message received notifications
    private TcpClientListener mMessageListener = null;
    // while this is true, the server will continue running
    private boolean mRun = false;
    // used to send messages
    private PrintWriter mBufferOut;
    // used to read messages from the server
    private BufferedReader mBufferIn;
    private Thread worker = null;
    private Socket socket = null;
    private String uid;

    /**
     * Constructor of the class. OnMessagedReceived listens for the messages
     * received from server
     */
    public TcpClient(TcpClientListener listener) {
        mMessageListener = listener;
    }

    public boolean isConnected() {
        if (socket == null)
            return false;
        return socket.isConnected();
    }

    /**
     * Sends the message entered by client to the server
     *
     * @param message text entered by client
     */
    public void sendMessage(String message) {
        if (mBufferOut != null) {
            Log.d(TAG, "Sending: " + message);
            mBufferOut.println(message + "\r\n");
            mBufferOut.flush();
        }
    }

    /**
     * Close the connection and release the members
     */
    public void stop() {
        mRun = false;

        if (mBufferOut != null) {
            mBufferOut.flush();
            mBufferOut.close();
        }

        mMessageListener = null;
        mBufferIn = null;
        mBufferOut = null;
        mServerMessage = null;
    }

    public void start(final String host, final int port) {
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                mRun = true;

                try {
                    // here you must put your computer's IP address.
                    InetAddress serverAddr = InetAddress.getByName(host);

                    Log.d(TAG, "Connecting to " + serverAddr.getCanonicalHostName() + ":" + port);

                    // create a socket to make the connection with the server
                    socket = new Socket(serverAddr, port);

                    try {

                        // sends the message to the server
                        mBufferOut = new PrintWriter(new BufferedWriter(
                                new OutputStreamWriter(socket.getOutputStream())), true);

                        // receives the message which the server sends back
                        mBufferIn = new BufferedReader(new InputStreamReader(
                                socket.getInputStream()));

                        if (mMessageListener != null)
                            mMessageListener.onConnected(TcpClient.this);

                        // in this while the client listens for the messages sent by the
                        // server
                        while (mRun) {

                            mServerMessage = mBufferIn.readLine();

                            if (mServerMessage != null && mMessageListener != null) {
                                // call the method messageReceived from MyActivity class
                                mMessageListener.onMessageReceived(TcpClient.this, mServerMessage);
                            }

                        }

                        Log.d(TAG, "Received Message: '" + mServerMessage + "'");

                    } catch (Exception e) {
                        Log.d(TAG, "Error", e);

                    } finally {
                        // the socket must be closed. It is not possible to reconnect to
                        // this socket
                        // after it is closed, which means a new socket instance has to
                        // be created.
                        socket.close();
                        if (mMessageListener != null)
                            mMessageListener.onDisconnected(TcpClient.this);
                    }

                } catch (Exception e) {

                    Log.d(TAG, "Error", e);

                }

            }
        };
        worker = new Thread(runnable);
        worker.start();
    }

    // Declare the interface. The method messageReceived(String message) will
    // must be implemented in the MyActivity
    // class at on asynckTask doInBackground
    public interface TcpClientListener {
        void onMessageReceived(TcpClient tcpClient, String message);

        void onConnected(TcpClient tcpClient);

        void onDisconnected(TcpClient tcpClient);
    }

}

