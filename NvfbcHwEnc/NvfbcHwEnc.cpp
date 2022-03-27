/*!
 * \brief
 * Demonstrates how to use NvFBC to synchronously grab and encode the
 * desktop or a fullscreen application with the hardware encoder.
 *
 * \file
 * This sample demonstrates the following features:
 * - Capture to H.264 or H.265/HEVC compressed frames in system memory;
 * - Select an output (monitor) to track;
 * - Frame scaling;
 * - Synchronous (blocking) capture.
 *
 * The sample demonstrates how to use NvFBC to grab and encode with the
 * hardware encoder. The program show how to initialize the NvFBCHwEnc
 * encoder class, set up the grab and encode, and grab the full-screen
 * framebuffer, compress it, and copy it to system memory.
 * Because of this, the NvFBCHwEnc class requires a Kepler GPU or better
 * to work.  Attempting to create an instance of that class on earlier cards
 * will result in the create call failing.
 *
 *
 * \copyright
 * Copyright (c) 2015-2017, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <dlfcn.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <NvFBC.h>

#include "NvFBCUtils.h"

#define APP_VERSION 4

#define LIB_NVFBC_NAME "libnvidia-fbc.so.1"

#define N_FRAMES 1000

#include "../streamer/streamer.hpp"
using namespace streamer;


/**
 * Prints usage information.
 */
static void usage(const char *pname) {
    printf("Usage: %s [options]\n", pname);
    printf("\n");
    printf("Options:\n");
    printf("  --help|-h         This message\n");
    printf("  --get-status|-g   Print status and exit\n");
    printf("  --track|-t <str>  Region of the screen to track\n");
    printf(
        "                    Can be 'default', 'screen' or '<output name>'\n");
    printf("                    as returned by --get-status\n");
    printf("  --frames|-f <n>   Number of frames to capture (default: %u)\n",
           N_FRAMES);
    printf("  --size|-s <w>x<h> Size of the captured frames\n");
    printf("                    (default: size of the framebuffer)\n");
    printf("  --codec|-c <str>  Codec to use\n");
    printf("                    Can be 'h264' or 'hevc'\n");
    printf("                    (default: 'h264')\n");
}

/**
 * Initializes the NvFBC library and creates an NvFBC instance.
 *
 * Creates and sets up a capture session to HW compressed frames in system
 * memory.
 *
 * Captures a bunch of frames and saves them to the disk.
 */
