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
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.preference.PreferenceManager;

/**
 * Created by johannes on 21.02.16.
 */
public class Settings {
    private static Settings instance = null;
    private Context ctx = null;

    public static Settings getInstance(Context context) {
        if (instance == null) {
            instance = new Settings();
        }
        if (context != null)
            instance.ctx = context.getApplicationContext();

        return instance;
    }

    public Context getContext() {
        return ctx;
    }

    public Resources getResources() {
        return ctx.getResources();
    }

    public SharedPreferences getPrefs() {
        return PreferenceManager.getDefaultSharedPreferences(ctx);
    }

    public Settings put(String key, String value) {
        SharedPreferences.Editor editor = getPrefs().edit();
        editor.putString(key, value);
        editor.commit();
        return this;
    }

    public Settings put(String key, boolean value) {
        SharedPreferences.Editor editor = getPrefs().edit();
        editor.putBoolean(key, value);
        editor.commit();
        return this;
    }

    public Settings put(String key, float value) {
        SharedPreferences.Editor editor = getPrefs().edit();
        editor.putFloat(key, value);
        editor.commit();
        return this;
    }

    public Settings put(String key, int value) {
        SharedPreferences.Editor editor = getPrefs().edit();
        editor.putInt(key, value);
        editor.commit();
        return this;
    }

    public Settings put(String key, long value) {
        SharedPreferences.Editor editor = getPrefs().edit();
        editor.putLong(key, value);
        editor.commit();
        return this;
    }

    public String getString(String key, String defaultValue) {
        return getPrefs().getString(key, defaultValue);
    }

    public boolean getBoolean(String key, boolean defaultValue) {
        return getPrefs().getBoolean(key, defaultValue);
    }

    public float getFloat(String key, float defaultValue) {
        return getPrefs().getFloat(key, defaultValue);
    }

    public int getInt(String key, int defaultValue) {
        return getPrefs().getInt(key, defaultValue);
    }

    public long getLong(String key, long defaultValue) {
        return getPrefs().getLong(key, defaultValue);
    }

    public String getHost() {
        return getString("host", "");
    }

    public int getStreamPort() {
        return getInt("streamPort", 1704);
    }

    public int getControlPort() {
        return getInt("controlPort", getStreamPort() + 1);
    }

    public boolean isAutostart() {
        return getBoolean("autoStart", false);
    }

    public void setAutostart(boolean autoStart) {
        put("autoStart", autoStart);
    }

    public void setHost(String host, int streamPort, int controlPort) {
        put("host", host);
        put("streamPort", streamPort);
        put("controlPort", controlPort);
    }
}
