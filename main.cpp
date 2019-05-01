// main.cpp
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

std::string bb_videoread(Aws::String const& filename);  //, Aws::String& output);
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

    AWS_LOGSTREAM_INFO(TAG, "Attempting to download file from s3://" << bucket << "/" << key);

    Aws::String base64_encoded_file;
    auto err = bb_wrapper(client, bucket, key);  //, base64_encoded_file);
    if (!err.empty()) 
    {			
        using namespace Aws::DynamoDB::Model;
	
        PutItemRequest pir;
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
             return invocation_response::success("Success" /*put_result.GetError().GetMessage()*/, "application/json");
        }	    
	
        return invocation_response::success("Yea-ee Yeah, DB put done! " + err, "application/json");  //invocation_response::failure(err, "DownloadFailure")
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

    AVFormatContext *inputContext = avformat_alloc_context();

    inputContext->iformat = av_probe_input_format(pd, 1);
    result = avformat_find_stream_info(inputContext, NULL);
    if (result < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't get stream info\n");
        return result;
    }

    video_stream = av_find_best_stream(inputContext, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_stream < 0) {
      av_log(NULL, AV_LOG_ERROR, "Can't find video stream in input file\n");
      return -1;
    }

    
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

   bb_videodecode(videoBits);


    return {videoDataStr};
}

std::string bb_wrapper(
    Aws::S3::S3Client const& client,
    Aws::String const& bucket,
    Aws::String const& key)  //,
    //Aws::String& encoded_output)
{
    using namespace Aws;

    Aws::String fileContents;
    Aws::String tempVideoFile = "videoTempFile.mp4";
    S3::Model::GetObjectRequest request;
    request.WithBucket(bucket).WithKey(key);

	//{
	    auto outcome = client.GetObject(request);
		
	    if (outcome.IsSuccess()) 
	    {
	    
		/*
		Aws::IFStream downloadedFile(tempVideoFile.c_str());
		if (downloadedFile.good())
		{
			downloadedFile.seekg(0, std::ios::end);
		         fileContents.reserve(static_cast<uint32_t>(downloadedFile.tellg()));
	            	downloadedFile.seekg(0, std::ios::beg);
	            	fileContents.assign((std::istreambuf_iterator<char>(downloadedFile)), std::istreambuf_iterator<char>());
		}*/
			
	        AWS_LOGSTREAM_INFO(TAG, "Download completed!");
		/*std::istreambuf_iterator<char> eos;
		std::istreambuf_iterator<char> bos (outcome.GetResult().GetBody().rdbuf());
		videoVec = std::vector<char> (bos, eos);*/
	        auto& s = outcome.GetResult().GetBody();
	        return bb_videoread(s);  //, encoded_output);
	    }
	    else {
	        AWS_LOGSTREAM_ERROR(TAG, "Failed with error: " << outcome.GetError());
	        return "Will Have An Error";  //outcome.GetError().GetMessage();
	    }
	//}
}

int main()
{
    using namespace Aws;
    SDKOptions options;
    options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Trace;
    options.loggingOptions.logger_create_fn = GetConsoleLoggerFactory();
    InitAPI(options);
    {
        Aws::Client::ClientConfiguration config;
 		
        config.region = Aws::Environment::GetEnv("AWS_REGION");
        config.caFile = "/etc/pki/tls/certs/ca-bundle.crt";

        auto handler_fn = [&config](aws::lambda_runtime::invocation_request const& req) {
            return my_handler(req, config);   //client);
        };
        run_handler(handler_fn);

    }
    ShutdownAPI(options);
    return 0;
}

