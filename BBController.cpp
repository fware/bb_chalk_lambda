#include "BBController.hpp"
#include "PlayerObs.hpp"

using namespace std;
using namespace cv;

BBController::BBController() : sizeFlag(false), frameCount(0), haveBackboard(false), thresh(85), semiCircleReady(false)
{
	greenColor = Scalar (0, 215, 0);
    bg_model = createBackgroundSubtractorMOG2(30, 16.0, false);
}

BBController::~BBController() {}


int BBController::initialize()
{
	bbsrc = imread("/tmp/bball-half-court-vga.jpg");
    if(!bbsrc.data )
    {
        cout <<  "Could not open or find the image" << std::endl ;
        return -1111;
    }
    if (std::remove( "/tmp/bball-half-court-vga.jpg" ) != 0)
    	return -1112;

	if( !body_cascade.load( "/tmp/haarcascade_fullbody.xml" ) )
	{
		cout << "--(!)Error loading body_cascade_name" << endl; 
		return -1113;
	}
    if (std::remove( "/tmp/haarcascade_fullbody.xml") != 0 )
    	return -1114;

	net = readNetFromDarknet("/tmp/made.cfg", "/tmp/made_8200.weights");
	if (net.empty())
	{
		cout << "dnn model is empty." << endl;
		return -1115;
	}
    if (std::remove( "/tmp/made.cfg" ) != 0 )
    	return -1116;
    if (std::remove( "/tmp/made_8200.weights" ) != 0 )
    	return -1117;

    ifstream classNamesFile("/tmp/made.names");
    if (classNamesFile.is_open())
    {
        string className = "";
        while (std::getline(classNamesFile, className))
            classNamesVec.push_back(className);
    }
    else
    {
    	return -1118;
    }
    if (std::remove( "/tmp/made.names" ) != 0 )
    	return -1119;

	return 0;
}

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

