#include <stdio.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/log.h>

// 编码并写入一帧
static int encode_write_frame(AVFormatContext *fmt_ctx, AVCodecContext *codec_ctx, AVFrame *frame) {
    int ret;
    // 将 frame 发送给编码器
    ret = avcodec_send_frame(codec_ctx, frame);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error sending a frame for encoding\n");
        return ret;
    }

    while (ret >= 0) {
        AVPacket *packet = av_packet_alloc();
        if (!packet) return AVERROR(ENOMEM);

        // 从编码器接收 packet
        ret = avcodec_receive_packet(codec_ctx, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_packet_free(&packet);
            return (ret == AVERROR_EOF) ? 1 : 0; // 1 for EOF, 0 for EAGAIN
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error during encoding\n");
            av_packet_free(&packet);
            return ret;
        }

        // 将 packet 的时间戳从编码器的时间基转换为输出流的时间基
        av_packet_rescale_ts(packet, codec_ctx->time_base, fmt_ctx->streams[0]->time_base);
        packet->stream_index = 0;

        // 将 packet 写入输出文件
        ret = av_interleaved_write_frame(fmt_ctx, packet);
        av_packet_free(&packet);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error while writing output packet\n");
            return ret;
        }
    }
    return ret;
}

int main(int argc, char **argv) {
    const char *filename = "test.mp4";
    const int width = 640, height = 480;
    const int frame_rate = 25;
    const int total_frames = 200;

    av_log_set_level(AV_LOG_INFO);

    AVFormatContext *fmt_ctx = NULL;
    const AVCodec *codec = NULL;
    AVCodecContext *codec_ctx = NULL;
    AVStream *stream = NULL;
    int ret;

    // 1. 创建封装器上下文
    avformat_alloc_output_context2(&fmt_ctx, NULL, NULL, filename);
    if (!fmt_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
        return -1;
    }

    // 2. 查找编码器
    codec = avcodec_find_encoder_by_name("libx264");
    if (!codec) {
        av_log(NULL, AV_LOG_ERROR, "Codec 'libx264' not found\n");
        goto end;
    }

    // 3. 创建新的视频流
    stream = avformat_new_stream(fmt_ctx, codec);
    if (!stream) {
        av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
        goto end;
    }
    stream->id = fmt_ctx->nb_streams - 1;

    // 4. 分配和配置编码器上下文
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Could not alloc an encoding context\n");
        goto end;
    }

    codec_ctx->codec_id = codec->id;
    codec_ctx->width = width;
    codec_ctx->height = height;
    codec_ctx->time_base = (AVRational){1, frame_rate};
    codec_ctx->framerate = (AVRational){frame_rate, 1};
    codec_ctx->gop_size = 12; // I-frame interval
    codec_ctx->max_b_frames = 1;
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

    if (codec->id == AV_CODEC_ID_H264) {
        av_opt_set(codec_ctx->priv_data, "preset", "slow", 0);
    }

    // 5. 打开编码器
    if ((ret = avcodec_open2(codec_ctx, codec, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open video encoder\n");
        goto end;
    }

    // 6. 将编码器参数复制到流
    if ((ret = avcodec_parameters_from_context(stream->codecpar, codec_ctx)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to copy encoder parameters to stream\n");
        goto end;
    }
    stream->time_base = codec_ctx->time_base;

    av_dump_format(fmt_ctx, 0, filename, 1);

    // 7. 打开输出文件
    if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        if ((ret = avio_open(&fmt_ctx->pb, filename, AVIO_FLAG_WRITE)) < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'\n", filename);
            goto end;
        }
    }

    // 8. 写入文件头
    if ((ret = avformat_write_header(fmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
        goto end;
    }

    // 9. 准备 AVFrame 和 AVPacket
    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate video frame\n");
        goto end;
    }
    frame->format = codec_ctx->pix_fmt;
    frame->width = codec_ctx->width;
    frame->height = codec_ctx->height;
    if ((ret = av_frame_get_buffer(frame, 0)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate the video frame data\n");
        goto end;
    }

    // 10. 编码循环
    for (int i = 0; i < total_frames; i++) {
        // 确保 frame 可写
        if (av_frame_make_writable(frame) < 0) break;

        // 生成图像: YUV 彩色渐变
        // Y plane (Luma)
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                frame->data[0][y * frame->linesize[0] + x] = x + y + i * 3;
            }
        }
        // Cb and Cr planes (Chroma)
        for (int y = 0; y < height / 2; y++) {
            for (int x = 0; x < width / 2; x++) {
                frame->data[1][y * frame->linesize[1] + x] = 128 + y + i * 2;
                frame->data[2][y * frame->linesize[2] + x] = 64 + x + i * 5;
            }
        }

        frame->pts = i;
        
        if (encode_write_frame(fmt_ctx, codec_ctx, frame) < 0) {
            break;
        }
    }

    // 11. 刷新编码器
    encode_write_frame(fmt_ctx, codec_ctx, NULL);

    // 12. 写入文件尾
    av_write_trailer(fmt_ctx);
    av_log(NULL, AV_LOG_INFO, "Encode finished to %s\n", filename);

end:
    if (frame) av_frame_free(&frame);
    if (codec_ctx) avcodec_free_context(&codec_ctx);
    if (fmt_ctx) {
        if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&fmt_ctx->pb);
        }
        avformat_free_context(fmt_ctx);
    }

    return ret < 0 ? 1 : 0;
}