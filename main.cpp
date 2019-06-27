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


using namespace aws::lambda_runtime;

static const char* ALLOCATION_TAG = "BBChalk";

std::string bb_video_process(
    Aws::S3::S3Client const& client,
    Aws::String const& bucket,
    Aws::String const& key);

std::string bb_load_court_and_model(Aws::S3::S3Client const& client, 
                            Aws::String const& bucket, 
                            Aws::String const& key_court, 
                            Aws::String const& key_body, 
                            Aws::String const& key_model, 
                            Aws::String const& key_cfg);

std::string bb_videoread(Aws::IOStream& stream);  //, Aws::String& output);

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
                                        "made.cfg");

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
                            Aws::String const& key_cfg)
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



    BBController bCtrl;

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
    request.WithBucket(bucket).WithKey(key);

    auto outcome = client.GetObject(request);
    
    if (outcome.IsSuccess()) 
    {
        auto& s = outcome.GetResult().GetBody();

        std::stringstream ss;
        ss << outcome.GetResult().GetBody().rdbuf();
        std::string str = ss.str();
        std::vector<char> bitData(str.begin(), str.end());

        std::ofstream outfile ("/tmp/videofile.mp4", std::ofstream::binary);
        char val;
        for (int i=0; i < bitData.size(); i++)
        {
            val = (char) bitData[i];
            outfile.write( reinterpret_cast<char *>(&val), sizeof(val) );
        }
        outfile.close();

        long outfileSize = bitData.size();
        bitData.clear();

        //fret_str = bCtrl.process("/tmp/videofile.mp4");
        //auto str_out = fret_str;
        //return str_out;
        return("Done");
    }
    else 
    {
        AWS_LOGSTREAM_ERROR(TAG, "Failed with error: " << outcome.GetError());
        return "Error in bb_video_process";
    }

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

