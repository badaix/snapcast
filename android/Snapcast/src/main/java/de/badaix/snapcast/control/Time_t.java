package de.badaix.snapcast.control;

import android.os.Parcel;
import android.os.Parcelable;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * Created by johannes on 06.01.16.
 */
public class Time_t implements JsonSerialisable {
    public static final Creator<Time_t> CREATOR = new Creator<Time_t>() {
        @Override
        public Time_t createFromParcel(Parcel in) {
            return new Time_t(in);
        }

        @Override
        public Time_t[] newArray(int size) {
            return new Time_t[size];
        }
    };
    private long sec;
    private long usec;

    public Time_t(JSONObject json) {
        fromJson(json);
    }

    public Time_t(long sec, long usec) {
        this.sec = sec;
        this.usec = usec;
    }

    protected Time_t(Parcel in) {
        sec = in.readLong();
        usec = in.readLong();
    }

    @Override
    public void fromJson(JSONObject json) {
        try {
            sec = json.getLong("sec");
            usec = json.getLong("usec");
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    @Override
    public JSONObject toJson() {
        JSONObject json = new JSONObject();
        try {
            json.put("sec", sec);
            json.put("usec", usec);
        } catch (JSONException e) {
            e.printStackTrace();
        }
        return json;
    }

    public long getSec() {
        return sec;
    }

    public void setSec(long sec) {
        this.sec = sec;
    }

    public long getUsec() {
        return usec;
    }

    public void setUsec(long usec) {
        this.usec = usec;
    }

    @Override
    public String toString() {
        return "Time_t{" +
                "sec=" + sec +
                ", usec=" + usec +
                '}';
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;

        Time_t time_t = (Time_t) o;

        if (sec != time_t.sec) return false;
        return usec == time_t.usec;

    }

    @Override
    public int hashCode() {
        int result = (int) sec;
        result = (int) (31 * result + usec);
        return result;
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeLong(sec);
        dest.writeLong(usec);
    }
}
