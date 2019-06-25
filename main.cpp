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
#include "opencv2/core/core.hpp"
#include <vector>
#include <iterator>
#include <iostream>
#include <fstream>
#include <memory>
#include <string>

#include "BBController.hpp"

extern "C" {

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/file.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

}

using namespace std;
using namespace aws::lambda_runtime;

struct buffer_data 
{
    uint8_t *ptr;
    size_t size; ///< size left in the buffer
};


static const char* ALLOCATION_TAG = "BBChalk";

#define INBUF_SIZE 4096

static int read_packet(void *opaque, uint8_t *buf, int buf_size);
int bb_videoread(Aws::IOStream& stream, uint8_t **bufptr, size_t *size);
std::string bb_chalk(
    Aws::S3::S3Client const& client,
    Aws::String const& bucket,
    Aws::String const& key);
std::string bb_videowrapper(Aws::IOStream& stream);
static int bb_videodecode(AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt,
                   const char *filename);



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



int bb_videoread(Aws::IOStream& stream, uint8_t **bufptr, size_t *size)
{
    Aws::Vector<uint8_t> videoBits;
    videoBits.reserve(stream.tellp());
    stream.seekg(0, stream.beg);
    
    char streamBuffer[4096 * 4];  //[1024 * 4];
    auto loops = 0;

    while (stream.good()) {
        loops++;

        stream.read(streamBuffer, sizeof(streamBuffer));
        auto bytesRead = stream.gcount();

        if (bytesRead > 0) {
            videoBits.insert(videoBits.end(), (uint8_t *)streamBuffer, (uint8_t *)streamBuffer + bytesRead);
        }
    }

    *size = videoBits.size();
    *bufptr = videoBits.data();
    
    return ((int)videoBits.size());
}



std::function<std::shared_ptr<Aws::Utils::Logging::LogSystemInterface>()> GetConsoleLoggerFactory()
{
    return [] {
        return Aws::MakeShared<Aws::Utils::Logging::ConsoleLogSystem>(
            "console_logger", Aws::Utils::Logging::LogLevel::Trace);
    };
}



int bb_videodecode(AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt)
{
    BBController bCtrl;
    char buf[1024];
    int dst_w = 640;
    int dst_h = 480;
    enum AVPixelFormat dst_pix_fmt = AV_PIX_FMT_YUV420P;
    struct SwsContext *sws_ctx;
    AVFrame *dstFrame;
    int ret, fret;
    bool firstTime = true;
    int fcount = 0;

    dstFrame = av_frame_alloc();

    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0)
        return -21;


    while (ret >= 0) 
    {
        ret = avcodec_receive_frame(dec_ctx, frame);
        
        if (ret == AVERROR(EAGAIN))
            return -22;
        else if (ret == AVERROR_EOF)
            return -23;
        else if (ret < 0)
            return -24;

        if (firstTime)
        {
            sws_ctx = sws_getContext(frame->width, frame->height, dec_ctx->pix_fmt,
                                        dst_w, dst_h, AV_PIX_FMT_YUV420P,
                                        SWS_BICUBIC, NULL, NULL, NULL);
            firstTime = false;
        }

        // frame should be filled by now (eg using avcodec_decode_video)
        sws_scale(sws_ctx, frame->data, frame->linesize, 
            0, frame->height, dstFrame->data, dstFrame->linesize);

        cv::Mat frameMat = cv::Mat(dstFrame->height * 3 / 2, dstFrame->width, CV_8UC1, dstFrame->data[0]);
        fret = bCtrl.process_frame(frameMat);

        fcount++;
    }

    return fcount;
}



