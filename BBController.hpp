/*
 * BBController.hpp
 *
 *  Created on: May 28, 2018
 *      Author: WareShop LLC
 */
#ifndef BBCONTROLLER_HPP_
#define BBCONTROLLER_HPP_

#include "opencv2/dnn.hpp"
#include "opencv2/dnn/shape_utils.hpp"
#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/video/background_segm.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/objdetect/objdetect.hpp"
#include "opencv2/features2d/features2d.hpp"
//#include <opencv2/legacy/compat.hpp>
#include <unistd.h>
#include <stdio.h>
#include <fstream>
#include <iostream>
#include <cstdio>
#include <vector>
#include <algorithm>

#include "DebugHelpers.hpp"
#include "PlayerObs.hpp"

using namespace std;
using namespace cv;
using namespace cv::dnn;


#define SHOT_DEBUG


class BBController
{
public:
  BBController();
  ~BBController();

public:
	int initialize();
	int findIndex_BSearch(const vector< int> &my_numbers, int key);
	double oneDDist(double p1, double p2);
	double euclideanDist(double x1, double y1, double x2, double y2);
	void getGray(const Mat& image, Mat& gray);
	//std::string process(std::string file_name);
	int process(cv::Mat image, int frame_count);

public:
	bool sizeFlag;
	bool isFirstPass;
	bool haveBackboard;
	bool semiCircleReady;
	bool imageEmpty;
	int frameCount;
	int thresh;
	int leftActiveBoundary;
	int rightActiveBoundary;
	int topActiveBoundary;
	int bottomActiveBoundary;
	int leftBBRegionLimit;
	int rightBBRegionLimit;
	int bottomBBRegionLimit;
	int BackboardCenterX;
	int BackboardCenterY;
	int backboardOffsetX;
	int backboardOffsetY;
	int newPlayerWindowSize;
	Size S;
	Mat grayImage;				//Gray image of source image.
	Mat fgmask;					//Foreground mask image.
	Mat bbsrc;
	Mat threshold_output;
	Rect Backboard;
	Rect offsetBackboard;
	Rect ballRect;	//Represents the box around the trackable basketball
	Rect unionRect;
	Point bodyPosit;
	Point bbCenterPosit;
	Point courtArc[50][1200];
	PlayerObs newPlayerWindow;
	vector <int> radiusArray;
	vector< vector<Point> > boardContours;
	vector<Vec4i> hierarchy;
    vector<Rect> bodys;
    vector<string> classNamesVec;
	Scalar greenColor;
	Ptr<BackgroundSubtractor> bg_model;
	CascadeClassifier body_cascade;
	dnn::Net net;
};

#endif /* BBCONTROLLER_HPP__ */