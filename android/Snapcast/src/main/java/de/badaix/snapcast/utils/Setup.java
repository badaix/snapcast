package de.badaix.snapcast.utils;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.AssetManager;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * Created by johannes on 16.01.16.
 */
public class Setup {

    private static final String TAG = "Setup";

    public static void copyAssets(Context context, String[] fileList) {
        Set<String> fileSet = new HashSet<String>(Arrays.asList(fileList));
        AssetManager assetManager = context.getAssets();
        String[] files = null;
        try {
            files = assetManager.list("");
        } catch (IOException e) {
            Log.e(TAG, "Failed to get asset file list.", e);
        }
        if (files != null) for (String filename : files) {
            if (!fileSet.contains(filename))
                continue;
            InputStream in = null;
            OutputStream out = null;
            try {
                SharedPreferences prefs = context.getSharedPreferences("assets", Context.MODE_PRIVATE);
                in = assetManager.open(filename);
                String md5 = MD5.calculateMD5(in);
                File outFile = new File(context.getFilesDir(), filename);
                Log.d(TAG, "Asset: " + outFile.getAbsolutePath() + ", md5: " + md5);
                if (outFile.exists() && md5.equals(prefs.getString(filename, "")))
                    continue;
                Log.d(TAG, "Copying " + outFile.getAbsolutePath());
                out = new FileOutputStream(outFile);
                copyFile(in, out);
                Runtime.getRuntime().exec("chmod 755 " + outFile.getAbsolutePath()).waitFor();
                prefs.edit().putString(filename, md5).apply();
            } catch (Exception e) {
                Log.e(TAG, "Failed to copy asset file: " + filename, e);
            } finally {
                if (in != null) {
                    try {
                        in.close();
                    } catch (IOException e) {
                        // NOOP
                    }
                }
                if (out != null) {
                    try {
                        out.close();
                    } catch (IOException e) {
                        // NOOP
                    }
                }
            }
        }
    }

    public static void copyFile(InputStream in, OutputStream out) throws IOException {
        byte[] buffer = new byte[1024];
        int read;
        while ((read = in.read(buffer)) != -1) {
            out.write(buffer, 0, read);
        }
    }


}
