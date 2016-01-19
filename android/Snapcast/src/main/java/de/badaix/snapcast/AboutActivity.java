package de.badaix.snapcast;

import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.webkit.WebView;

public class AboutActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_about);
        WebView wv= (WebView)findViewById(R.id.webView);
        wv.loadUrl("file:///android_asset/" + this.getText(R.string.about_file));
    }
}
