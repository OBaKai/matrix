package com.llk.trace;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.SystemClock;
import android.util.Log;
import android.view.View;
import android.widget.TextView;

import com.tencent.matrix.trace.tracer.SignalAnrTracer;
import com.tencent.matrix.trace.util.Utils;
import com.tencent.matrix.util.MatrixLog;

import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStreamReader;

public class MainActivity extends AppCompatActivity {

    private static TextView textView;

    private static Handler handler = new Handler(Looper.getMainLooper()){
        @Override
        public void handleMessage(@NonNull Message msg) {
            super.handleMessage(msg);
            textView.setText((String)msg.obj);
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        textView = findViewById(R.id.tv);
    }

    public void make_anr(View view) {
        SignalAnrTracer.printTrace();
        try {
            Thread.sleep(20000);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    //native反调用的方法，执行在子线程（Signal Catcher）的
    //但是也要开个线程干活，别堵人家干活啊啊啊啊啊啊啊啊啊啊啊啊
    @Keep
    private static void onAnrTraceWriteFinish() {
        Log.e("llk", "onAnrTraceWriteFinish");
        new Thread(() -> {
            String s = getFileContent(App.anrTraceFilePath);
            Message msg = Message.obtain();
            msg.obj = s;
            handler.sendMessage(msg);
        }).start();
    }

    public static String getFileContent(String filePath){
        BufferedReader reader = null;
        try {
            reader = new BufferedReader(new InputStreamReader(new FileInputStream(filePath), "UTF-8"));
            StringBuilder sb = new StringBuilder();
            String line;
            while ((line = reader.readLine()) != null) {
                sb.append(line).append("\n");
            }
            return sb.toString();
        } catch (Throwable t) {
            if (null != reader) {
                try {
                    reader.close();
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        } finally {
            if (null != reader) {
                try {
                    reader.close();
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }

        return null;
    }
}