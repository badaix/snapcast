package de.badaix.snapcast.control;

import org.json.JSONObject;

/**
 * Created by johannes on 08.01.16.
 */
public interface JsonSerialisable {
    public void fromJson(JSONObject json);

    public JSONObject toJson();
}
