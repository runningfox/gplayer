#include <jni.h>
#include <string>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE 192000

#define  LOG_TAG    "gplayer"
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libsdl/SDL.h>
JNIEXPORT jstring JNICALL
Java_com_example_luhaiyang_ffplay_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject) {
    std::string hello = "Hello from Gplayer++";
    return env->NewStringUTF(hello.c_str());
}

typedef struct PacketQueue {
    AVPacketList *first_pkt, *last_pkt;
    int nb_packets;
    int size;
    SDL_mutex *mutex;
    SDL_cond *cond;
} PacketQueue;


PacketQueue audioq;


void packet_queue_init(PacketQueue *q) {
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = SDL_CreateMutex();
    q->cond = SDL_CreateCond();
}
int packet_queue_put(PacketQueue *q, AVPacket *pkt) {

    AVPacketList *pkt1;
    if (av_dup_packet(pkt) < 0) {
        return -1;
    }
    pkt1 = (AVPacketList *) av_malloc(sizeof(AVPacketList));
    if (!pkt1)
        return -1;
    pkt1->pkt = *pkt;
    pkt1->next = NULL;


    SDL_LockMutex(q->mutex);

    if (!q->last_pkt)
        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size += pkt1->pkt.size;
    SDL_CondSignal(q->cond);

    SDL_UnlockMutex(q->mutex);
    return 0;
}
static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block) {
    AVPacketList *pkt1;
    int ret;

    SDL_LockMutex(q->mutex);

    for (;;) {

        pkt1 = q->first_pkt;
        if (pkt1) {
            q->first_pkt = pkt1->next;
            if (!q->first_pkt)
                q->last_pkt = NULL;
            q->nb_packets--;
            q->size -= pkt1->pkt.size;
            *pkt = pkt1->pkt;
            av_free(pkt1);
            ret = 1;
            break;
        } else if (!block) {
            ret = 0;
            break;
        } else {
            SDL_CondWait(q->cond, q->mutex);
        }
    }
    SDL_UnlockMutex(q->mutex);
    return ret;
}

int audio_decode_frame(AVCodecContext *aCodecCtx, uint8_t *audio_buf, int buf_size) {

    static AVPacket pkt;
    static uint8_t *audio_pkt_data = NULL;
    static int audio_pkt_size = 0;
    static AVFrame frame;

    int len1, data_size = 0;

    for (;;) {
        while (audio_pkt_size > 0) {
            int got_frame = 0;
            len1 = avcodec_decode_audio4(aCodecCtx, &frame, &got_frame, &pkt);
            if (len1 < 0) {
                /* if error, skip frame */
                audio_pkt_size = 0;
                break;
            }
            audio_pkt_data += len1;
            audio_pkt_size -= len1;
            data_size = 0;
            if (got_frame) {
                data_size = av_samples_get_buffer_size(NULL,
                                                       aCodecCtx->channels,
                                                       frame.nb_samples,
                                                       aCodecCtx->sample_fmt,
                                                       1);
                if (data_size <= buf_size) {
                    memcpy(audio_buf, frame.data[0], data_size);
                }
            }
            if (data_size <= 0) {
                /* No data yet, get more frames */
                continue;
            }
            /* We have data, return it and come back for more later */
            return data_size;
        }
        if (pkt.data)
            av_free_packet(&pkt);


        if (packet_queue_get(&audioq, &pkt, 1) < 0) {
            return -1;
        }
        audio_pkt_data = pkt.data;
        audio_pkt_size = pkt.size;
    }
}

void audio_callback(void *userdata, Uint8 *stream, int len) {
    LOGD("444 audio_callback called");
    AVCodecContext *aCodecCtx = (AVCodecContext *) userdata;
    int len1, audio_size;

    static uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
    static unsigned int audio_buf_size = 0;
    static unsigned int audio_buf_index = 0;

    while (len > 0) {
        if (audio_buf_index >= audio_buf_size) {
            /* We have already sent all our data; get more */
            audio_size = audio_decode_frame(aCodecCtx, audio_buf, sizeof(audio_buf));
            if (audio_size < 0) {
                /* If error, output silence */
                audio_buf_size = SDL_AUDIO_BUFFER_SIZE; // arbitrary?
                memset(audio_buf, 0, audio_buf_size);
            } else {
                audio_buf_size = audio_size;
            }
            audio_buf_index = 0;
        }
        len1 = audio_buf_size - audio_buf_index;
        if (len1 > len)
            len1 = len;
        memcpy(stream, (uint8_t *) audio_buf + audio_buf_index, len1);
        len -= len1;
        stream += len1;
        audio_buf_index += len1;
    }
}

