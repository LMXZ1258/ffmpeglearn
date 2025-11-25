#include <stdio.h>
#include <libavformat/avformat.h>
#include <libavutil/log.h>

int main(int argc, char *argv[]) {
    // 1. 检查命令行参数
    if (argc < 2) {
        printf("Usage: %s <input_file>\n", argv[0]);
        return -1;
    }
    const char *input_filename = argv[1];

    // 设置日志级别，方便观察FFmpeg内部信息
    av_log_set_level(AV_LOG_INFO);

    // 2. 创建 AVFormatContext 结构体
    // AVFormatContext 是一个贯穿始终的结构体，它包含了媒体文件的所有信息。
    // 我们需要传递一个指向指针的指针，因为 avformat_open_input 会为我们分配内存。
    AVFormatContext *fmt_ctx = NULL;

    // 3. 打开媒体文件
    // avformat_open_input 会探测文件格式，打开文件，并读取文件头。
    int ret = avformat_open_input(&fmt_ctx, input_filename, NULL, NULL);
    if (ret < 0) {
        // av_strerror 用于将FFmpeg的错误码转换为可读的字符串
        char err_buf[128];
        av_strerror(ret, err_buf, sizeof(err_buf));
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file '%s': %s\n", input_filename, err_buf);
        return ret;
    }

    av_log(NULL, AV_LOG_INFO, "Successfully opened file: %s\n", input_filename);

    // 4. 读取流信息
    // 对于某些没有文件头的格式（如MPEG-TS），avformat_open_input可能无法获取到足够的信息。
    // avformat_find_stream_info 会读取并解码几帧数据来填充这些信息。
    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        char err_buf[128];
        av_strerror(ret, err_buf, sizeof(err_buf));
        av_log(NULL, AV_LOG_ERROR, "Failed to find stream information: %s\n", err_buf);
        avformat_close_input(&fmt_ctx); // 记得在出错时也要关闭
        return ret;
    }

    av_log(NULL, AV_LOG_INFO, "Stream information found.\n\n");

    // 5. 打印媒体文件的详细信息
    // 这是一个非常方便的调试函数，它会将 AVFormatContext 中的所有信息
    // (容器格式、时长、码率、流的数量、每个流的详细参数等) 格式化地打印到标准错误流(stderr)。
    // 参数：上下文，流索引(-1表示所有流)，文件名，是否为输出(0表示输入)
    av_dump_format(fmt_ctx, 0, input_filename, 0);

    // 6. 清理和关闭
    // avformat_close_input 会关闭文件，释放所有与 AVFormatContext 相关的内存，
    // 并将指针设置为 NULL。
    avformat_close_input(&fmt_ctx);

    av_log(NULL, AV_LOG_INFO, "\nCleanup finished.\n");

    return 0;
}