//std::string BBController::process(std::string file_name) 
int BBController::process(cv::Mat image, int frame_count) 
{
	Mat img = image.clone();
	int frameCount = frame_count;
	RNG rng(12345);
	string str;
//	VideoCapture cap(file_name);

    //if( !cap.isOpened() )
    //    return("can not open video file");

    vector<string> classNamesVec;
    classNamesVec.push_back("made");
    //ifstream classNamesFile("/tmp/made.names");
    //if (classNamesFile.is_open())
    //{
        //string className = "";
        //while (std::getline(classNamesFile, className))
        //   classNamesVec.push_back(className);
    //}
    //else
    //{
    //	return("Failed to open made.names.");
    //}

    //Mat firstFrame;
	//cap >> firstFrame;

	if (img.empty())
        return(-20);  //"Cannot retrieve first video capture frame.");

    if (frameCount <= 1)
    {
    	S = Size( img.cols, img.rows );

    	if (S.width > 320)
    	{
    		sizeFlag = true;
    		S = Size(240, 180);  //Size(640, 480);
    	}    	

		leftActiveBoundary 			= S.width/4;  
		rightActiveBoundary			= S.width*3/4;
		topActiveBoundary				= S.height/4;
		bottomActiveBoundary			= S.height*3/4;
		leftBBRegionLimit = (int) S.width * 3 / 8;
		rightBBRegionLimit = (int) S.width * 5 / 8;
		//topBBRegionLimit = (int) S.height*2/8;
		bottomBBRegionLimit = (int) leftActiveBoundary;

		isFirstPass = true;
		imageEmpty = false;

		resize(bbsrc, bbsrc, S);
    }

	Mat	finalImg(S.height, S.width+S.width, CV_8UC3);		

    //for(;;)
    //{
        //cap >> img;

		if (img.empty())
		{
			cout << __LINE__ << " Empty img mat" << endl;
			imageEmpty = true;
			return -1;
		}

   		frameCount++;

   		//if (frameCount == 10000)
   		//	break;

        if (sizeFlag)
        	resize(img, img, S);

      	flip(img, img, 0);

		///*************************Start of main code to detect BackBoard*************************
		stringstream ss;

		if (!haveBackboard)
		{
	    	ss << frameCount;

			getGray(img,grayImage);					//Converts to a gray image.  All we need is a gray image for cv computing.
			blur(grayImage, grayImage, Size(3,3));	//Blurs, i.e. smooths, an image using the normalized box filter.  Used to reduce noise.

			Canny(grayImage, grayImage, thresh, thresh*2, 3);
			findContours( grayImage, boardContours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE, Point(0, 0) );
			vector< vector<Point> > contours_poly( boardContours.size() );
			vector<Rect> boundRect( boardContours.size() );
			for ( size_t i = 0; i < boardContours.size(); i++ )
			{
				approxPolyDP(Mat(boardContours[i]),contours_poly[i],3,true);
				boundRect[i] = boundingRect(Mat(contours_poly[i]));

				double bb_w = (double) boundRect[i].size().width;
				double bb_h = (double) boundRect[i].size().height;
	    		double bb_ratio = (double) bb_w / bb_h;
	    		if ( (boundRect[i].x > leftBBRegionLimit)
					  && (boundRect[i].x < rightBBRegionLimit)
					  && (boundRect[i].x + boundRect[i].width < rightBBRegionLimit)
					  && (boundRect[i].y < bottomBBRegionLimit)
					  && (boundRect[i].area() > 50)
					  && (bb_ratio < 1.3)
					  && (bb_w > (bb_h * 0.74) ) )
				{
	    			if (isFirstPass)
        			{
        				unionRect = boundRect[i];
        				isFirstPass = false;
        			}
        			else if (frameCount < 100)
        			{
        				unionRect = unionRect | boundRect[i];
        			}

        			if (frameCount > 99) {
        				//rectangle(img, unionRect.tl(), unionRect.br(), Scalar(0,255,0), 2, 8, 0);
						backboardOffsetX = -unionRect.tl().x + img.size().width/2 - 13;
						backboardOffsetY = -unionRect.tl().y + 30;
						offsetBackboard = Rect(unionRect.tl().x+backboardOffsetX,
												unionRect.tl().y+backboardOffsetY,
												unionRect.size().width,
                                                unionRect.size().height);

						Point semiCircleCenterPt( (offsetBackboard.tl().x+offsetBackboard.width/2) , (offsetBackboard.tl().y + offsetBackboard.height/2) );
		                bbCenterPosit = semiCircleCenterPt;

        				Backboard = unionRect;
        				BackboardCenterX = (Backboard.tl().x+(Backboard.width/2));
        				BackboardCenterY = (Backboard.tl().y+(Backboard.height/2));
        				haveBackboard = true;
        			}
        			//else
        			//	rectangle(img, boundRect[i].tl(), boundRect[i].br(), Scalar(0,255,0), 2, 8, 0);
				}
			}
		}   //if (!haveBackboard)
		///*************************End of main code to detect BackBoard*************************

		/*if (frameCount == 100) 
		{
			str = std::to_string(frameCount) + ":BackboardCenterX=" + std::to_string(BackboardCenterX);
			break;
		}*/

		///*******Start of main code to detect Basketball*************************
		if (haveBackboard && ((frameCount % 40) == 0))
		{
	    	if (!semiCircleReady)
	    	{
	    		int radiusIdx = 0;
	    		for (int radius = 40; radius < 280; radius+= 20)   //Radius for distFromBB
	    		{
	    			radiusArray.push_back(radius);

	    			int temp1, temp2, temp3;
	    			int yval;
	    			for (int j = (bbCenterPosit.x - radius); j <= bbCenterPosit.x + radius; j++)   //Using Pythagorean's theorem to find positions on the each court arc.
	    			{
	    				temp1 = radius * radius;
	    				temp2 = (j - bbCenterPosit.x) * (j - bbCenterPosit.x);
	    				temp3 = temp1 - temp2;
	    				yval = sqrt(temp3);
	    				yval += bbCenterPosit.y;
	    				Point ptTemp = Point(j, yval);
	    				courtArc[radiusIdx][j] = ptTemp;
	    			}

	    			radiusIdx++;
	    		}
	    		semiCircleReady = true;
	    	}


	    	cout << "haveBackboard=" << haveBackboard << "   frameCount=" << frameCount << endl;

			getGray(img,grayImage);											//Converts to a gray image.  All we need is a gray image for cv computing.
			blur(grayImage, grayImage, Size(3,3));							//Blurs, i.e. smooths, an image using the normalized box filter.  Used to reduce noise.
			bg_model->apply(grayImage, fgmask);				//Computes a foreground mask for the input video frame.
			Canny(fgmask, fgmask, thresh, thresh*2, 3);			//Finds edges in an image.  Going to use it to help identify and track the basketball.
																//Also used in the processing pipeline to identify the person(i.e. human body) shooting the ball.

			vector<vector<Point> > bballContours;
			vector<Vec4i> hierarchy;
			findContours(fgmask,bballContours,hierarchy,RETR_TREE,CHAIN_APPROX_SIMPLE, Point(0, 0) );	//Finds contours in foreground mask image.

			Mat imgBball = Mat::zeros(fgmask.size(),CV_8UC1);
			for (size_t i = 0; i < bballContours.size(); i++ )
			{
				Scalar color = Scalar( rng.uniform(0, 255), rng.uniform(0, 255), rng.uniform(0, 255) );
				drawContours(imgBball,bballContours,i,color,2,8,hierarchy,0,Point());	//Draws contours onto output image, i.e. imgBball.
																						//The goal here is the find and track the basketball inside of imgBball image frames
			}

			//------------Track the basketball!!!!---------------
			vector<Vec3f> basketballTracker;
			float canny1 = 100;
			float canny2 = 14; //16;
			double minDist = imgBball.rows/4;   //8; //4;
			HoughCircles(imgBball, basketballTracker, HOUGH_GRADIENT, 1, minDist, canny1, canny2, 1, 9 );	//Finds circles in input image. (imgBball)
																												//Writes output to output array (basketballTracker)

			if (basketballTracker.size() > 0)
			{
				for (size_t i = 0; i < basketballTracker.size(); i++)
				{
					Point bballCenter(cvRound(basketballTracker[i][0]), cvRound(basketballTracker[i][1]));
					double bballRadius = (double) cvRound(basketballTracker[i][2]);
					double bballDiameter = (double)(2*bballRadius);

					int bballXtl = (int)(basketballTracker[i][0]-bballRadius);
					int bballYtl = (int)(basketballTracker[i][1]-bballRadius);
					ballRect = Rect(bballXtl, bballYtl, bballDiameter, bballDiameter);

					if ( (ballRect.x > leftActiveBoundary)
									&& (ballRect.x < rightActiveBoundary)
									&& (ballRect.y > topActiveBoundary)
									&& (ballRect.y < bottomActiveBoundary) )
					{
						//The basketball on video frames.
						rectangle(img, ballRect.tl(), ballRect.br(), Scalar(60,180,255), 2, 8, 0 );
						Rect objIntersect = Backboard & ballRect;

						//---Start of the process of identifying a shot at the basket!!!------------
						if (objIntersect.area() > 0)
						{

							//Predict is a made shot
							Mat basketRoI = img(Backboard).clone();
				            //resize(basketRoI, basketRoI, Size(416,416));

				            //! [Prepare blob]
				            Mat inputBlob = blobFromImage(basketRoI, 1 / 255.F, Size(416, 416), Scalar(), true, false); //Convert Mat to batch of images
				            //! [Prepare blob]

				            //! [Set input blob]
				            net.setInput(inputBlob, "data");                   //set the network input
				            //! [Set input blob]

				            //! [Make forward pass]
				            Mat detectionMat = net.forward("detection_out");   //compute output

				            for (int i = 0; i < detectionMat.rows; i++)
				            {
				                const int probability_index = 5;
				                const int probability_size = detectionMat.cols - probability_index;
				                float *prob_array_ptr = &detectionMat.at<float>(i, probability_index);

				                size_t objectClass = max_element(prob_array_ptr, prob_array_ptr + probability_size) - prob_array_ptr;
				                float confidence = detectionMat.at<float>(i, (int)objectClass + probability_index);

				                if (confidence > 0.24)
				                {
				                    float x = detectionMat.at<float>(i, 0);
				                    float y = detectionMat.at<float>(i, 1);
				                    float width = detectionMat.at<float>(i, 2);
				                    float height = detectionMat.at<float>(i, 3);
				                    int xLeftBottom = static_cast<int>((x - width / 2) * basketRoI.cols);
				                    int yLeftBottom = static_cast<int>((y - height / 2) * basketRoI.rows);
				                    int xRightTop = static_cast<int>((x + width / 2) * basketRoI.cols);
				                    int yRightTop = static_cast<int>((y + height / 2) * basketRoI.rows);

				                    Rect object(xLeftBottom, yLeftBottom,
				                                xRightTop - xLeftBottom,
				                                yRightTop - yLeftBottom);

				                    rectangle(basketRoI, object, Scalar(0, 255, 0));

				                    if (objectClass < classNamesVec.size())
				                    {
				                        ss.str("");
				                        ss << confidence;
				                        String conf(ss.str());
				                        String label = String(classNamesVec[objectClass]) + ": " + conf;
				                        int baseLine = 0;
				                        Size labelSize = getTextSize(label, FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);
				                        rectangle(basketRoI, Rect(Point(xLeftBottom, yLeftBottom ),
				                                              Size(labelSize.width, labelSize.height + baseLine)),
				                                  Scalar(255, 255, 255), CV_FILLED);
				                        putText(basketRoI, label, Point(xLeftBottom, yLeftBottom+labelSize.height),
				                                FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0,0,0));
				                    }
				                    else
				                    {
				                        cout << "Class: " << objectClass << endl;
				                        cout << "Confidence: " << confidence << endl;
				                        cout << " " << xLeftBottom
				                             << " " << yLeftBottom
				                             << " " << xRightTop
				                             << " " << yRightTop << endl;
				                    }
				                }
				            }

				            //imshow("YOLO: Detections", basketRoI);

				            //********End of Shot Prediction **********


							//---Start of using player position on halfcourt image to draw shot location-----
							if (frameCount > 50)
							{
								circle(bbsrc, courtArc[newPlayerWindow.radiusIdx][newPlayerWindow.placement], 1, Scalar(0, 165, 255), 3);
							}
						}
						//---Start of using player position on halfcourt image to draw shot location-----
						//---End of the process of identifying a shot at the basket!!!------------
					}
				}
				///*******End of code to detect & select Basketball*************************
			}

			//-- detect body
			body_cascade.detectMultiScale(grayImage, bodys, 1.1, 2, 18|9|CASCADE_SCALE_IMAGE, Size(3,7));  //Detects object of different sizes in the input image.
																											 //This detector is looking for human bodies with min Size(3, 7) in a VGA image.
			ss << frameCount;

			for( int j = 0; j < (int) bodys.size(); j++ )
			{
				//-----------Identifying player height and position!!--------------
				Point bodyCenter( bodys[j].x + bodys[j].width*0.5, bodys[j].y + bodys[j].height*0.5 );

				//--- Start of adjusting player position on image of half court!!!-----
				newPlayerWindow.frameCount = frameCount;
				newPlayerWindow.activeValue = 1;
				newPlayerWindow.position = bodyCenter;

				double distFromBB = euclideanDist((double) BackboardCenterX,(double) BackboardCenterY,(double) bodyCenter.x, (double) bodyCenter.y);
				double xDistFromBB = oneDDist(BackboardCenterX, bodyCenter.x);
				double yDistFromBB = oneDDist(BackboardCenterY, bodyCenter.y);

				if (distFromBB > 135)
				{
					newPlayerWindow.radiusIdx = radiusArray.size() * 0.99;
					distFromBB += 120;

					int tempPlacement = (bbCenterPosit.x + radiusArray[newPlayerWindow.radiusIdx])
									- (bbCenterPosit.x - radiusArray[newPlayerWindow.radiusIdx]);

					if (bodyCenter.x > BackboardCenterX)
						tempPlacement -= 1;
					else
						tempPlacement = 0;

					tempPlacement += (bbCenterPosit.x - radiusArray[newPlayerWindow.radiusIdx]);

					newPlayerWindow.placement = tempPlacement;
				}
				else if (distFromBB < 30)
				{
					int tempPlacement;
					if (bodyCenter.x < BackboardCenterX)
						tempPlacement = 0;
					else
						tempPlacement = (bbCenterPosit.x + radiusArray[newPlayerWindow.radiusIdx])
									- (bbCenterPosit.x - radiusArray[newPlayerWindow.radiusIdx]) - 1;

					newPlayerWindow.placement = tempPlacement;
					newPlayerWindow.radiusIdx = radiusArray.size() * 0.01;
				}
				else
				{
					if (bodys[j].height < 170)    //NOTE:  If not true, then we have inaccurate calculation of body height from detectMultiscale method.  Do not estimate a player position for it.
					{
						newPlayerWindow.radiusIdx = findIndex_BSearch(radiusArray, distFromBB);
						newPlayerWindow.radiusIdx += 5;

						if ((xDistFromBB < 51) && (yDistFromBB < 70))
							newPlayerWindow.radiusIdx = 0;

						double percentPlacement = (double) (bodyCenter.x - leftActiveBoundary) / (rightActiveBoundary - leftActiveBoundary);
						int leftRingBound		= bbCenterPosit.x - radiusArray[newPlayerWindow.radiusIdx];
						int rightRingBound		= bbCenterPosit.x + radiusArray[newPlayerWindow.radiusIdx];
						int chartPlacementTemp	= (rightRingBound - leftRingBound) * percentPlacement;
						int chartPlacement		= leftRingBound + chartPlacementTemp;

						newPlayerWindow.placement = chartPlacement;
					}
				}
				//--- End of adjusting player position on image of half court!!!-----
			}
			rectangle(img, Backboard.tl(), Backboard.br(), Scalar(0,0,255), 2, 8, 0);
		}  //if (haveBackboard)

		//Create string of frame counter to display on video window.
		str = "frame" + ss.str();		
		putText(img, str, Point(100, 100), FONT_HERSHEY_PLAIN, 2 , greenColor, 2);
		Mat left(finalImg, Rect(0, 0, img.cols, img.rows));
		img.copyTo(left);
		Mat right(finalImg, Rect(bbsrc.cols, 0, bbsrc.cols, bbsrc.rows));
		bbsrc.copyTo(right);		   		
		cout << __LINE__ << " Bug" << endl;

	//} //for (;;)
	cout << __LINE__ << " Bug" << endl;

    if (imageEmpty)
    	cout << "broke loop before image is empty" << endl;
    else
    	cout << "Loop has ended frameCount:" << frameCount << endl;

	img.release();
	left.release();
	right.release();
	finalImg.release();

    str = "We broke loop at frameCount " + std::to_string(frameCount);
    return 0;
	//return(str);
    //return("BBController process done.");
}
