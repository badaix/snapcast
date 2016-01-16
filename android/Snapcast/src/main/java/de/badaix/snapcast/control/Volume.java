package de.badaix.snapcast.control;

import android.os.Parcel;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * Created by johannes on 06.01.16.
 */
public class Volume implements JsonSerialisable {
    public static final Creator<Volume> CREATOR = new Creator<Volume>() {
        @Override
        public Volume createFromParcel(Parcel in) {
            return new Volume(in);
        }

        @Override
        public Volume[] newArray(int size) {
            return new Volume[size];
        }
    };
    private boolean muted;
    private int percent;

    public Volume(JSONObject json) {
        fromJson(json);
    }

    public Volume(int percent, boolean muted) {
        this.percent = percent;
        this.muted = muted;
    }

    protected Volume(Parcel in) {
        muted = in.readByte() != 0;
        percent = in.readInt();
    }

    @Override
    public void fromJson(JSONObject json) {
        try {
            percent = json.getInt("percent");
            muted = json.getBoolean("muted");
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    @Override
    public JSONObject toJson() {
        JSONObject json = new JSONObject();
        try {
            json.put("percent", percent);
            json.put("muted", muted);
        } catch (JSONException e) {
            e.printStackTrace();
        }
        return json;
    }

    public int getPercent() {
        return percent;
    }

    public void setPercent(int percent) {
        this.percent = percent;
    }

    public boolean isMuted() {
        return muted;
    }

    public void setMuted(boolean muted) {
        this.muted = muted;
    }

    @Override
    public String toString() {
        return "Volume{" +
                "muted=" + muted +
                ", percent=" + percent +
                '}';
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;

        Volume volume = (Volume) o;

        if (muted != volume.muted) return false;
        return percent == volume.percent;

    }

    @Override
    public int hashCode() {
        int result = (muted ? 1 : 0);
        result = 31 * result + percent;
        return result;
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeByte((byte) (muted ? 1 : 0));
        dest.writeInt(percent);
    }
}
