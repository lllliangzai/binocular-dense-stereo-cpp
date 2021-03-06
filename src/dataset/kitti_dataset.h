/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                           License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2014, Itseez Inc, all rights reserved.
// Third party copyrights are property of their respective owners.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of the copyright holders may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the Itseez Inc or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/

#ifndef OPENCV_DATASETS_SLAM_KITTI_HPP
#define OPENCV_DATASETS_SLAM_KITTI_HPP

#include <string>
#include <vector>

#include "dataset.hpp"
#include <iostream>

#include <opencv2/imgproc/imgproc.hpp>
#include "opencv2/highgui/highgui.hpp"

#include <opencv2/core/core.hpp>
#include "../logger/log.h"

namespace cv
{
    namespace datasets
    {

//! @addtogroup datasets_slam
//! @{

        struct pose
        {
            double elem[12];
        };

        struct image_datasetObj : public Object {

            cv::Mat transformMatrix;
            Matx33d k;
            Matx33d r;
            double tl[3];
        };

        struct SLAM_kittiObj : public Object
        {
            std::string name;
            std::vector<std::string> images[2];
            std::vector<double> times, p[2];
            std::vector<pose> posesArray;
            std::vector<image_datasetObj> calibArray;
        };


        class CV_EXPORTS SLAM_kitti : public Dataset {

        public:
            virtual void load(const std::string &path) = 0;

            static Ptr<SLAM_kitti> create();

            virtual cv::Mat loadImage(const int img_num) = 0;
            virtual FramePair load_stereo_images(const int img_num) = 0;
        };

//! @}

    }
}

#endif