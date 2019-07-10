// main.cpp
#include <stdio.h>
#include <aws/core/Aws.h>
#include <aws/core/utils/Outcome.h> 
#include <aws/dynamodb/DynamoDBClient.h>
#include <aws/dynamodb/model/AttributeDefinition.h>
#include <aws/dynamodb/model/PutItemRequest.h>
#include <aws/dynamodb/model/PutItemResult.h>
#include <aws/core/utils/logging/LogLevel.h>
#include <aws/core/utils/logging/ConsoleLogSystem.h>
#include <aws/core/utils/logging/LogMacros.h>
#include <aws/core/utils/json/JsonSerializer.h>
#include <aws/core/utils/StringUtils.h>
#include <aws/core/utils/HashingUtils.h>
#include <aws/core/platform/Environment.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/lambda-runtime/runtime.h>
#include <vector>
#include <iterator>
#include <iostream>
#include <fstream>
#include <memory>
#include <string>

#include "BBController.hpp"

extern "C" {

#include <libavutil/adler32.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavformat/avio.h>
#include <libavutil/file.h>
#include <libswscale/swscale.h>

}

struct buffer_data {
    uint8_t *ptr;
    size_t size; ///< size left in the buffer
};

using namespace aws::lambda_runtime;

static const char* ALLOCATION_TAG = "BBChalk";
BBController bCtrl;

std::string bb_video_process(
    Aws::S3::S3Client const& client,
    Aws::String const& bucket,
    Aws::String const& key);

std::string bb_load_court_and_model(Aws::S3::S3Client const& client, 
                            Aws::String const& bucket, 
                            Aws::String const& key_court, 
                            Aws::String const& key_body, 
                            Aws::String const& key_model, 
                            Aws::String const& key_cfg,
                            Aws::String const& key_names);

std::string bb_videoread(Aws::IOStream& stream);  //, Aws::String& output);

char const TAG[] = "LAMBDA_ALLOC";

static int read_packet(void *opaque, uint8_t *buf, int buf_size)
{
    struct buffer_data *bd = (struct buffer_data *)opaque;
    buf_size = FFMIN(buf_size, bd->size);

    if (!buf_size)
        return AVERROR_EOF;
    printf("ptr:%p size:%zu\n", bd->ptr, bd->size);

    /* copy internal buffer data to buf */
    memcpy(buf, bd->ptr, buf_size);
    bd->ptr  += buf_size;
    bd->size -= buf_size;

    return buf_size;
}

