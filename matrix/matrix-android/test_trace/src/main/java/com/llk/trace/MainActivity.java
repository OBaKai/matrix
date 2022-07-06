package com.llk.trace;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.os.SystemClock;
import android.view.View;

public class MainActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
    }

    public void test_anr(View view) {
        A();
    }



    private void A() {
        B();
        H();
        L();
        SystemClock.sleep(800);
    }

    private void B() {
        C();
        G();
        SystemClock.sleep(200);
    }

    private void C() {
        D();
        E();
        F();
        SystemClock.sleep(100);
    }

    private void D() {
        SystemClock.sleep(20);
    }

    private void E() {
        SystemClock.sleep(20);
    }

    private void F() {
        SystemClock.sleep(20);
    }

    private void G() {
        SystemClock.sleep(20);
    }

    private void H() {
        SystemClock.sleep(20);
        I();
        J();
        K();
    }

    private void I() {
        SystemClock.sleep(20);
    }

    private void J() {
        SystemClock.sleep(6);
    }

    private void K() {
        SystemClock.sleep(10);
    }


    private void L() {
        SystemClock.sleep(10000);
    }

    public void test_anr_2(View view) {
        try {
            Thread.sleep(20000);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }
}