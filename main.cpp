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

//#include "FFMPEGHelper.h"

extern "C" {

#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

}

using namespace aws::lambda_runtime;

static const char* ALLOCATION_TAG = "BBChalk";

#define INBUF_SIZE 4096

std::string bb_chalk(
    Aws::S3::S3Client const& client,
    Aws::String const& bucket,
    Aws::String const& key);
int bb_videowrapper(Aws::IOStream& stream);
static int bb_videodecode(AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt,
                   const char *filename);


std::string bb_videoread(Aws::IOStream& stream);

char const TAG[] = "LAMBDA_ALLOC";



std::string bb_videoread(Aws::IOStream& stream)
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



std::function<std::shared_ptr<Aws::Utils::Logging::LogSystemInterface>()> GetConsoleLoggerFactory()
{
    return [] {
        return Aws::MakeShared<Aws::Utils::Logging::ConsoleLogSystem>(
            "console_logger", Aws::Utils::Logging::LogLevel::Trace);
    };
}



int bb_videodecode(AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt)
{
    char buf[1024];
    int dst_w = 640;
    int dst_h = 480;
    enum AVPixelFormat dst_pix_fmt = AV_PIX_FMT_YUV420P;
    struct SwsContext *sws_ctx;
    AVFrame *dstFrame;
    int ret;
    bool firstTime = true;
    int resizedHeight = 0;


    dstFrame = av_frame_alloc();


    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0)
        return -21;



    while (ret >= 0) 
    {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return -22;
        else if (ret < 0)
            return -23;

        if (firstTime)
        {
            sws_ctx = sws_getContext(frame->width, frame->height, dec_ctx->pix_fmt,
                                        dst_w, dst_h, AV_PIX_FMT_YUV420P,
                                        SWS_BICUBIC, NULL, NULL, NULL);
            firstTime = false;
            resizedHeight = frame->height;
        }

        // frame1 should be filled by now (eg using avcodec_decode_video)
        sws_scale(sws_ctx, frame->data, frame->linesize, 
            0, resizedHeight, dstFrame->data, dstFrame->linesize);


        //sws_scale(sws_ctx, ((AVPicture*)pFrame)->data, ((AVPicture*)pFrame)->linesize, 0, pCodecCtx->height, ((AVPicture *)pFrameRGB)->data, ((AVPicture *)pFrameRGB)->linesize);
        /* convert to destination format */
        //sws_scale(sws_ctx, (const uint8_t * const*)src_data, src_linesize, 0, src_h, dst_data, dst_linesize);

        //printf("saving frame %3d\n", dec_ctx->frame_number);
        //fflush(stdout);
    }

    return 3012;
}



int bb_videowrapper(Aws::IOStream& stream)
{
    const AVCodec *codec;
    AVCodecParserContext *parser;
    AVCodecContext *c = NULL;
    AVFrame *frame;
    /*uint8_t*/  char inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    uint8_t *data;
    size_t data_size;
    int ret;
    AVPacket *pkt;

    pkt = av_packet_alloc();
    if (!pkt)
        return -11;

    /* set end of buffer to 0 (this ensures that no overreading happens for damaged MPEG streams) */
    memset(inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);

    /* find the MPEG-1 video decoder */
    codec = avcodec_find_decoder(AV_CODEC_ID_MPEG1VIDEO);
    if (!codec)
        return -12;

    parser = av_parser_init(codec->id);
    if (!parser)
        return -13;

    c = avcodec_alloc_context3(codec);
    if (!c)
        return -14;

    if (avcodec_open2(c, codec, NULL) < 0)
        return -15;

    frame = av_frame_alloc();
    if (!frame)
        return -16;


    /* Loop through the raw video data and decode each frame */
    stream.seekg(0, stream.beg);
    
    //char streamBuffer[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];   //[4096];


    while (stream.good()) 
    {
        /* read raw data from the IOStream */
        stream.read(inbuf, INBUF_SIZE);
        
        data_size = stream.gcount();

        data = reinterpret_cast<uint8_t *> (inbuf); 

        while (data_size > 0)
        {
            ret = av_parser_parse2(parser, c, &pkt->data, &pkt->size,
                                data, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);

            if (ret < 0)
                return -17;

            data      += ret;
            data_size -= ret;

            if (pkt->size) {
                bb_videodecode(c, frame, pkt);
            }

        }
    }
    /* flush the decoder */
    bb_videodecode(c, frame, NULL);

    av_parser_close(parser);
    avcodec_free_context(&c);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    
    return 1000;
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
        int ret = bb_videowrapper(stream);
        return std::to_string(ret);

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