static invocation_response my_handler(invocation_request const& req, 
                            //Aws::S3::S3Client const& client)
                            Aws::Client::ClientConfiguration config)
{
    using namespace Aws::Utils::Json;
    using namespace Aws::S3;
    using namespace Aws::DynamoDB;

    auto credentialsProvider = Aws::MakeShared<Aws::Auth::EnvironmentAWSCredentialsProvider>(TAG);
    S3Client client(credentialsProvider, config);
    DynamoDBClient dynamoClient(credentialsProvider, config);

    JsonValue json(req.payload);
    if (!json.WasParseSuccessful()) 
    {
        return invocation_response::failure("Failed to parse input JSON", "InvalidJSON");
    }

    auto v = json.View();

    if (!v.ValueExists("s3bucket") || !v.ValueExists("s3key") || !v.GetObject("s3bucket").IsString() ||
        !v.GetObject("s3key").IsString()) 
    {
        return invocation_response::failure("Missing input value s3bucket or s3key", "InvalidJSON");
    }

    auto bucket = v.GetString("s3bucket");
    auto key = v.GetString("s3key");
    auto key_body = "haarcascade_fullbody.xml";
    auto key_court = "bball-half-court-vga.jpeg";
    auto key_model = "made_8200.weights";
    auto key_cfg = "made.cfg";
    //AWS_LOGSTREAM_INFO(TAG, "Attempting to download file from s3://" << bucket << "/" << key);

    auto err_config = bb_load_court_and_model(client, 
                                        bucket, 
                                        "bball-half-court-vga.jpeg", 
                                        "haarcascade_fullbody.xml", 
                                        "made_8200.weights", 
                                        "made.cfg",
                                        "made.names");

    auto err = bb_video_process(client, 
                                bucket, 
                                key);
    if (!err.empty()) 
    {           
        using namespace Aws::DynamoDB::Model;
    
        /*PutItemRequest pir;
        pir.SetTableName("truchalkdb-1");
        AttributeValue av;
        av.SetS("FrenchHorn");
        pir.AddItem("UserName", av);
        AttributeValue val;
        val.SetS("StepBack");
        pir.AddItem("RecordName", val);
        val.SetS("800");
        pir.AddItem("TotalShots", val);
        val.SetS("500");
        pir.AddItem("ShotsMissed", val);
        val.SetS("300");
        pir.AddItem("ShotsMade", val);

        const PutItemOutcome put_result = dynamoClient.PutItem(pir);

        if (!put_result.IsSuccess())
        {
             return invocation_response::success("Success", "application/json");
             //return invocation_response::success(put_result.GetError().GetMessage(), "application/json");
        }*/    
    
        return invocation_response::success("Our error messaging: " + err, "application/json");  //invocation_response::failure(err, "DownloadFailure")
    }
    else if (!err_config.empty())
        return invocation_response::success("Our error messaging: " + err_config, "application/json");


    return invocation_response::success("bb_video_process return with empty results.  Not Good.", "application/json"); //invocation_response::success(base64_encoded_file, "application/base64");
}


std::function<std::shared_ptr<Aws::Utils::Logging::LogSystemInterface>()> GetConsoleLoggerFactory()
{
    return [] {
        return Aws::MakeShared<Aws::Utils::Logging::ConsoleLogSystem>(
            "console_logger", Aws::Utils::Logging::LogLevel::Trace);
    };
}


std::string bb_videoread(Aws::IOStream& stream)  //, Aws::String& output)
{
    Aws::Vector<unsigned char> videoBits;
    videoBits.reserve(stream.tellp());
    stream.seekg(0, stream.beg);
    
    char streamBuffer[4096 * 4];  //[1024 * 4];
    auto loops = 0;

    while (stream.good()) {
        loops++;

        stream.read(streamBuffer, sizeof(streamBuffer));
        auto bytesRead = stream.gcount();

        if (bytesRead > 0) {
            videoBits.insert(videoBits.end(), (unsigned char*)streamBuffer, (unsigned char*)streamBuffer + bytesRead);
        }
    }

    auto videoDataStr = std::to_string(videoBits.size());  

    return videoDataStr;
}


