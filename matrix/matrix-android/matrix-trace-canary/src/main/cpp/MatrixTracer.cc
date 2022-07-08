/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: leafjia@tencent.com
//
// MatrixTracer.cpp

#include "MatrixTracer.h"

#include <jni.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <pthread.h>
#include <android/log.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <xhook_ext.h>
#include <linux/prctl.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <stdlib.h>
#include <fcntl.h>

#include <cstdio>
#include <ctime>
#include <csignal>
#include <thread>
#include <memory>
#include <string>
#include <optional>
#include <sstream>
#include <fstream>

#include "Logging.h"
#include "Support.h"
#include "nativehelper/managed_jnienv.h"
#include "nativehelper/scoped_utf_chars.h"
#include "nativehelper/scoped_local_ref.h"
#include "AnrDumper.h"

#include "android_log.h"

#define PROP_VALUE_MAX  92
#define PROP_SDK_NAME "ro.build.version.sdk"
#define HOOK_CONNECT_PATH "/dev/socket/tombstoned_java_trace"
#define HOOK_OPEN_PATH "/data/anr/traces.txt"
#define VALIDATE_RET 50

#define HOOK_REQUEST_GROUPID_ANR_DUMP_TRACE 0x12

using namespace MatrixTracer;
using namespace std;

static std::optional<AnrDumper> sAnrDumper;
static bool isTraceWrite = false;
static bool fromMyPrintTrace = false;
static bool isHooking = false;
static std::string anrTracePathString;
static std::string printTracePathString;
static int signalCatcherTid;

static struct StacktraceJNI {
    jclass AnrDetective;
    jmethodID AnrDetector_onANRDumped;
    jmethodID AnrDetector_onANRDumpTrace;
    jmethodID AnrDetector_onPrintTrace;
    jmethodID AnrDetector_onNativeBacktraceDumped;


    jclass Test_MainActivity;
    jmethodID Test_WriteAnrTraceFinish;
} gJ;


void writeAnr(const std::string& content, const std::string &filePath) {
    //unHookAnrTraceWrite();
    std::string to;
    std::ofstream outfile;
    outfile.open(filePath);
    outfile << content;
}

int (*original_connect)(int __fd, const struct sockaddr* __addr, socklen_t __addr_length);
int my_connect(int __fd, const struct sockaddr* __addr, socklen_t __addr_length) {
    if (__addr!= nullptr) {
        ALOGE("my_connect aaaaaaaaaaa %s", __addr->sa_data);
        if (strcmp(__addr->sa_data, HOOK_CONNECT_PATH) == 0) {
            ALOGE("my_connect aaaaaaaaaaa");
            signalCatcherTid = gettid();
            isTraceWrite = true;
        }
    }
    return original_connect(__fd, __addr, __addr_length);
}

int (*original_open)(const char *pathname, int flags, mode_t mode);

int my_open(const char *pathname, int flags, mode_t mode) {
    if (pathname!= nullptr) {
        ALOGE("my_open aaaaaaaaaaa %s", pathname);
        if (strcmp(pathname, HOOK_OPEN_PATH) == 0) {
            ALOGE("my_open aaaaaaaaaaa");
            signalCatcherTid = gettid();
            isTraceWrite = true;
        }
    }
    return original_open(pathname, flags, mode);
}

ssize_t (*original_write)(int fd, const void* const __pass_object_size0 buf, size_t count);
ssize_t my_write(int fd, const void* const buf, size_t count) {
    ALOGE("my_write aaaaaaaaaaa tid=%d  signalCatcherTid=%d isTraceWrite=%d",  gettid(), signalCatcherTid, isTraceWrite);
    if(isTraceWrite && gettid() == signalCatcherTid) {
        isTraceWrite = false;
        signalCatcherTid = 0;
        if (buf != nullptr) {
            std::string targetFilePath;
            if (fromMyPrintTrace) {
                targetFilePath = printTracePathString;
            } else {
                targetFilePath = anrTracePathString;
            }
            if (!targetFilePath.empty()) {
                char *content = (char *) buf;
                writeAnr(content, targetFilePath);
                if(!fromMyPrintTrace) {
                    anrDumpTraceCallback();
                } else {
                    printTraceCallback();
                }
                call_onAnrTraceWriteFinish();
                fromMyPrintTrace = false;
            }
        }
    }
    return original_write(fd, buf, count);
}

bool anrDumpCallback() {
    JNIEnv *env = JniInvocation::getEnv();
    if (!env) return false;
    env->CallStaticVoidMethod(gJ.AnrDetective, gJ.AnrDetector_onANRDumped);
    return true;
}

bool anrDumpTraceCallback() {
    JNIEnv *env = JniInvocation::getEnv();
    if (!env) return false;
    env->CallStaticVoidMethod(gJ.AnrDetective, gJ.AnrDetector_onANRDumpTrace);
    return true;
}

bool nativeBacktraceDumpCallback() {
    JNIEnv *env = JniInvocation::getEnv();
    if (!env) return false;
    env->CallStaticVoidMethod(gJ.AnrDetective, gJ.AnrDetector_onNativeBacktraceDumped);
    std::string to;
    std::ofstream outfile;
    return true;
}

bool call_onAnrTraceWriteFinish() {
    JNIEnv *env = JniInvocation::getEnv();
    if (!env) return false;
    env->CallStaticVoidMethod(gJ.Test_MainActivity, gJ.Test_WriteAnrTraceFinish);
    return true;
}


