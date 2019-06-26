cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 11)
project(bb_chalk LANGUAGES CXX)

find_package(aws-lambda-runtime)
find_package(AWSSDK COMPONENTS dynamodb s3)
#find_package(OpenCV COMPONENTS opencv_core opencv_imgproc opencv_highgui)
find_package(OpenCV REQUIRED)
find_package(FFmpeg REQUIRED)

message(STATUS "OpenCV library status:")
message(STATUS "    version: ${OpenCV_VERSION}")
message(STATUS "    libraries: ${OpenCV_LIBS}")
message(STATUS "    include path: ${OpenCV_INCLUDE_DIRS}")
message(STATUS "    include path: ${AVCODEC_INCLUDE_DIR}")

message(STATUS "FFMPEG library status:")
message(STATUS "    libraries: ${FFMPEG_LIST_LIBRARIES}")
message(STATUS "    include path: ${FFMPEG_INCLUDE_DIR}")

add_executable(${PROJECT_NAME} "main.cpp" "BBController.cpp" "PlayerObs.cpp")

#if(CMAKE_VERSION VERSION_LESS "2.8.11")
  # Add OpenCV headers location to your include paths
  #include_directories(${OpenCV_INCLUDE_DIRS})

  # Add FFMPEG headers location to your include paths
  #include_directories(${FFMPEG_INCLUDE_DIR})

#endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
                      AWS::aws-lambda-runtime
                       ${AWSSDK_LINK_LIBRARIES}
		       ${OpenCV_LIBS}
		       ${FFMPEG_LIBRARIES})

target_compile_features(${PROJECT_NAME} PRIVATE "cxx_std_11")

aws_lambda_package_target(${PROJECT_NAME})
#aws_lambda_package_target(${PROJECT_NAME} NO_LIBC)