int init() {
    // static struct option longopts[] = {{"get-status", no_argument, NULL, 'g'},
    //                                    {"track", required_argument, NULL, 't'},
    //                                    {"frames", required_argument, NULL, 'f'},
    //                                    {"size", required_argument, NULL, 's'},
    //                                    {"codec", required_argument, NULL, 'c'},
    //                                    {NULL, 0, NULL, 0}};

    char filename[64];

    int opt, ret;
    unsigned int i, nFrames = N_FRAMES;
    NVFBC_SIZE frameSize = {0, 0};
    NVFBC_BOOL printStatusOnly = NVFBC_FALSE;

    NVFBC_TRACKING_TYPE trackingType = NVFBC_TRACKING_DEFAULT;
    char outputName[NVFBC_OUTPUT_NAME_LEN];
    uint32_t outputId = 0;

    FILE *fd;

    void *libNVFBC = NULL;
    PNVFBCCREATEINSTANCE NvFBCCreateInstance_ptr = NULL;
    NVFBC_API_FUNCTION_LIST pFn;

    NVFBCSTATUS fbcStatus;

    NVFBC_SESSION_HANDLE fbcHandle;
    NVFBC_CREATE_HANDLE_PARAMS createHandleParams;
    NVFBC_GET_STATUS_PARAMS statusParams;
    NVFBC_CREATE_CAPTURE_SESSION_PARAMS createCaptureParams;
    NVFBC_DESTROY_CAPTURE_SESSION_PARAMS destroyCaptureParams;
    NVFBC_DESTROY_HANDLE_PARAMS destroyHandleParams;

    NVFBC_HWENC_CONFIG encoderConfig;
    NVFBC_TOHWENC_SETUP_PARAMS setupParams;

    NVFBC_HWENC_CODEC codec = NVFBC_HWENC_CODEC_HEVC; // hevc is not choppy. h264 is choppy. 

    NvFBCUtilsPrintVersions(APP_VERSION);

    if (codec == NVFBC_HWENC_CODEC_H264) {
        sprintf(filename, "NvFBCSample.h264");
    } else {
        sprintf(filename, "NvFBCSample.hevc");
    }

    /*
     * Dynamically load the NvFBC library.
     */
    libNVFBC = dlopen(LIB_NVFBC_NAME, RTLD_NOW);
    if (libNVFBC == NULL) {
        fprintf(stderr, "Unable to open '%s' (%s)\n", LIB_NVFBC_NAME,
                dlerror());
        return EXIT_FAILURE;
    }

    /*
     * Resolve the 'NvFBCCreateInstance' symbol that will allow us to get
     * the API function pointers.
     */
    NvFBCCreateInstance_ptr =
        (PNVFBCCREATEINSTANCE)dlsym(libNVFBC, "NvFBCCreateInstance");
    if (NvFBCCreateInstance_ptr == NULL) {
        fprintf(stderr, "Unable to resolve symbol 'NvFBCCreateInstance'\n");
        return EXIT_FAILURE;
    }

    /*
     * Create an NvFBC instance.
     *
     * API function pointers are accessible through pFn.
     */
    memset(&pFn, 0, sizeof(pFn));

    pFn.dwVersion = NVFBC_VERSION;

    fbcStatus = NvFBCCreateInstance_ptr(&pFn);
    if (fbcStatus != NVFBC_SUCCESS) {
        fprintf(stderr, "Unable to create NvFBC instance (status: %d)\n",
                fbcStatus);
        return EXIT_FAILURE;
    }

    /*
     * Create a session handle that is used to identify the client.
     */
    memset(&createHandleParams, 0, sizeof(createHandleParams));

    createHandleParams.dwVersion = NVFBC_CREATE_HANDLE_PARAMS_VER;

    fbcStatus = pFn.nvFBCCreateHandle(&fbcHandle, &createHandleParams);
    if (fbcStatus != NVFBC_SUCCESS) {
        fprintf(stderr, "%s\n", pFn.nvFBCGetLastErrorStr(fbcHandle));
        return EXIT_FAILURE;
    }

    /*
     * Get information about the state of the display driver.
     *
     * This call is optional but helps the application decide what it should
     * do.
     */
    memset(&statusParams, 0, sizeof(statusParams));

    statusParams.dwVersion = NVFBC_GET_STATUS_PARAMS_VER;

    fbcStatus = pFn.nvFBCGetStatus(fbcHandle, &statusParams);
    if (fbcStatus != NVFBC_SUCCESS) {
        fprintf(stderr, "%s\n", pFn.nvFBCGetLastErrorStr(fbcHandle));
        return EXIT_FAILURE;
    }

    if (printStatusOnly) {
        NvFBCUtilsPrintStatus(&statusParams);
        return EXIT_SUCCESS;
    }

    if (statusParams.bCanCreateNow == NVFBC_FALSE) {
        fprintf(stderr, "It is not possible to create a capture session "
                        "on this system.\n");
        return EXIT_FAILURE;
    }

    if (trackingType == NVFBC_TRACKING_OUTPUT) {
        if (!statusParams.bXRandRAvailable) {
            fprintf(stderr, "The XRandR extension is not available.\n");
            fprintf(stderr,
                    "It is therefore not possible to track an RandR output.\n");
            return EXIT_FAILURE;
        }

        outputId = NvFBCUtilsGetOutputId(statusParams.outputs,
                                         statusParams.dwOutputNum, outputName);
        if (outputId == 0) {
            fprintf(stderr, "RandR output '%s' not found.\n", outputName);
            return EXIT_FAILURE;
        }
    }

    /*
     * Create a capture session.
     */
    printf("Creating a capture session of %u HW compressed frames.\n", nFrames);

    memset(&createCaptureParams, 0, sizeof(createCaptureParams));

    createCaptureParams.dwVersion = NVFBC_CREATE_CAPTURE_SESSION_PARAMS_VER;
    createCaptureParams.eCaptureType = NVFBC_CAPTURE_TO_HW_ENCODER;
    createCaptureParams.bWithCursor = NVFBC_TRUE;
    createCaptureParams.frameSize = frameSize;
    createCaptureParams.bRoundFrameSize = NVFBC_TRUE;
    createCaptureParams.eTrackingType = trackingType;

    if (trackingType == NVFBC_TRACKING_OUTPUT) {
        createCaptureParams.dwOutputId = outputId;
    }

    fbcStatus = pFn.nvFBCCreateCaptureSession(fbcHandle, &createCaptureParams);
    if (fbcStatus != NVFBC_SUCCESS) {
        fprintf(stderr, "%s\n", pFn.nvFBCGetLastErrorStr(fbcHandle));
        return EXIT_FAILURE;
    }

    /*
     * Configure the HW encoder.
     *
     * Select encoding quality, bitrate, etc.
     *
     * Here, we are configuring a 60 fps capture at a balanced encode / decode
     * time, using a high quality profile.
     */
    memset(&encoderConfig, 0, sizeof(encoderConfig));

    encoderConfig.dwVersion = NVFBC_HWENC_CONFIG_VER;
    if (codec == NVFBC_HWENC_CODEC_H264) {
        encoderConfig.dwProfile = 77;
    } else {
        encoderConfig.dwProfile = 1;
    }
    encoderConfig.dwFrameRateNum = 60;
    encoderConfig.dwFrameRateDen = 1;
    encoderConfig.dwAvgBitRate = 8000000;
    encoderConfig.dwPeakBitRate = encoderConfig.dwAvgBitRate * 1.5;
    encoderConfig.dwGOPLength = 100;
    encoderConfig.eRateControl = NVFBC_HWENC_PARAMS_RC_VBR;
    encoderConfig.ePresetConfig = NVFBC_HWENC_PRESET_LOW_LATENCY_HP;
    encoderConfig.dwQP = 26;
    encoderConfig.eInputBufferFormat = NVFBC_BUFFER_FORMAT_NV12;
    encoderConfig.dwVBVBufferSize = encoderConfig.dwAvgBitRate;
    encoderConfig.dwVBVInitialDelay = encoderConfig.dwVBVBufferSize;
    encoderConfig.codec = codec;

    /*
     * Frame headers are included with the frame.  Set this to TRUE for
     * outband header fetching.  Headers can then be fetched using the
     * NvFBCToHwEncGetHeader() API call.
     */
    encoderConfig.bOutBandSPSPPS = NVFBC_FALSE;

    /*
     * Set up the capture session.
     */
    memset(&setupParams, 0, sizeof(setupParams));

    setupParams.dwVersion = NVFBC_TOHWENC_SETUP_PARAMS_VER;
    setupParams.pEncodeConfig = &encoderConfig;

    fbcStatus = pFn.nvFBCToHwEncSetUp(fbcHandle, &setupParams);
    if (fbcStatus != NVFBC_SUCCESS) {
        fprintf(stderr, "%s\n", pFn.nvFBCGetLastErrorStr(fbcHandle));
        return EXIT_FAILURE;
    }

    printf("Initializing streamer.\n");
    Streamer streamer;
    StreamerConfig streamer_config( 640, 480, 640, 480,
                                   60, 8000000, "main",
                                   "rtmp://live-push.bilivideo.com/live-bvc/?streamname=live_1418412989_72166059&key=37f7a37372160ae1d511810411c47ebc&schedule=rtmp&pflag=1");

    streamer.enable_av_debug_log();

    streamer.init(streamer_config);

    /*
     * We are now ready to start grabbing frames.
     */
    printf("Frame capture session started.  New frames will be captured when "
           "the display is refreshed or when the mouse cursor moves.\n");

    fd = fopen(filename, "wb");
    if (fd == NULL) {
        fprintf(stderr, "Unable to create '%s'\n", filename);
        return EXIT_FAILURE;
    }


    for (i = 0; i < nFrames; i++) {
        static unsigned char *frame = NULL;

        size_t nBytes;
        uint64_t t1, t2;

        NVFBC_TOHWENC_GRAB_FRAME_PARAMS grabParams;

        NVFBC_FRAME_GRAB_INFO frameInfo;
        NVFBC_HWENC_FRAME_INFO encFrameInfo;

        t1 = NvFBCUtilsGetTimeInMillis();

        memset(&grabParams, 0, sizeof(grabParams));
        memset(&frameInfo, 0, sizeof(frameInfo));
        memset(&encFrameInfo, 0, sizeof(encFrameInfo));

        grabParams.dwVersion = NVFBC_TOHWENC_GRAB_FRAME_PARAMS_VER;

        /*
         * Use blocking calls.
         *
         * The application will wait for new frames.  New frames are generated
         * when the mouse cursor moves or when the screen if refreshed.
         */
        grabParams.dwFlags = NVFBC_TOHWENC_GRAB_FLAGS_NOFLAGS;

        /*
         * This structure will contain information about the captured frame.
         */
        grabParams.pFrameGrabInfo = &frameInfo;

        /*
         * This structure will contain information about the encoding of
         * the captured frame.
         */
        grabParams.pEncFrameInfo = &encFrameInfo;

        /*
         * Specify per-frame encoding configuration.  Here, keep the defaults.
         */
        grabParams.pEncodeParams = NULL;

        /*
         * This pointer is allocated by the NvFBC library and will
         * contain the captured frame.
         *
         * Make sure this pointer stays the same during the capture session
         * to prevent memory leaks.
         */
        grabParams.ppBitStreamBuffer = (void **)&frame;

        /*
         * Capture a new frame.
         */
        fbcStatus = pFn.nvFBCToHwEncGrabFrame(fbcHandle, &grabParams);
        if (fbcStatus != NVFBC_SUCCESS) {
            fprintf(stderr, "%s\n", pFn.nvFBCGetLastErrorStr(fbcHandle));
            return EXIT_FAILURE;
        }

        printf("New frame grabbed and starting streaming\n");
        streamer.stream_frame(frame);
        printf("New frame grabbed and ending streaming\n");

        // /*
        //  * Save frame to the output file.
        //  *
        //  * Information such as dimension and size in bytes is available from
        //  * the frameInfo structure.
        //  */
        // nBytes = fwrite(frame, 1, frameInfo.dwByteSize, fd);
        // if (nBytes == 0) {
        //     fprintf(stderr, "Unable to write to '%s'\n", filename);
        //     return EXIT_FAILURE;
        // }

        // t2 = NvFBCUtilsGetTimeInMillis();

        // printf("New frame id %u grabbed and saved in %llu ms\n",
        //        frameInfo.dwCurrentFrame, (unsigned long long)(t2 - t1));
    }

    /*
     * Destroy capture session, tear down resources.
     */
    memset(&destroyCaptureParams, 0, sizeof(destroyCaptureParams));

    destroyCaptureParams.dwVersion = NVFBC_DESTROY_CAPTURE_SESSION_PARAMS_VER;

    fbcStatus =
        pFn.nvFBCDestroyCaptureSession(fbcHandle, &destroyCaptureParams);
    if (fbcStatus != NVFBC_SUCCESS) {
        fprintf(stderr, "%s\n", pFn.nvFBCGetLastErrorStr(fbcHandle));
        return EXIT_FAILURE;
    }

    /*
     * Destroy session handle, tear down more resources.
     */
    memset(&destroyHandleParams, 0, sizeof(destroyHandleParams));

    destroyHandleParams.dwVersion = NVFBC_DESTROY_HANDLE_PARAMS_VER;

    fbcStatus = pFn.nvFBCDestroyHandle(fbcHandle, &destroyHandleParams);
    if (fbcStatus != NVFBC_SUCCESS) {
        fprintf(stderr, "%s\n", pFn.nvFBCGetLastErrorStr(fbcHandle));
        return EXIT_FAILURE;
    }

    fclose(fd);
    printf("File '%s' saved.  It can played back with e.g., mplayer.\n",
           filename);

    return EXIT_SUCCESS;
}
