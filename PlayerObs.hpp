/*
 * PlayerObs.hpp
 *
 *  Created on: May 28, 2018
 *      Author: WareShop LLC
 */
#ifndef PLAYEROBS_HPP_
#define PLAYEROBS_HPP_

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
#include <vector>

using namespace std;
using namespace cv;

class PlayerObs {
	public:
		PlayerObs();
		~PlayerObs();
	public:
		int		activeValue;
		int		radiusIdx;
		int 	placement;
		Point   position; 
		int 	frameCount;
};

#endif /* PLAYEROBS_HPP_ */