std::string bb_load_court_and_model(Aws::S3::S3Client const& client, 
                            Aws::String const& bucket, 
                            Aws::String const& key_court, 
                            Aws::String const& key_body, 
                            Aws::String const& key_model, 
                            Aws::String const& key_cfg,
                            Aws::String const& key_names)
{
    using namespace Aws;

    Aws::S3::Model::GetObjectRequest request_court;
    request_court.WithBucket(bucket).WithKey(key_court);

    auto outcome_court = client.GetObject(request_court);
    if (outcome_court.IsSuccess()) 
    {
        std::stringstream ss;
        ss << outcome_court.GetResult().GetBody().rdbuf();
        std::string str = ss.str();
        std::vector<char> bitData(str.begin(), str.end());

        std::ofstream outfile ("/tmp/bball-half-court-vga.jpg", std::ofstream::binary);
        char val;
        for (int i=0; i < bitData.size(); i++)
        {
            val = (char) bitData[i];
            outfile.write( reinterpret_cast<char *>(&val), sizeof(val) );
        }
        outfile.close();
        bitData.clear();
    }
    else 
    {
        return("Error in get bball-half-court-vga.jpeg");
    }


    Aws::S3::Model::GetObjectRequest request_body;
    request_body.WithBucket(bucket).WithKey(key_body);

    auto outcome_body = client.GetObject(request_body);
    if (outcome_body.IsSuccess()) 
    {
        std::stringstream ss;
        ss << outcome_body.GetResult().GetBody().rdbuf();
        std::string str = ss.str();
        std::vector<char> bitData(str.begin(), str.end());

        std::ofstream outfile ("/tmp/haarcascade_fullbody.xml", std::ofstream::binary);
        char val;
        for (int i=0; i < bitData.size(); i++)
        {
            val = (char) bitData[i];
            outfile.write( reinterpret_cast<char *>(&val), sizeof(val) );
        }
        outfile.close();
        bitData.clear();
    }
    else 
    {
        return "Error in getting haarcascade_fullbody.xml";
    }


    Aws::S3::Model::GetObjectRequest request_model;
    request_model.WithBucket(bucket).WithKey(key_model);

    auto outcome_model = client.GetObject(request_model);
    if (outcome_model.IsSuccess()) 
    {
        std::stringstream ss;
        ss << outcome_model.GetResult().GetBody().rdbuf();
        std::string str = ss.str();
        std::vector<char> bitData(str.begin(), str.end());

        std::ofstream outfile ("/tmp/made_8200.weights", std::ofstream::binary);
        char val;
        for (int i=0; i < bitData.size(); i++)
        {
            val = (char) bitData[i];
            outfile.write( reinterpret_cast<char *>(&val), sizeof(val) );
        }
        outfile.close();
        bitData.clear();
    }
    else 
    {
        return "Error in getting made_8200.weights";
    }


    Aws::S3::Model::GetObjectRequest request_cfg;
    request_cfg.WithBucket(bucket).WithKey(key_cfg);

    auto outcome_cfg = client.GetObject(request_cfg);
    if (outcome_cfg.IsSuccess()) 
    {
        std::stringstream ss;
        ss << outcome_cfg.GetResult().GetBody().rdbuf();
        std::string str = ss.str();
        std::vector<char> bitData(str.begin(), str.end());

        std::ofstream outfile ("/tmp/made.cfg", std::ofstream::binary);
        char val;
        for (int i=0; i < bitData.size(); i++)
        {
            val = (char) bitData[i];
            outfile.write( reinterpret_cast<char *>(&val), sizeof(val) );
        }
        outfile.close();
        bitData.clear();
    }
    else 
    {
        return "Error in getting made.cfg";
    }

    Aws::S3::Model::GetObjectRequest request_names;
    request_names.WithBucket(bucket).WithKey(key_names);

    auto outcome_names = client.GetObject(request_names);
    if (outcome_names.IsSuccess()) 
    {
        std::stringstream ss;
        ss << outcome_names.GetResult().GetBody().rdbuf();
        std::string str = ss.str();
        std::vector<char> bitData(str.begin(), str.end());

        std::ofstream outfile ("/tmp/made.names", std::ofstream::binary);
        char val;
        for (int i=0; i < bitData.size(); i++)
        {
            val = (char) bitData[i];
            outfile.write( reinterpret_cast<char *>(&val), sizeof(val) );
        }
        outfile.close();
        bitData.clear();
    }
    else 
    {
        return "Error in getting made.names";
    }



    //BBController bCtrl;

    int ret = bCtrl.initialize();
    if (ret < 0)
        return("Error trying to initialize bCtrl");


    return("Success for bb_load_court_and_model");
}


