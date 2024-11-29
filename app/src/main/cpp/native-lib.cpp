#include <jni.h>
#include <string>
#include <sstream>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/video.hpp>
#include <opencv2/opencv.hpp>
#include "android/bitmap.h"
#include <android/log.h>
#include <deque>

using namespace std;
using namespace cv;

deque<Mat> buffer;
const int bufferSize = 5;

#define LOG_TAG "JNI"

#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

void bitmapToMat(JNIEnv *env, jobject bitmap, cv::Mat &dst, jboolean needUnPremultiplyAlpha) {
    AndroidBitmapInfo info;
    void* pixels = 0;
    try {
        LOGI("Iniciando la conversión de Bitmap a Mat");
        CV_Assert(AndroidBitmap_getInfo(env, bitmap, &info) >= 0);
        LOGI("Información del Bitmap: width=%d, height=%d", info.width, info.height);
        CV_Assert(info.format == ANDROID_BITMAP_FORMAT_RGBA_8888 || info.format == ANDROID_BITMAP_FORMAT_RGB_565);
        CV_Assert(AndroidBitmap_lockPixels(env, bitmap, &pixels) >= 0);
        CV_Assert(pixels);
        dst.create(info.height, info.width, CV_8UC4);
        if(info.format == ANDROID_BITMAP_FORMAT_RGBA_8888) {
            cv::Mat tmp(info.height, info.width, CV_8UC4, pixels);
            if(needUnPremultiplyAlpha)
                cvtColor(tmp, dst, cv::COLOR_mRGBA2RGBA);
            else
                tmp.copyTo(dst);
        } else {
            cv::Mat tmp(info.height, info.width, CV_8UC2, pixels);
            cvtColor(tmp, dst, cv::COLOR_BGR5652RGBA);
        }
        AndroidBitmap_unlockPixels(env, bitmap);
        LOGI("Conversión completada");
        return;
    } catch(const cv::Exception& e) {
        AndroidBitmap_unlockPixels(env, bitmap);
        LOGE("Error en bitmapToMat: %s", e.what());
        jclass je = env->FindClass("java/lang/Exception");
        env->ThrowNew(je, e.what());
        return;
    } catch (...) {
        AndroidBitmap_unlockPixels(env, bitmap);
        LOGE("Error desconocido en bitmapToMat");
        jclass je = env->FindClass("java/lang/Exception");
        env->ThrowNew(je, "Unknown exception in JNI code {nBitmapToMat}");
        return;
    }
}

void matToBitmap(JNIEnv *env, cv::Mat src, jobject bitmap, jboolean needPremultiplyAlpha) {
    AndroidBitmapInfo info;
    void* pixels = 0;
    try {
        CV_Assert(AndroidBitmap_getInfo(env, bitmap, &info) >= 0);
        CV_Assert(info.format == ANDROID_BITMAP_FORMAT_RGBA_8888 || info.format == ANDROID_BITMAP_FORMAT_RGB_565);
        CV_Assert(src.dims == 2 && info.height == (uint32_t)src.rows && info.width == (uint32_t)src.cols);
        CV_Assert(src.type() == CV_8UC1 || src.type() == CV_8UC3 || src.type() == CV_8UC4);
        CV_Assert(AndroidBitmap_lockPixels(env, bitmap, &pixels) >= 0);
        CV_Assert(pixels);
        if(info.format == ANDROID_BITMAP_FORMAT_RGBA_8888) {
            cv::Mat tmp(info.height, info.width, CV_8UC4, pixels);
            if(src.type() == CV_8UC1) {
                cvtColor(src, tmp, cv::COLOR_GRAY2RGBA);
            } else if(src.type() == CV_8UC3) {
                cvtColor(src, tmp, cv::COLOR_RGB2RGBA);
            } else if(src.type() == CV_8UC4) {
                if(needPremultiplyAlpha)
                    cvtColor(src, tmp, cv::COLOR_RGBA2mRGBA);
                else
                    src.copyTo(tmp);
            }
        } else { // RGB_565
            cv::Mat tmp(info.height, info.width, CV_8UC2, pixels);
            if(src.type() == CV_8UC1) {
                cvtColor(src, tmp, cv::COLOR_GRAY2BGR565);
            } else if(src.type() == CV_8UC3) {
                cvtColor(src, tmp, cv::COLOR_RGB2BGR565);
            } else if(src.type() == CV_8UC4) {
                cvtColor(src, tmp, cv::COLOR_RGBA2BGR565);
            }
        }
        AndroidBitmap_unlockPixels(env, bitmap);
        return;
    } catch(const cv::Exception& e) {
        AndroidBitmap_unlockPixels(env, bitmap);
        jclass je = env->FindClass("java/lang/Exception");
        env->ThrowNew(je, e.what());
        return;
    } catch(...) {
        AndroidBitmap_unlockPixels(env, bitmap);
        jclass je = env->FindClass("java/lang/Exception");
        env->ThrowNew(je, "Unknown exception in JNI code {nMatToBitmap}");
        return;
    }
}

