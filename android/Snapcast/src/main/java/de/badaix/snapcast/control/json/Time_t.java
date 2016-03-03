package de.badaix.snapcast.control.json;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * Created by johannes on 06.01.16.
 */
public class Time_t implements JsonSerialisable {
    private long sec;
    private long usec;

    public Time_t(JSONObject json) {
        fromJson(json);
    }

    public Time_t(long sec, long usec) {
        this.sec = sec;
        this.usec = usec;
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
}