std::string bb_video_process(
    Aws::S3::S3Client const& client,
    Aws::String const& bucket,
    Aws::String const& key)
{
    using namespace Aws;

    //BBController bCtrl;
    std::string fret_str;

    Aws::S3::Model::GetObjectRequest request;
    request.SetBucket(bucket);
    request.SetKey(key);
    //request.WithBucket(bucket).WithKey(key);

    int result;
    int video_stream;
    int end_of_stream = 0;
    int got_frame = 0;
    int byte_buffer_size;
    int number_of_written_bytes;
    int frame_count = 0;
    int i = 0;
    uint8_t *byte_buffer = NULL;
    struct buffer_data bd = { 0 };
    Aws::Vector<uint8_t> videoBits;
    uint8_t *avio_ctx_buffer = NULL;
    size_t avio_ctx_buffer_size = 4096;
    AVIOContext *avio_ctx = NULL;
    AVFormatContext *fmt_ctx = NULL;
    AVCodecParameters *origin_par = NULL;
    AVCodec *codec = NULL;
    AVCodecContext *ctx= NULL;
    AVFrame *fr = NULL;
    AVPacket pkt;

    auto outcome = client.GetObject(request);
    if (outcome.IsSuccess()) 
    {
        auto& s = outcome.GetResult().GetBody();

        videoBits.reserve(s.tellp());
        s.seekg(0, s.beg);
        char streamBuffer[4096 * 4];  //[1024 * 4];
        while (s.good()) 
        {
            s.read(streamBuffer, sizeof(streamBuffer));
            auto bytesRead = s.gcount();

            if (bytesRead > 0) 
            {
                videoBits.insert(videoBits.end(), (uint8_t *)streamBuffer, (uint8_t *)streamBuffer + bytesRead);
            }

            /* fill opaque structure used by the AVIOContext read callback */
            bd.ptr  = static_cast<uint8_t *> (videoBits.data());
            bd.size = videoBits.size();

        }
        cout << __LINE__ << "AWS:  WE should have stream now" << endl;
    }

    if (!(fmt_ctx = avformat_alloc_context())) {
        result = AVERROR(ENOMEM);
        return std::to_string(-5);
    }

    avio_ctx_buffer = static_cast<uint8_t *>(av_malloc(avio_ctx_buffer_size));
    if (!avio_ctx_buffer) {
        result = AVERROR(ENOMEM);
        return std::to_string(-4);
    }
    
    avio_ctx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size,
                                  0, &bd, &read_packet, NULL, NULL);
    if (!avio_ctx) {
        result = AVERROR(ENOMEM);
        return std::to_string(-3);
    }
    fmt_ctx->pb = avio_ctx;

    result = avformat_open_input(&fmt_ctx, NULL, NULL, NULL);
    if (result < 0) {
        fprintf(stderr, "Could not open input\n");
        return std::to_string(-2);
    }

    result = avformat_find_stream_info(fmt_ctx, NULL);
    if (result < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't get stream info\n");
        return std::to_string(result);
    }

    video_stream = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_stream < 0) {
      av_log(NULL, AV_LOG_ERROR, "Can't find video stream in input file\n");
      return std::to_string(-1);
    }

    origin_par = fmt_ctx->streams[video_stream]->codecpar;

    codec = avcodec_find_decoder(origin_par->codec_id);
    if (!codec) {
        av_log(NULL, AV_LOG_ERROR, "Can't find decoder\n");
        return std::to_string(-1);
    }

    ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        av_log(NULL, AV_LOG_ERROR, "Can't allocate decoder context\n");
        return std::to_string(AVERROR(ENOMEM));
    }

    result = avcodec_parameters_to_context(ctx, origin_par);
    if (result) {
        av_log(NULL, AV_LOG_ERROR, "Can't copy decoder context\n");
        return std::to_string(result);
    }

    result = avcodec_open2(ctx, codec, NULL);
    if (result < 0) {
        av_log(ctx, AV_LOG_ERROR, "Can't open decoder\n");
        return std::to_string(result);
    }

    fr = av_frame_alloc();
    if (!fr) {
        av_log(NULL, AV_LOG_ERROR, "Can't allocate frame\n");
        return std::to_string(AVERROR(ENOMEM));
    }

    byte_buffer_size = av_image_get_buffer_size(ctx->pix_fmt, ctx->width, ctx->height, 16);
    byte_buffer = (uint8_t *) av_malloc(byte_buffer_size);
    if (!byte_buffer) {
        av_log(NULL, AV_LOG_ERROR, "Can't allocate buffer\n");
        return std::to_string(AVERROR(ENOMEM));
    }

    printf("#tb %d: %d/%d\n", video_stream, fmt_ctx->streams[video_stream]->time_base.num, fmt_ctx->streams[video_stream]->time_base.den);
    i = 0;
    av_init_packet(&pkt);
    do {
        if (!end_of_stream)
            if (av_read_frame(fmt_ctx, &pkt) < 0)
                end_of_stream = 1;
        if (end_of_stream) {
            pkt.data = NULL;
            pkt.size = 0;
        }
        if (pkt.stream_index == video_stream || end_of_stream) 
        {
            got_frame = 0;
            if (pkt.pts == AV_NOPTS_VALUE)
                pkt.pts = pkt.dts = i;

            result = avcodec_decode_video2(ctx, fr, &got_frame, &pkt);
            if (result < 0) 
            {
                av_log(NULL, AV_LOG_ERROR, "Error decoding frame\n");
                return std::to_string(result);
            }

            if (got_frame) 
            {
                number_of_written_bytes = av_image_copy_to_buffer(byte_buffer, byte_buffer_size,
                                        (const uint8_t* const *)fr->data, (const int*) fr->linesize,
                                        ctx->pix_fmt, ctx->width, ctx->height, 1);
                if (number_of_written_bytes < 0) 
                {
                    av_log(NULL, AV_LOG_ERROR, "Can't copy image to buffer\n");
                    return std::to_string(number_of_written_bytes);
                }

                Mat img(ctx->height, ctx->width, CV_8UC1, fr->data[0], fr->linesize[0]); 
                //Mat imgYUV((ctx->height + ctx->height/2), ctx->width, CV_8UC1, fr->data[0], fr->linesize[0]);
                //cvtColor(imgYUV, imgRGB, COLOR_YUV2RGB_NV12);

                uint8_t p = img.at<uint8_t>(200, 200);
                printf("(img ristics): %d  :  p=%d \n", 
                                                frame_count, 
                                                p );
               
                printf("(img ristics): %d  :  channels=%d    width=%d    height=%d\n", 
                                                                        frame_count, 
                                                                        img.channels(),
                                                                        img.cols,
                                                                        img.rows );
                
                printf("%d:  %d, %10"PRId64", %10"PRId64", %8"PRId64", %8d, 0x%08lx\n", 
                    frame_count++,
                    video_stream,
                        fr->pts, 
                        fr->pkt_dts, 
                        fr->pkt_duration,
                        number_of_written_bytes, 
                        av_adler32_update(0, (const uint8_t*)byte_buffer, number_of_written_bytes)
                        );

                //result = bCtrl.process(img);
            }
            av_packet_unref(&pkt);
            av_init_packet(&pkt);
        }
        i++;
    } while (!end_of_stream || got_frame);

    av_packet_unref(&pkt);
    av_frame_free(&fr);
    avcodec_close(ctx);
    avformat_close_input(&fmt_ctx);
    avcodec_free_context(&ctx);
    av_freep(&byte_buffer);


    return("bb_video_process Done");
}

int main()
{
    using namespace Aws;
    Aws::SDKOptions options;
    //options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Trace;
    //options.loggingOptions.logger_create_fn = GetConsoleLoggerFactory();
    Aws::InitAPI(options);
    {
        Aws::Client::ClientConfiguration config;
        
        config.region = Aws::Environment::GetEnv("AWS_REGION");
        config.caFile = "/etc/pki/tls/certs/ca-bundle.crt";

        auto handler_fn = [config](aws::lambda_runtime::invocation_request const& req) {
            return my_handler(req, config);   //client);
        };
        run_handler(handler_fn);

    }
    ShutdownAPI(options);
    return 0;
}

