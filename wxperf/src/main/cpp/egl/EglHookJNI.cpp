//
// Created by 邓沛堆 on 2020-05-06.
//

#include <jni.h>
#include <xhook.h>
#include <EGL/egl.h>
#include "EglHook.h"
#include "log.h"

#ifdef __cplusplus
extern "C" {
#endif

static HookFunction const EGL_HOOK_FUNCTIONS[] = {
        {"eglCreateContext",        (void *) HANDLER_FUNC_NAME(eglCreateContext),        NULL},
        {"eglDestroyContext",       (void *) HANDLER_FUNC_NAME(eglDestroyContext),       NULL},
        {"eglCreateWindowSurface",  (void *) HANDLER_FUNC_NAME(eglCreateWindowSurface),  NULL},
        {"eglCreatePbufferSurface", (void *) HANDLER_FUNC_NAME(eglCreatePbufferSurface), NULL},
        {"eglDestroySurface", (void *) HANDLER_FUNC_NAME(eglDestorySurface), NULL }
};

void egl_hook() {
    for (auto f: EGL_HOOK_FUNCTIONS) {
        xhook_register(".*\\.so$", f.name, f.handler_ptr, f.origin_ptr);
    }
}

void egl_hook_on_dlopen(const char *__file_name) {
    unwindstack::update_maps();
}

/*
 * Class:     com_tencent_mm_performance_jni_egl_EglHook
 * Method:    startHook
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_egl_EglHook_startHook(JNIEnv *, jobject) {
    egl_hook();
    add_dlopen_hook_callback(egl_hook_on_dlopen);
}

#ifdef __cplusplus
}
#endif