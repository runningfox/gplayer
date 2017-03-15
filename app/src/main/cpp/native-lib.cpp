#include <jni.h>
#include <string>


extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

JNIEXPORT jstring JNICALL
Java_com_example_luhaiyang_ffplay_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject) {
    std::string hello = "Hello from C++";
    av_register_all();
    return env->NewStringUTF(hello.c_str());
}
}