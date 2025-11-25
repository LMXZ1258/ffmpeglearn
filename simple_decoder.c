#include <stdio.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/log.h>

// 保存 AVFrame 的 Y 分量 (灰度图) 为 PGM 文件
// PGM 格式:
// P5
// width height
// max_val
// data
void save_gray_frame(AVFrame *frame, int frame_num) {
    char filename[1024];
    snprintf(filename, sizeof(filename), "frame-%d.pgm", frame_num);
    FILE *f = fopen(filename, "wb");
    if (!f) {
        av_log(NULL, AV_LOG_ERROR, "Could not open %s\n", filename);
        return;
    }

    // 写入 PGM 头
    fprintf(f, "P5\n%d %d\n255\n", frame->width, frame->height);

    // 写入像素数据 (Y plane)
    for (int y = 0; y < frame->height; y++) {
        // frame->data[0] 是 Y 分量的起始地址
        // frame->linesize[0] 是 Y 分量一行的字节数，可能比 width 大 (为了对齐)
        fwrite(frame->data[0] + y * frame->linesize[0], 1, frame->width, f);
    }

    fclose(f);
    av_log(NULL, AV_LOG_INFO, "Saved frame %d to %s\n", frame_num, filename);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <input_file>\n", argv[0]);
        return -1;
    }
    const char *input_filename = argv[1];

    av_log_set_level(AV_LOG_INFO);

    AVFormatContext *fmt_ctx = NULL;
    if (avformat_open_input(&fmt_ctx, input_filename, NULL, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return -1;
    }
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to find stream information\n");
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    av_dump_format(fmt_ctx, 0, input_filename, 0);

    int video_stream_index = -1;
    AVCodecParameters *codec_par = NULL;
    const AVCodec *codec = NULL;

    video_stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_stream_index < 0) {
        av_log(NULL, AV_LOG_ERROR, "No video stream found\n");
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    codec_par = fmt_ctx->streams[video_stream_index]->codecpar;
    codec = avcodec_find_decoder(codec_par->codec_id);
    if (!codec) {
        av_log(NULL, AV_LOG_ERROR, "Unsupported codec!\n");
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    if (avcodec_parameters_to_context(codec_ctx, codec_par) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to copy codec parameters to context\n");
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to open codec\n");
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    int frames_saved = 0;

    while (av_read_frame(fmt_ctx, packet) >= 0) {
        if (packet->stream_index == video_stream_index) {
            int ret = avcodec_send_packet(codec_ctx, packet);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Error sending a packet for decoding\n");
                break;
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(codec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break; // 需要更多 packet 或者已经结束
                } else if (ret < 0) {
                    av_log(NULL, AV_LOG_ERROR, "Error during decoding\n");
                    goto end;
                }

                if (frames_saved < 5) {
                    save_gray_frame(frame, frames_saved);
                    frames_saved++;
                } else {
                    goto end; // 已经保存够 5 帧，直接跳到结尾
                }
            }
        }
        av_packet_unref(packet);
    }

end:
    av_packet_free(&packet);
    av_frame_free(&frame);
    avcodec_close(codec_ctx);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);

    return 0;
}