bool printTraceCallback() {
    JNIEnv *env = JniInvocation::getEnv();
    if (!env) return false;
    env->CallStaticVoidMethod(gJ.AnrDetective, gJ.AnrDetector_onPrintTrace);
    return true;
}

int getApiLevel() {
    char buf[PROP_VALUE_MAX];
    int len = __system_property_get(PROP_SDK_NAME, buf);
    if (len <= 0)
        return 0;

    return atoi(buf);
}


void hookAnrTraceWrite(bool isSiUser) {
    int apiLevel = getApiLevel();
    if (apiLevel < 19) {
        return;
    }

    if (!fromMyPrintTrace && isSiUser) {
        ALOGE("hookAnrTraceWrite ffffffffff");
        return;
    }

    if (isHooking) {
        ALOGE("hookAnrTraceWrite ffffffffff 2");
        return;
    }

    isHooking = true;

    if (apiLevel >= 27) {
        xhook_grouped_register(HOOK_REQUEST_GROUPID_ANR_DUMP_TRACE, ".*libcutils\\.so$",
                               "connect", (void *) my_connect, (void **) (&original_connect));
    } else {
        xhook_grouped_register(HOOK_REQUEST_GROUPID_ANR_DUMP_TRACE, ".*libart\\.so$",
                               "open", (void *) my_open, (void **) (&original_open));
    }

    if (apiLevel >= 30 || apiLevel == 25 || apiLevel == 24) {
        xhook_grouped_register(HOOK_REQUEST_GROUPID_ANR_DUMP_TRACE, ".*libc\\.so$",
                               "write", (void *) my_write, (void **) (&original_write));
    } else if (apiLevel == 29) {
        xhook_grouped_register(HOOK_REQUEST_GROUPID_ANR_DUMP_TRACE, ".*libbase\\.so$",
                               "write", (void *) my_write, (void **) (&original_write));
    } else {
        xhook_grouped_register(HOOK_REQUEST_GROUPID_ANR_DUMP_TRACE, ".*libart\\.so$",
                               "write", (void *) my_write, (void **) (&original_write));
    }

    xhook_refresh(true);
}

//void unHookAnrTraceWrite() {
//    isHooking = false;
//}

static void nativeInitSignalAnrDetective(JNIEnv *env, jclass, jstring anrTracePath, jstring printTracePath) {
    const char* anrTracePathChar = env->GetStringUTFChars(anrTracePath, nullptr);
    const char* printTracePathChar = env->GetStringUTFChars(printTracePath, nullptr);
    anrTracePathString = std::string(anrTracePathChar);
    printTracePathString = std::string(printTracePathChar);
    sAnrDumper.emplace(anrTracePathChar, printTracePathChar);

    //这里就进行hook
    hookAnrTraceWrite(false);
}

static void nativeFreeSignalAnrDetective(JNIEnv *env, jclass) {
    sAnrDumper.reset();
}

static void nativePrintTrace() {
    fromMyPrintTrace = true;
    kill(getpid(), SIGQUIT);
}

template <typename T, std::size_t sz>
static inline constexpr std::size_t NELEM(const T(&)[sz]) { return sz; }

static const JNINativeMethod ANR_METHODS[] = {
    {"nativeInitSignalAnrDetective", "(Ljava/lang/String;Ljava/lang/String;)V", (void *) nativeInitSignalAnrDetective},
    {"nativeFreeSignalAnrDetective", "()V", (void *) nativeFreeSignalAnrDetective},
    {"nativePrintTrace", "()V", (void *) nativePrintTrace},
};

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *) {
    JniInvocation::init(vm);

    JNIEnv *env;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK)
        return -1;

    jclass anrDetectiveCls = env->FindClass("com/tencent/matrix/trace/tracer/SignalAnrTracer");
    if (!anrDetectiveCls)
        return -1;
    gJ.AnrDetective = static_cast<jclass>(env->NewGlobalRef(anrDetectiveCls));
    gJ.AnrDetector_onANRDumped =
            env->GetStaticMethodID(anrDetectiveCls, "onANRDumped", "()V");
    gJ.AnrDetector_onANRDumpTrace =
            env->GetStaticMethodID(anrDetectiveCls, "onANRDumpTrace", "()V");
    gJ.AnrDetector_onPrintTrace =
            env->GetStaticMethodID(anrDetectiveCls, "onPrintTrace", "()V");

    gJ.AnrDetector_onNativeBacktraceDumped =
            env->GetStaticMethodID(anrDetectiveCls, "onNativeBacktraceDumped", "()V");


    if (env->RegisterNatives(
            anrDetectiveCls, ANR_METHODS, static_cast<jint>(NELEM(ANR_METHODS))) != 0)
        return -1;

    env->DeleteLocalRef(anrDetectiveCls);



    jclass testClazz = env->FindClass("com/llk/trace/MainActivity");
    if (!testClazz)
        return -1;
    gJ.Test_MainActivity = static_cast<jclass>(env->NewGlobalRef(testClazz));
    gJ.Test_WriteAnrTraceFinish =
            env->GetStaticMethodID(testClazz, "onAnrTraceWriteFinish", "()V");

    env->DeleteLocalRef(testClazz);


    return JNI_VERSION_1_6;
}   // namespace MatrixTracer
