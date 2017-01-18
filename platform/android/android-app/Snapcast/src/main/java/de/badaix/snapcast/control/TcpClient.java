/*
 *     This file is part of snapcast
 *     Copyright (C) 2014-2016  Johannes Pohl
 *
 *     This program is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation, either version 3 of the License, or
 *     (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

package de.badaix.snapcast.control;

import android.util.Log;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.net.InetAddress;
import java.net.Socket;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

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
    private Thread readerThread = null;
    private Socket socket = null;
    private String uid;
    private BlockingQueue<String> messages = new LinkedBlockingQueue<>();

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
    public void sendMessage(final String message) {
        messages.offer(message);
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

//        mMessageListener = null;
        readerThread.interrupt();
        try {
            readerThread.join(1000);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }

        mBufferIn = null;
        mBufferOut = null;
        mServerMessage = null;
    }

    public void start(final String host, final int port) {
        ReaderRunnable readerRunnable = new ReaderRunnable(host, port);
        readerThread = new Thread(readerRunnable);
        readerThread.start();
    }

    // Declare the interface. The method messageReceived(String message) will
    // must be implemented in the MyActivity
    // class at on asynckTask doInBackground
    interface TcpClientListener {
        void onMessageReceived(TcpClient tcpClient, String message);

        void onConnecting(TcpClient tcpClient);

        void onConnected(TcpClient tcpClient);

        void onDisconnected(TcpClient tcpClient, Exception e);
    }


    private class WriterRunnable implements Runnable {
        @Override
        public void run() {
            while (mRun) {
                try {
                    String message = messages.poll(50, TimeUnit.MILLISECONDS);
                    if ((message != null) && (mBufferOut != null)) {
                        Log.d(TAG, "Sending: " + message);
                        mBufferOut.println(message + "\r\n");
                        mBufferOut.flush();
                    }
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }
        }
    }

    private class ReaderRunnable implements Runnable {
        private String host;
        private int port;

        ReaderRunnable(final String host, final int port) {
            this.host = host;
            this.port = port;
        }

        @Override
        public void run() {
            mRun = true;
            Exception exception = null;
            Thread writerThread = null;

            try {
                if (mMessageListener != null)
                    mMessageListener.onConnecting(TcpClient.this);

                // here you must put your computer's IP address.
                InetAddress serverAddr = InetAddress.getByName(host);

                Log.d(TAG, "Connecting to " + serverAddr.getCanonicalHostName() + ":" + port);

                // create a socket to make the connection with the server
                socket = new Socket(serverAddr, port);


                // sends the message to the server
                mBufferOut = new PrintWriter(new BufferedWriter(
                        new OutputStreamWriter(socket.getOutputStream())), true);

                // receives the message which the server sends back
                mBufferIn = new BufferedReader(new InputStreamReader(
                        socket.getInputStream()));

                if (mMessageListener != null)
                    mMessageListener.onConnected(TcpClient.this);

                writerThread = new Thread(new WriterRunnable());
                writerThread.start();

                // in this while the client listens for the messages sent by the
                // server
                while (mRun) {

                    mServerMessage = mBufferIn.readLine();

                    if (mServerMessage != null) {
                        Log.d(TAG, "Received Message: '" + mServerMessage + "'");
                        if (mMessageListener != null) {
                            mMessageListener.onMessageReceived(TcpClient.this, mServerMessage);
                        }
                    } else {
                        break;
                    }
                }


            } catch (Exception e) {
                Log.d(TAG, "Error", e);
                exception = e;
            } finally {
                // the socket must be closed. It is not possible to reconnect to
                // this socket
                // after it is closed, which means a new socket instance has to
                // be created.
                mRun = false;
                if (writerThread != null) {
                    try {
                        writerThread.interrupt();
                        writerThread.join(100);
                    } catch (Exception e) {
                        e.printStackTrace();
                    }
                }
                if (socket != null) {
                    try {
                        socket.close();
                    } catch (Exception e) {
                    }
                }
                socket = null;
                if (mMessageListener != null)
                    mMessageListener.onDisconnected(TcpClient.this, exception);
            }
        }
    }

}

