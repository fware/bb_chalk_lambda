#include <iostream>
#include <string>

#include <opencv2/core/core.hpp>


class BBController
{
public:
  BBController();
  ~BBController();

  int process_frame(cv::Mat mat_frame);

};