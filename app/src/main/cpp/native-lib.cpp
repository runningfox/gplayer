#include <jni.h>
#include <string>

extern "C"{
#include <libavformat/avformat.h>
}
JNIEXPORT jstring JNICALL
Java_com_example_luhaiyang_ffplay_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject  thiz) {
    std::string hello = "Hello from C++";
    av_register_all();
    return env->NewStringUTF(hello.c_str());
}
