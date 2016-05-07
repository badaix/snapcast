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

package de.badaix.snapcast.utils;

import android.content.Context;
import android.content.res.AssetManager;
import android.util.Log;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;

/**
 * Created by johannes on 16.01.16.
 */
public class Setup {

    private static final String TAG = "Setup";

    public static String getProp(String prop, String def) {
        Process process = null;
        try {
            process = new ProcessBuilder()
                    .command("/system/bin/getprop", prop)
                    .redirectErrorStream(true)
                    .start();

            BufferedReader bufferedReader = new BufferedReader(
                    new InputStreamReader(process.getInputStream()));
            String line;
            if ((line = bufferedReader.readLine()) != null) {
                return line;
            }
        } catch (IOException e) {
            e.printStackTrace();
        }

        return def;
    }

    public static boolean copyBinAsset(Context context, String sourceFilename, String destFilename) {
        if (copyAsset(context, "bin/" + getProp("ro.product.cpu.abi", "armeabi") + "/" + sourceFilename, destFilename))
            return true;
        else if (copyAsset(context, "bin/" + getProp("ro.product.cpu.abi2", "armeabi") + "/" + sourceFilename, destFilename))
            return true;
        else if (copyAsset(context, "bin/armeabi/" + sourceFilename, destFilename))
            return true;
        return false;
    }

    public static boolean copyAsset(Context context, String sourceFilename, String destFilename) {
        AssetManager assetManager = context.getAssets();

        InputStream in = null;
        OutputStream out = null;
        try {
            in = assetManager.open(sourceFilename);
            String md5 = MD5.calculateMD5(in);
            File outFile = new File(context.getFilesDir(), destFilename);
            Log.d(TAG, "Asset: " + sourceFilename + " => " + outFile.getAbsolutePath() + ", md5: " + md5);
            String hashKey = "asset_" + sourceFilename;
            if (outFile.exists() && md5.equals(Settings.getInstance(context).getString(hashKey, "")))
                return true;
            Log.d(TAG, "Copying " + sourceFilename + " => " + outFile.getAbsolutePath());
            out = new FileOutputStream(outFile);
            copyFile(in, out);
            Runtime.getRuntime().exec("chmod 755 " + outFile.getAbsolutePath()).waitFor();
            Settings.getInstance(context).put(hashKey, md5);
            return true;
        } catch (Exception e) {
            Log.e(TAG, "Failed to copy asset file: " + sourceFilename, e);
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
        return false;
    }

    public static void copyAssets(Context context, String sourceDir, String destDir) {
        AssetManager assetManager = context.getAssets();
        String[] files = null;
        try {
            files = assetManager.list(sourceDir);
            if (files == null)
                return;
        } catch (IOException e) {
            Log.e(TAG, "Failed to get asset file list.", e);
        }

        for (String file : files) {
            String sourceFile = sourceDir + "/" + file;
            String destFile = destDir + "/" + file;
            copyAsset(context, sourceFile, destFile);
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
