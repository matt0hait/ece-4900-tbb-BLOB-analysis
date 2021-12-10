//
// Created by Matthew Hait on 11/11/2021.
//

extern "C" {
    #include "img_utils.h"
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
}
#include "ffmpeg_utils.h"

bool load_frame(const char* inFile, int* width, int* height, unsigned char** date) {
    AVFormatContext* av_format_ctx = avformat_alloc_context();  // Create format context
    if (!av_format_ctx) { printf("Could not create context.\n"); return false;}
    if(!avformat_open_input(&av_format_ctx, inFile, nullptr, nullptr) != 0) {
        printf("Could not open file.\n");
        // TODO: Free AV Context
        return false;
    }
    // Iterate through video streams
    for (size_t i = 0; i < av_format_ctx->nb_streams; i++) {
        auto stream = av_format_ctx->streams[i];

    }
    return false;
}