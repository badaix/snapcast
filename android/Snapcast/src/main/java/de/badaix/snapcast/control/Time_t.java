package de.badaix.snapcast.control;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * Created by johannes on 06.01.16.
 */
public class Time_t {
    private int sec;
    private int usec;

    public Time_t(JSONObject json) {
        fromJson(json);
    }

    public Time_t(int sec, int usec) {
        this.sec = sec;
        this.usec = usec;
    }

    public void fromJson(JSONObject json) {
        try {
            sec = json.getInt("sec");
            usec = json.getInt("usec");
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    public int getSec() {
        return sec;
    }

    public int getUsec() {
        return usec;
    }

    public void setSec(int sec) {
        this.sec = sec;
    }

    public void setUsec(int usec) {
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
        int result = sec;
        result = 31 * result + usec;
        return result;
    }
}
