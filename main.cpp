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

#include "FFMPEGHelper.h"

extern "C" {

#include <libavformat/avformat.h>

}

using namespace aws::lambda_runtime;

static const char* ALLOCATION_TAG = "BBChalk";

std::string bb_wrapper(
    Aws::S3::S3Client const& client,
    Aws::String const& bucket,
    Aws::String const& key);//,
    //Aws::String& encoded_output);

std::string bb_videoread(Aws::IOStream& stream);  //, Aws::String& output);
int bb_videodecode(Aws::Vector<unsigned char> video_bits);

char const TAG[] = "LAMBDA_ALLOC";



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
    //AWS_LOGSTREAM_INFO(TAG, "Attempting to download file from s3://" << bucket << "/" << key);

    Aws::String base64_encoded_file;
    auto err = bb_wrapper(client, bucket, key);  //, base64_encoded_file);
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


std::function<std::shared_ptr<Aws::Utils::Logging::LogSystemInterface>()> GetConsoleLoggerFactory()
{
    return [] {
        return Aws::MakeShared<Aws::Utils::Logging::ConsoleLogSystem>(
            "console_logger", Aws::Utils::Logging::LogLevel::Trace);
    };
}


int bb_videodecode(Aws::Vector<unsigned char> video_bits)
{
    int result;
    int video_stream;
    AVProbeData *pd;

    pd->buf = (unsigned char *) video_bits.data();
    pd->buf_size = (int) video_bits.size() - 1;
    pd->filename = "";

    AVFormatContext *inputContext = NULL;
    if (!(inputContext = avformat_alloc_context())) {
        return -12;
    }

    //inputContext->iformat = av_probe_input_format(pd, 1);
    if (!(inputContext->iformat = av_probe_input_format(pd, 1))) {
        return -23;
    }

    /*result = avformat_find_stream_info(inputContext, NULL);
    if (result < 0) {
        //av_log(NULL, AV_LOG_ERROR, "Can't get stream info\n");
        return -34;  //return result;
    }

    video_stream = av_find_best_stream(inputContext, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_stream < 0) {
      //av_log(NULL, AV_LOG_ERROR, "Can't find video stream in input file\n");
      return -45;    //return -1;
    }*/
    
    return 0;
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

   auto videoDataStr = std::to_string(videoBits.size());  //"loops=" + std::to_string(loops);
    //Aws::Utils::ByteBuffer bb(bits.data(), bits.size());
    //output = bitstr;   //Aws::Utils::HashingUtils::Base64Encode(bb);

    //int bbret = bb_videodecode(videoBits);
    //std::string bbret_str = std::to_string(bbret);


    return videoDataStr;   //bbret_str;  // {videoDataStr};
}

std::string bb_wrapper(
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
        auto& s = outcome.GetResult().GetBody();
        //return bb_videoread(s);

        /*std::istreambuf_iterator<char> eos;
        std::istreambuf_iterator<char> bos (outcome.GetResult().GetBody().rdbuf());
        std::vector<char> videoData = std::vector<char> (bos, eos);*/

        std::stringstream ss;
        ss << outcome.GetResult().GetBody().rdbuf();
        std::string str = ss.str();
        std::vector<char> videoData(str.begin(), str.end());

        /*av_register_all();

        int ret;

        FFMPEGHelper ffHelper((unsigned char *) videoData.data(), videoData.size());

        AVFormatContext *avFmtCtx = avformat_alloc_context(); 

        ffHelper.setAVFormatContext(avFmtCtx);

        ret = avformat_open_input(&avFmtCtx, "dummy", nullptr, nullptr);

        if (ret < 0)
        {
            return "avformat_open_input failed";
        }*/

        std::ofstream outfile ("/tmp/videofile.mp4", std::ofstream::binary);
        char val;
        for (int i=0; i < videoData.size(); i++)
        {
            val = (char) videoData[i];

            //outfile << videoData[i];
            outfile.write( reinterpret_cast<char *>(&val), sizeof(val) );
        }
        outfile.close();

        long outfileSize = videoData.size();
        videoData.clear();

        std::vector<char> rVideoData;
        std::ifstream infile ("/tmp/videofile.mp4", std::ifstream::binary);

        /*long in_length;
        if (infile)
        {
            // get length of file
            infile.seekg(0, infile.end);
            in_length = (long) infile.tellg();
            infile.seekg(0, infile.beg);
        }*/


        char temp_input;
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
        return str_out;
    }
    else 
    {
        AWS_LOGSTREAM_ERROR(TAG, "Failed with error: " << outcome.GetError());
        return "Error in bb_wrapper";  //outcome.GetError().GetMessage();
    }
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

