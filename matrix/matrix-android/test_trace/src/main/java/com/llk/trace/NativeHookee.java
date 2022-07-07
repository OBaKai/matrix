package com.llk.trace;

public class NativeHookee {

    static {
        System.loadLibrary("hookee");
    }

    public static void test() {
        nativeTest();
    }

    private static native void nativeTest();
}
