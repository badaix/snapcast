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