JNIEXPORT void JNICALL
Java_com_example_luhaiyang_ffplay_MainActivity_play(JNIEnv *env, jobject thiz, jobject jsurface) {

    av_register_all();
    SDL_Init(SDL_INIT_AUDIO);
    char *hls = "http://devimages.apple.com.edgekey.net/streaming/examples/bipbop_4x3/bipbop_4x3_variant.m3u8";

    //1 init avFormatContext
    AVFormatContext *avFormatContext = avformat_alloc_context();

    //2 open stream
    if (avformat_open_input(&avFormatContext, hls, NULL, NULL) != 0) {
        LOGD("333 open stream failed %s", hls);
        return;
    }

    //3 find stream info
    if (avformat_find_stream_info(avFormatContext, NULL) < 0) {
        LOGD("333 find stream info failed");
        return;
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
        return;
    }

    AVCodecContext *avCodecContext = avFormatContext->streams[videoStream]->codec;
    AVCodec *avCodec = avcodec_find_decoder(avCodecContext->codec_id);

    if (avCodec == NULL) {
        LOGD("333 find codec failed");
        return;
    }

    SDL_AudioSpec wanted, get;
    wanted.freq = avCodecContext->sample_rate;
    wanted.format = AUDIO_S16SYS;
    wanted.channels = avCodecContext->channels;
    wanted.silence = 0;
    wanted.samples = 1024;
    wanted.callback = audio_callback;
    wanted.userdata = avCodecContext;
    int audio_ready = SDL_OpenAudio(&wanted, &get);

    if (avcodec_open2(avCodecContext, avCodec, NULL) < 0) {
        LOGD("333 open codec failed");
        return;
    }

    if (audio_ready == 0) {
        LOGD("444 SDL_OpenAudio = %d ok", audio_ready);
        packet_queue_init(&audioq);
        SDL_PauseAudio(0);
        LOGD("444 SDL_PauseAudio(0);");
    } else {
        LOGD("444 SDL_OpenAudio = %d failed", audio_ready);
    }


    LOGD("333 find video stream %d width = %d , height = %d", videoStream, avCodecContext->width,
         avCodecContext->height);

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

    ANativeWindow *nativeWindow = ANativeWindow_fromSurface(env, jsurface);
    ANativeWindow_setBuffersGeometry(nativeWindow, avCodecContext->width, avCodecContext->height,
                                     WINDOW_FORMAT_RGBA_8888);
    ANativeWindow_Buffer windowBuffer;
    int frameFinished;
    AVPacket packet;

    SDL_PauseAudio(0);

    LOGD("333 av_read_frame");
    while (av_read_frame(avFormatContext, &packet) >= 0) {
        if (packet.stream_index == videoStream) {
            avcodec_decode_video2(avCodecContext, avFrame, &frameFinished, &packet);
            if (frameFinished) {
                ANativeWindow_lock(nativeWindow, &windowBuffer, 0);
                sws_scale(swsContext, (uint8_t const *const *) avFrame->data,
                          avFrame->linesize, 0, avCodecContext->height,
                          avFrameRGB->data, avFrameRGB->linesize);


                uint8_t *dst = (uint8_t *) windowBuffer.bits;
                int dstStride = windowBuffer.stride * 4;
                uint8_t *src = (avFrameRGB->data[0]);
                int srcStride = avFrameRGB->linesize[0];

                // 由于window的stride和帧的stride不同,因此需要逐行复制
                int h;
                for (h = 0; h < avCodecContext->height; h++) {
                    memcpy(dst + h * dstStride, src + h * srcStride, srcStride);
                }

                ANativeWindow_unlockAndPost(nativeWindow);
                av_free_packet(&packet);
            }
        } else if (packet.stream_index == audioStream) {
            packet_queue_put(&audioq, &packet);
        } else {
            av_free_packet(&packet);
        }
    }

    av_free(buffer);
    av_free(avFrameRGB);

    av_free(avFrame);

    avcodec_close(avCodecContext);
    avformat_close_input(&avFormatContext);

}

}