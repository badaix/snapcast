package de.badaix.snapcast.control;

import android.os.Parcelable;

import org.json.JSONObject;

/**
 * Created by johannes on 08.01.16.
 */
public interface JsonSerialisable extends Parcelable {
    public void fromJson(JSONObject json);
    public JSONObject toJson();
}
