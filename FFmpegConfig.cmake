# - Try to find the FFMPEG package -
# If found the following will be defined
#
#  FFMPEG_FOUND - FFMPEG package found on the system
#  FFMPEG_INCLUDE_DIR - Directory of the FFMPEG package include files
#  FFMPEG_LIBRARY - Where libkbfxcommon.so resides
#

FIND_PATH(FFMPEG_INCLUDE_DIR libavformat/avformat.h libavcodec/avcodec.h libavutil/avutil.h libswscale/swscale.h
  PATHS
  /usr/include/x86_64-linux-gnu
  #/usr/include
  #~/install
)

#IF ( NOT FFMPEG_INCLUDE_DIR )
	#  FIND_PATH(FFMPEG_avutil_INCLUDE_DIR libavutil/avutil.h
	  #    PATHS
    #    ~/DevTools/ffmpeg-4.1.3/
    #    /usr/local/include
    #    /usr/include
    #    ~/install
    #    )    
#ENDIF ( NOT FFMPEG_avutil_INCLUDE_DIR )

# Build the include path with duplicates removed.
#IF (FFMPEG_INCLUDE_DIRS)
#    LIST(REMOVE_DUPLICATES FFMPEG_INCLUDE_DIRS)
#ENDIF (FFMPEG_INCLUDE_DIRS)

GET_FILENAME_COMPONENT(FFMPEG_INCLUDE_DIR ${FFMPEG_INCLUDE_DIR} ABSOLUTE)

FIND_LIBRARY(FFMPEG_avformat_LIBRARY NAMES libavformat.so
  PATHS
  /usr/lib
  #/usr/local/lib
  #~/DevTools/ffmpeg-4.1.3/libavformat
)


FIND_LIBRARY(FFMPEG_avcodec_LIBRARY NAMES libavcodec.so
  PATHS
  /usr/lib
  #/usr/local/lib
  #~/DevTools/ffmpeg-4.1.3/libavcodec
)

FIND_LIBRARY(FFMPEG_avutil_LIBRARY NAMES libavutil.so
  PATHS
  /usr/lib
  #/usr/local/lib
  #~/DevTools/ffmpeg-4.1.3/libavcodec
)

FIND_LIBRARY(FFMPEG_swsscale_LIBRARY NAMES libswscale.so
  PATHS
  /usr/lib
  #/usr/local/lib
  #~/DevTools/ffmpeg-4.1.3/libswscale
)

SET(FFMPEG_LIBRARIES)
IF(FFMPEG_INCLUDE_DIR)
  IF(FFMPEG_avutil_LIBRARY)
    SET(FFMPEG_FOUND TRUE)
    SET(FFMPEG_INCLUDE_DIRS ${FFMPEG_INCLUDE_DIR})
    SET(FFMPEG_LIST_LIBRARIES 
	    ${FFMPEG_avformat_LIBRARY}
	    ${FFMPEG_avcodec_LIBRARY}
	    ${FFMPEG_avutil_LIBRARY}
	    ${FFMPEG_swsscale_LIBRARY}
	    )

    SET(FFMPEG_LIBRARIES ${FFMPEG_LIST_LIBRARIES})

  ENDIF(FFMPEG_avutil_LIBRARY)
ENDIF(FFMPEG_INCLUDE_DIR)

MARK_AS_ADVANCED(
  FFMPEG_INCLUDE_DIR
  FFMPEG_avformat_LIBRARY
  FFMPEG_avcodec_LIBRARY
  FFMPEG_avutil_LIBRARY
  FFMPEG_swsscale_LIBRARY
)



#IF(FFMPEG_FOUND)
#  IF(NOT FFMPEG_FIND_QUIETLY)
#    MESSAGE(STATUS "Found FFMPEG package: ${FFMPEG_LIBRARY}")
#  ENDIF(NOT FFMPEG_FIND_QUIETLY)
#ELSE(FFMPEG_FOUND)
#  IF(FFMPEG_FIND_REQUIRED)
#    MESSAGE(FATAL_ERROR "Could not find FFMPEG package! Please download and install FFMPEG from #http://ffmpeg.mplayerhq.hu")
#  ENDIF(FFMPEG_FIND_REQUIRED)
#ENDIF(FFMPEG_FOUND)

