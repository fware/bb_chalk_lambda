/******
 *  OrgTrack.hpp
 *  Author:  WareShop Consulting LLC
 *
 *  Copyright 2016
 *
 */
#include "opencv2/dnn.hpp"
#include "opencv2/dnn/shape_utils.hpp"
#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/video/background_segm.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/objdetect/objdetect.hpp"
#include "opencv2/features2d/features2d.hpp"
#include "DebugHelpers.hpp"
//#include <opencv2/legacy/compat.hpp>
#include <unistd.h>
#include <stdio.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>

using namespace std;
using namespace cv;


#define SHOT_DEBUG


class BBController
{
public:
  BBController();
  ~BBController();

public:
	int findIndex_BSearch(const vector< int> &my_numbers, int key);
	double oneDDist(double p1, double p2);
	double euclideanDist(double x1, double y1, double x2, double y2);
	void getGray(const Mat& image, Mat& gray);
	std::string process(std::string file_name);

public:
	string videofileName;
	bool sizeFlag;
	Mat finalImg;
	int leftActiveBoundary;
	int rightActiveBoundary;
	int topActiveBoundary;
	int bottomActiveBoundary;
	int leftBBRegionLimit;
	int rightBBRegionLimit;
	int bottomBBRegionLimit;
	cv::Rect unionRect;
	bool isFirstPass;
	Mat img;

};