cv::Mat applyAdvancedGlitchEffect(const cv::Mat& input, int glitchIntensity, int noiseAmount) {
    if (input.empty()) {
        LOGE("La imagen de entrada está vacía en applyAdvancedGlitchEffect.");
        return input;
    }

    cv::Mat output = input.clone();
    int rows = output.rows;
    int cols = output.cols;

    for (int i = 0; i < rows; i++) {
        if (rand() % 100 < glitchIntensity) {
            int shift = rand() % 30;
            if (rand() % 2 == 0) {
                output.row(i).copyTo(output.row((i + shift) % rows));
            } else {
                output.row(i).copyTo(output.row((i - shift + rows) % rows));
            }
        }
    }

    cv::Mat noise = cv::Mat::zeros(output.size(), output.type());
    randn(noise, 0, noiseAmount);
    output += noise;

    cv::min(output, 255, output);
    cv::max(output, 0, output);

    return output;
}

Mat applyCanny(const Mat& input) {
    Mat gray, edges;

    cvtColor(input, gray, COLOR_BGR2GRAY);

    Canny(gray, edges, 100, 200, 3);

    GaussianBlur(edges, edges, Size(5, 5), 0);
    return edges;
}

Mat colorizeEdges(const Mat& edges, int edgeColorChange) {
    Mat coloredEdges = Mat::zeros(edges.size(), CV_8UC3);

    for (int y = 0; y < edges.rows; ++y) {
        for (int x = 0; x < edges.cols; ++x) {
            if (edges.at<uchar>(y, x) > 0) {
                int intensity = edges.at<uchar>(y, x);

                int amplifiedChange = edgeColorChange * 10;

                coloredEdges.at<Vec3b>(y, x) = Vec3b(
                        (intensity * amplifiedChange) % 256,
                        (intensity * amplifiedChange / 2) % 256,
                        (intensity + amplifiedChange) % 256
                );
            }
        }
    }
    return coloredEdges;
}


extern "C"
JNIEXPORT void JNICALL
Java_com_example_aplicacionnativa_MainActivity_applyGlitchEffect(
        JNIEnv *env,
        jobject thiz,
        jobject bitmapIn,
        jobject bitmapOut,
        jint glitchIntensity,
        jint noiseAmount,
        jint edgeColorChange) {
    Mat src, edges, coloredEdges, delayedFrame, glitchEffect, combined;

    bitmapToMat(env, bitmapIn, src, false);

    if (src.empty()) {
        LOGE("Error: La imagen de entrada está vacía.");
        return;
    }

    if (src.channels() == 4) {
        cvtColor(src, src, COLOR_RGBA2RGB);
    }

    buffer.push_back(src.clone());
    if (buffer.size() > bufferSize) {
        buffer.pop_front();
    }

    if (buffer.size() == bufferSize) {
        delayedFrame = buffer.front();
    } else {
        delayedFrame = src;
    }

    glitchEffect = applyAdvancedGlitchEffect(delayedFrame, glitchIntensity, noiseAmount);

    edges = applyCanny(src);

    coloredEdges = colorizeEdges(edges, edgeColorChange);

    resize(glitchEffect, glitchEffect, src.size());
    resize(coloredEdges, coloredEdges, src.size());

    addWeighted(glitchEffect, 0.7, coloredEdges, 0.3, 0, combined);

    if (combined.empty()) {
        LOGE("Error: La combinación de imágenes falló.");
        return;
    }

    matToBitmap(env, combined, bitmapOut, false);
}


