#include "gif.h"

bool reset(GifInfo *info) {
    if (info->rewindFunction(info) != 0)
        return false;
    info->nextStartTime = 0;
    info->currentLoop = 0;
    info->currentIndex = 0;
    info->lastFrameRemainder = -1;
    return true;
}

__unused JNIEXPORT jboolean JNICALL
Java_pl_droidsonroids_gif_GifInfoHandle_reset(JNIEnv *__unused  env, jclass  __unused class,
                                              jlong gifInfo) {
    GifInfo *info = (GifInfo *) (intptr_t) gifInfo;
    if (info == NULL)
        return JNI_FALSE;

    if (reset(info))
        return JNI_TRUE;
    return JNI_FALSE;
}

__unused JNIEXPORT void JNICALL
Java_pl_droidsonroids_gif_GifInfoHandle_setSpeedFactor(JNIEnv __unused *env, jclass __unused handleClass,
                                                       jlong gifInfo, jfloat factor) {
    GifInfo *info = (GifInfo *) (intptr_t) gifInfo;
    if (info == NULL)
        return;
    info->speedFactor = factor;
}

__unused JNIEXPORT void JNICALL
Java_pl_droidsonroids_gif_GifInfoHandle_seekToTime(JNIEnv *env, jclass __unused handleClass,
                                                   jlong gifInfo, jint desiredPos, jobject jbitmap) {
    GifInfo *info = (GifInfo *) (intptr_t) gifInfo;
    if (info == NULL)
        return;
    if (info->gifFilePtr->ImageCount == 1)
        return;

    unsigned long sum = 0;
    int desiredIndex;
    for (desiredIndex = 0; desiredIndex < info->gifFilePtr->ImageCount; desiredIndex++) {
        unsigned long newSum = sum + info->infos[desiredIndex].DelayTime;
        if (newSum >= desiredPos)
            break;
        sum = newSum;
    }

    if (desiredIndex < info->currentIndex && !reset(info)) {
        info->gifFilePtr->Error = D_GIF_ERR_REWIND_FAILED;
        return;
    }

    if (info->lastFrameRemainder != -1) {
        info->lastFrameRemainder = desiredPos - sum;
        if (desiredIndex == info->gifFilePtr->ImageCount - 1 &&
            info->lastFrameRemainder > info->infos[desiredIndex].DelayTime)
            info->lastFrameRemainder = info->infos[desiredIndex].DelayTime;
    }
    if (info->currentIndex < desiredIndex) {
        void *pixels;
        if (lockPixels(env, jbitmap, info, &pixels) != 0) {
            return;
        }
        while (info->currentIndex < desiredIndex) {
            DDGifSlurp(info, true);
            getBitmap((argb *) pixels, info);
        }
        unlockPixels(env, jbitmap);
    }

    info->nextStartTime = getRealTime() + (time_t) (info->lastFrameRemainder / info->speedFactor);
}

__unused JNIEXPORT void JNICALL
Java_pl_droidsonroids_gif_GifInfoHandle_seekToFrame(JNIEnv *env, jclass __unused handleClass,
                                                    jlong gifInfo, jint desiredIndex, jobject jbitmap) {
    GifInfo *info = (GifInfo *) (intptr_t) gifInfo;
    if (info == NULL || info->gifFilePtr->ImageCount == 1)
        return;

    if (desiredIndex < info->currentIndex && !reset(info)) {
        info->gifFilePtr->Error = D_GIF_ERR_REWIND_FAILED;
        return;
    }

    if (desiredIndex >= info->gifFilePtr->ImageCount)
        desiredIndex = info->gifFilePtr->ImageCount - 1;

    uint_fast32_t lastFrameDuration = info->infos[info->currentIndex].DelayTime;
    if (info->currentIndex < desiredIndex) {
        void *pixels;
        if (lockPixels(env, jbitmap, info, &pixels) != 0) {
            return;
        }
        while (info->currentIndex < desiredIndex) {
            DDGifSlurp(info, true);
            lastFrameDuration = getBitmap((argb *) pixels, info);
        }
        unlockPixels(env, jbitmap);
    }

    info->nextStartTime = getRealTime() + (time_t) (lastFrameDuration / info->speedFactor);
    if (info->lastFrameRemainder != -1)
        info->lastFrameRemainder = 0;
}

__unused JNIEXPORT void JNICALL
Java_pl_droidsonroids_gif_GifInfoHandle_saveRemainder(JNIEnv *__unused  env, jclass __unused handleClass,
                                                      jlong gifInfo) {
    GifInfo *info = (GifInfo *) (intptr_t) gifInfo;
    if (info == NULL || info->lastFrameRemainder != -1 || info->currentIndex == info->gifFilePtr->ImageCount ||
        info->gifFilePtr->ImageCount == 1)
        return;
    info->lastFrameRemainder = info->nextStartTime - getRealTime();
    if (info->lastFrameRemainder < 0)
        info->lastFrameRemainder = 0;
}

__unused JNIEXPORT jlong JNICALL
Java_pl_droidsonroids_gif_GifInfoHandle_restoreRemainder(JNIEnv *__unused env,
                                                         jclass __unused handleClass, jlong gifInfo) {
    GifInfo *info = (GifInfo *) (intptr_t) gifInfo;
    if (info == NULL || info->lastFrameRemainder == -1 || info->gifFilePtr->ImageCount == 1 ||
        info->currentLoop == info->loopCount)
        return -1;
    info->nextStartTime = getRealTime() + info->lastFrameRemainder;
    const time_t remainder = info->lastFrameRemainder;
    info->lastFrameRemainder = -1;
    return remainder;
}