std::string bb_videowrapper(Aws::IOStream& stream)
{
    AVFormatContext *fmt_ctx = NULL;
    AVIOContext *avio_ctx = NULL;
    AVCodecContext *avctx;
    uint8_t *buffer = NULL, *avio_ctx_buffer = NULL;
    size_t buffer_size, avio_ctx_buffer_size = 4096;
    char *input_filename = NULL;
    int ret = 0;
    struct buffer_data bd = { 0 };

    Aws::Vector<uint8_t> videoBits;
    videoBits.reserve(stream.tellp());
    stream.seekg(0, stream.beg);
    
    char streamBuffer[4096 * 4];  //[1024 * 4];
    auto loops = 0;

    while (stream.good()) {
        loops++;

        stream.read(streamBuffer, sizeof(streamBuffer));
        auto bytesRead = stream.gcount();

        if (bytesRead > 0) {
            videoBits.insert(videoBits.end(), (uint8_t *)streamBuffer, (uint8_t *)streamBuffer + bytesRead);
        }
    }

    /* fill opaque structure used by the AVIOContext read callback */
    bd.ptr  = reinterpret_cast<uint8_t *> (videoBits.data());
    bd.size = videoBits.size();

    if (!(fmt_ctx = avformat_alloc_context())) 
    {
        return("ENOMEM for avformat_alloc_context");
    }

    
    avio_ctx_buffer = reinterpret_cast<uint8_t *> (av_malloc(avio_ctx_buffer_size));
    if (!avio_ctx_buffer) 
    {
        return("ENOMEM for av_malloc");
    }
    avio_ctx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size,
                                  0, &bd, &read_packet, NULL, NULL);
    if (!avio_ctx) 
    {
        return("ENOMEM for avio_alloc_context");
    }
    fmt_ctx->pb = avio_ctx;

    ret = avformat_open_input(&fmt_ctx, NULL, NULL, NULL);
    if (ret < 0) 
    {
        return("Failure for avformat_open_input");
    }

    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) 
    {
        return("Could not find stream information");
    }

    avctx = avcodec_alloc_context3(NULL);
    if (!avctx)
        return("Failure for avcodec_alloc_context3");

    AVStream *st = fmt_ctx->streams[0];  //Note: We only need first stream
    ret = avcodec_parameters_to_context(avctx, st->codecpar);
    if (ret < 0) {
        avcodec_free_context(&avctx);
        return("Failure for avcodec_parameters_to_context");
    }

    // Fields which are missing from AVCodecParameters need to be taken from the AVCodecContext
    avctx->properties = st->codec->properties;
    avctx->codec      = st->codec->codec;
    avctx->qmin       = st->codec->qmin;
    avctx->qmax       = st->codec->qmax;
    avctx->coded_width  = st->codec->coded_width;
    avctx->coded_height = st->codec->coded_height;

    auto temp_str = std::to_string(st->codec->coded_width);
    return temp_str;
    //return("bb_wrapper_Success");
}


std::string bb_chalk(
    Aws::S3::S3Client const& client,
    Aws::String const& bucket,
    Aws::String const& key)
{
    using namespace Aws;

    Aws::S3::Model::GetObjectRequest request;
    request.WithBucket(bucket).WithKey(key);

    auto outcome = client.GetObject(request);
	
    if (outcome.IsSuccess()) 
    {
        auto& stream = outcome.GetResult().GetBody();
        std::string str_ret = bb_videowrapper(stream);
        return str_ret;

        /*** NOTES:  One way of extracting and casting stream to a char pointer.  Not using it. ***/
        /*std::stringstream ss;
        ss << outcome.GetResult().GetBody().rdbuf();
        std::string str = ss.str();
        std::vector<char> videoData(str.begin(), str.end());*/
        /*** NOTES:  One way of extracting and casting stream to a char pointer.  Not using it. ***/


        /***************START: Writing video file in /tmp ***********/
        /*std::ofstream outfile ("/tmp/videofile.mp4", std::ofstream::binary);
        char val;
        for (int i=0; i < videoData.size(); i++)
        {
            val = (char) videoData[i];
            outfile.write( reinterpret_cast<char *>(&val), sizeof(val) );
        }
        outfile.close();

        long outfileSize = videoData.size();
        videoData.clear();*/
        /***************END: Writing video file in /tmp  ***********/



        /***************START: Reading video file from /tmp  ***********/
        /*std::vector<char> rVideoData;
        std::ifstream infile ("/tmp/videofile.mp4", std::ifstream::binary);*/

        /*long in_length;
        if (infile)
        {
            // get length of file
            infile.seekg(0, infile.end);
            in_length = (long) infile.tellg();
            infile.seekg(0, infile.beg);
        }*/


        /*char temp_input;
        while( infile.read( reinterpret_cast<char *>(&temp_input), sizeof(temp_input) ) )
        {
            rVideoData.push_back(temp_input);
        }
        infile.close();

        std::string str_out;
        long inSize = (long) rVideoData.size();
        std::string strInSize = std::to_string(inSize);
        str_out = "rVideoData-size:" + strInSize; 
        //str_out = "rVideoData-size:" + std::to_string(in_length);
        return str_out;*/

        /***************END: Reading video file from /tmp  ***********/

    }
    else 
    {
        AWS_LOGSTREAM_ERROR(TAG, "Failed with error: " << outcome.GetError());
        return "Error in bb_wrapper";  //outcome.GetError().GetMessage();
    }
}



static invocation_response my_handler(invocation_request const& req, 
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
    //AWS_LOGSTREAM_INFO(TAG, "Attempting to download file from s3://" << bucket << "/" << key);

    Aws::String base64_encoded_file;

    auto err = bb_chalk(client, bucket, key);  //, base64_encoded_file);


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

    return invocation_response::success("bb_wrapper return with empty results.  Not Good.", "application/json"); //invocation_response::success(base64_encoded_file, "application/base64");
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
            return my_handler(req, config);
        };
        run_handler(handler_fn);

    }
    ShutdownAPI(options);
    return 0;
}

