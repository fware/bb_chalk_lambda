#include "BBController.hpp"

using namespace std;
using namespace cv;

BBController::BBController() : sizeFlag(false)
{

}

BBController::~BBController() {}


int BBController::findIndex_BSearch(const vector< int> &numbersArray, int key) {

	int iteration = 0;
	int left = 0;
	int right = numbersArray.size()-1;
	int mid;

	while (left <= right) {
		iteration++;
		mid = (int) ((left + right) / 2);
		if (key <= numbersArray[mid]) 
		{
			right = mid - 1;
		}
		else if (key > numbersArray[mid])
		{
			left = mid + 1;
		}
	}
	return (mid);
}

double BBController::oneDDist(double p1, double p2) {
	double dist;
	
	double p = p1 - p2;
	dist = pow(p, 2);
	dist = sqrt(dist);
	
	return dist;
}


double BBController::euclideanDist(double x1, double y1, double x2, double y2)
{
	double x = x1 - x2; //calculating number to square in next step
	double y = y1 - y2;
	double dist;

	dist = pow(x, 2) + pow(y, 2);       //calculating Euclidean distance
	dist = sqrt(dist);                  

	return dist;
}


void BBController::getGray(const Mat& image, Mat& gray)
{
    if (image.channels()  == 3)
        cv::cvtColor(image, gray, COLOR_BGR2GRAY);
    else if (image.channels() == 4)
        cv::cvtColor(image, gray, COLOR_BGRA2GRAY);
    else if (image.channels() == 1)
        gray = image;
}

std::string BBController::process(std::string file_name) 
{
	VideoCapture cap(file_name);

    if( !cap.isOpened() )
        return("can not open video file");

    Mat firstFrame;
	cap >> firstFrame;
	if (firstFrame.empty())
        return("Cannot retrieve first video capture frame.");

    Size S = Size((int) cap.get(CAP_PROP_FRAME_WIDTH),    // Acquire input size
                  (int) cap.get(CAP_PROP_FRAME_HEIGHT));

	if (S.width > 640)
	{
		sizeFlag = true;
		S = Size(640, 480);
		resize(firstFrame, firstFrame, S);
	}

	Mat finalImg(S.height, S.width+S.width, CV_8UC3);
	leftActiveBoundary 			= firstFrame.cols/4;  
	rightActiveBoundary			= firstFrame.cols*3/4;
	topActiveBoundary				= firstFrame.rows/4;
	bottomActiveBoundary			= firstFrame.rows*3/4;
	leftBBRegionLimit = (int) firstFrame.cols * 3 / 8;
	rightBBRegionLimit = (int) firstFrame.cols * 5 / 8;
	//topBBRegionLimit = (int) firstFrame.rows*2/8;
	bottomBBRegionLimit = (int) leftActiveBoundary;
	firstFrame.release();

	isFirstPass = true;

    for(;;)
    {
        cap >> img;

        if (sizeFlag)
        	resize(img, img, S);

      	flip(img, img, 0);
	}


    return("BBController process done.");
}
