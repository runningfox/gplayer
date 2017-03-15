#include <jni.h>
#include <string>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>


#define  LOG_TAG    "gplayer"
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

JNIEXPORT jstring JNICALL
Java_com_example_luhaiyang_ffplay_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject) {
    std::string hello = "Hello from Gplayer++";
    av_register_all();
    char *hls = "http://devimages.apple.com.edgekey.net/streaming/examples/bipbop_4x3/bipbop_4x3_variant.m3u8";

    //1 init avFormatContext
    AVFormatContext *avFormatContext = avformat_alloc_context();

    //2 open stream
    if (avformat_open_input(&avFormatContext, hls, NULL, NULL) != 0) {
        LOGD("333 open stream failed %s", hls);
        hello = "can not open stream";
        return env->NewStringUTF(hello.c_str());;
    }

    //3 find stream info
    if (avformat_find_stream_info(avFormatContext, NULL) < 0) {
        LOGD("333 find stream info failed");
        hello = "can not find stream info";
        return env->NewStringUTF(hello.c_str());
    }

    int videoStream = -1, audioStream = -1, i;
    LOGD("333 find stream info with %d streams", avFormatContext->nb_streams);
    for (i = 0; i < avFormatContext->nb_streams; i++) {
        AVMediaType avMediaType = avFormatContext->streams[i]->codec->codec_type;
        switch (avMediaType) {
            case AVMEDIA_TYPE_VIDEO: {
                LOGD("333 find stream info AVMEDIA_TYPE_VIDEO");
                if (videoStream < 0) {
                    videoStream = i;
                }
                break;
            }
            case AVMEDIA_TYPE_AUDIO: {
                LOGD("333 find stream info AVMEDIA_TYPE_AUDIO");
                if (audioStream < 0) {
                    audioStream = i;
                }
                break;
            }
            default: {
                LOGD("333 find stream info AVMEDIA_TYPE_UNKNOWN");
            }
        }
    }

    if (videoStream == -1) {
        LOGD("333 find video stream failed");
        hello = "can not find video stream";
        return env->NewStringUTF(hello.c_str());
    }

    LOGD("333 find video stream %d", videoStream);

    AVCodecContext *avCodecContext = avFormatContext->streams[videoStream]->codec;
    AVCodec *avCodec = avcodec_find_decoder(avCodecContext->codec_id);

    if (avCodec == NULL) {
        LOGD("333 find codec failed");
        return env->NewStringUTF(hello.c_str());
    }

    if (avcodec_open2(avCodecContext, avCodec, NULL) < 0) {
        LOGD("333 open codec failed");
        return env->NewStringUTF(hello.c_str());
    }


    AVFrame *avFrame = av_frame_alloc();
    AVFrame *avFrameRGB = av_frame_alloc();

    int buffer_size = av_image_get_buffer_size(AV_PIX_FMT_RGBA, avCodecContext->width, avCodecContext->height, 1);
    uint8_t *buffer = (uint8_t *) av_malloc(buffer_size * sizeof(uint8_t));
    av_image_fill_arrays(avFrameRGB->data, avFrameRGB->linesize, buffer, AV_PIX_FMT_RGBA, avCodecContext->width,
                         avCodecContext->height, 1);
    struct SwsContext *swsContext = sws_getContext(avCodecContext->width,
                                                   avCodecContext->height,
                                                   avCodecContext->pix_fmt,
                                                   avCodecContext->width,
                                                   avCodecContext->height,
                                                   AV_PIX_FMT_RGBA,
                                                   SWS_BILINEAR,
                                                   NULL,
                                                   NULL,
                                                   NULL);

    int frameFinished;
    AVPacket packet;
    LOGD("333 av_read_frame");
    while (av_read_frame(avFormatContext, &packet) >= 0) {
        if (packet.stream_index == videoStream) {
            avcodec_decode_video2(avCodecContext, avFrame, &frameFinished, &packet);
            if (frameFinished) {
//                LOGD("333 frame decoded finished");
            }
        }
    }

    av_free(buffer);
    av_free(avFrameRGB);

    av_free(avFrame);

    avcodec_close(avCodecContext);
    avformat_close_input(&avFormatContext);

    return env->NewStringUTF(hello.c_str());
}
}