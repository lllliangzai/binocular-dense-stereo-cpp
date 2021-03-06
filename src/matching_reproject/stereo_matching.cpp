/*
    *  stereo_match.cpp
    *  calibration
    *
    *  Created by Victor  Eruhimov on 1/18/10.
    *  Copyright 2010 Argus Corp. All rights reserved.
    *
    */
#include <cv.h>
#include <highgui.h>
#include <iostream>
#include <string>
#include <pcl/common/common_headers.h>
#include <pcl/io/pcd_io.h>
#include <pcl/visualization/pcl_visualizer.h>
#include <boost/thread/thread.hpp>
#include "opencv2/photo/photo.hpp"


#include "opencv2/calib3d/calib3d.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/contrib/contrib.hpp"
#include <libconfig.h++>



#include <stdio.h>
#include <libconfig.h>
#include <pcl/common/transforms.h>
#include <pcl/io/ply_io.h>

// local includes
#include "stereo_matching.hpp"
#include "../utils/util.hpp"
#include "../dataset/msm_middlebury.hpp"
#include "../dataset/tsukuba_dataset.h"
#include "../dataset/dataset.hpp"


// logger
#include "../logger/log.h"
#include "../stereo_viewer/viewer.hpp"


using namespace cv;

namespace binocular_dense_stereo {

    /*
        input   :
                Mat& img1, Mat& img2, Mat& M1, Mat& D1, Mat& M2, Mat& D2, Mat& R, Mat& T, scale
        output  :
                Mat& R1, Mat& R2, Mat& P1, Mat& P2, Mat& Q, Rect &roi1, Rect &roi2,
     */
    cv::datasets::FramePair rectifyImages(Mat& img1, Mat& img2, Mat& M1, Mat& D1, Mat& M2, Mat& D2, Mat& R, Mat& T, Mat& R1, Mat& R2, Mat& P1, Mat& P2, Mat& Q, Rect &roi1, Rect &roi2, float scale){

        Size img_size = img1.size();

        M1 *= scale;
        M2 *= scale;

        FILE_LOG(logDEBUG) << binocular_dense_stereo::infoMatrix(T);

        // dopo Q: 0 o CV_CALIB_ZERO_DISPARITY
        int flags = 0;
        stereoRectify( M1, D1, M2, D2, img_size, R, T, R1, R2, P1, P2, Q, flags, -1, img_size, &roi1, &roi2 );

        Mat map11, map12, map21, map22;
        initUndistortRectifyMap(M1, D1, R1, P1, img_size, CV_16SC2, map11, map12);
        initUndistortRectifyMap(M2, D2, R2, P2, img_size, CV_16SC2, map21, map22);

        Mat img1r, img2r;
        remap(img1, img1r, map11, map12, INTER_CUBIC);
        remap(img2, img2r, map21, map22, INTER_CUBIC); // prima linear

        cv::datasets::FramePair pair;

        pair.frame_left = img1r;
        pair.frame_right = img2r;

        return pair;

    }

    void
    depthFromDisparity (cv::Mat& disparity_image, float focal, float baseline_, float min_disparity, cv::Mat& depth_image, bool gt)
    {

        FILE_LOG(logINFO) << "depth a 100 100: " << depth_image.at<float>(100,100);

        double min, max;
        cv::minMaxIdx(disparity_image, &min, &max);
        FILE_LOG(logINFO) << "disparity min : " << min << " max: "<< max;
        FILE_LOG(logINFO) << "disparity - " << binocular_dense_stereo::infoMatrix(disparity_image);
        FILE_LOG(logINFO) << "baseline - " << baseline_;
        FILE_LOG(logINFO) << "focal - " << focal;

        int miky = 0;
        depth_image = cv::Mat::zeros(disparity_image.rows, disparity_image.cols, CV_32F);
        for (int i = 0; i < disparity_image.rows; i++)
        {
            for (int j = 0; j < disparity_image.cols; j++)
            {

                float disparity_value = (float) disparity_image.at<short>(i,j);
                if (!gt){
//                    FILE_LOG(logINFO) << " depth not from gt. ";
                    disparity_value = disparity_value/16;
                }
                if (disparity_value > min_disparity)
                {
                    depth_image.at<float>(i,j) = baseline_ * focal / disparity_value;
//                    if (miky < 20) {
//                        FILE_LOG(logINFO) << "disparity_value - " << disparity_value;
//                        FILE_LOG(logINFO) << "depth_image.at<float>("<<i<<","<<j<<") - " << depth_image.at<float>(i,j);
//                        miky++;
//
//                    }
                }
            }
        }

        cv::minMaxIdx(depth_image, &min, &max);
        FILE_LOG(logINFO) << "depth min : " << min << " max: "<< max;
        FILE_LOG(logINFO) << "depth - " << binocular_dense_stereo::infoMatrix(depth_image);
    }

    void
    pointcloudFromDepthImage (cv::Mat& depth_image, cv::Mat& img_left, cv::Mat& depth_intrinsics, PointCloudRGB::Ptr& output_cloud)
    {
        FILE_LOG(logINFO) << " fx " << depth_intrinsics.at<double>(0,0);
        FILE_LOG(logINFO) << " fy " << depth_intrinsics.at<double>(1,1);

        // For point clouds XYZ
        double depth_focal_inverted_x = 1/depth_intrinsics.at<double>(0,0);  // 1/fx
        double depth_focal_inverted_y = 1/depth_intrinsics.at<double>(1,1);  // 1/fy

        PointTRGB new_point;

        output_cloud->points.resize(depth_image.cols*depth_image.rows, new_point);
        output_cloud->width = depth_image.cols;
        output_cloud->height = depth_image.rows;
        output_cloud->is_dense = false;

        int miky = 0;

        for (int i=0;i<depth_image.rows;i++)
        {
            for (int j=0;j<depth_image.cols;j++)
            {
                float depth_value = depth_image.at<float>(i,j);

                if (depth_value > 0)
                {
//                    FILE_LOG(logINFO) << " in teoria non mi vedi ";

                    // Find 3D position with respect to depth frame:
                    new_point.z = depth_value;
                    new_point.x =  static_cast<float>(j - depth_intrinsics.at<double>(0,2)) * new_point.z * depth_focal_inverted_x;
                    new_point.y =  static_cast<float>(i - depth_intrinsics.at<double>(1,2)) * new_point.z * depth_focal_inverted_y;

//                    if (miky < 20){
//                        FILE_LOG(logINFO) << "depth_image.at<float>("<<i<<","<<j<<") - " << depth_image.at<float>(i,j);
//                        FILE_LOG(logINFO) << "new_point.x <float>("<<i<<","<<j<<") - " << new_point.y ;
//                        FILE_LOG(logINFO) << "new_point.y <float>("<<i<<","<<j<<") - " << new_point.y ;
//
//                        miky++;
//                    }

                    cv::Vec3b intensity = img_left.at<cv::Vec3b>(i, j); //BGR
                    uint32_t rgb = (static_cast<uint32_t>(intensity[2]) << 16 | static_cast<uint32_t>(intensity[1]) << 8 | static_cast<uint32_t>(intensity[0]));
                    new_point.rgb = *reinterpret_cast<float *>(&rgb);

                    output_cloud->at(j,i) = new_point;
                }
//                else
//                {
//                    new_point.z = std::numeric_limits<float>::quiet_NaN();
//                    new_point.x = std::numeric_limits<float>::quiet_NaN();
//                    new_point.y = std::numeric_limits<float>::quiet_NaN();
//                    output_cloud->at(j,i) = new_point;
//                }
            }
        }
    }

    void computeDisparityTsukuba(const int img_frame, Mat& img_left, Mat& img_right,Mat& disp){


        std::string tipo = "SGBM";

        FILE_LOG(logDEBUG) << "USING SGBM DISPARITY - ";

        StereoSGBM sgbm;

//        sgbm.preFilterCap = 15;
//        sgbm.SADWindowSize = 5;
//        sgbm.P1 = 50;
//        sgbm.P2 = 800;
//        sgbm.minDisparity = 0;
//        sgbm.numberOfDisparities = 256;
//        sgbm.uniquenessRatio = 0;
//        sgbm.speckleWindowSize = 100;
//        sgbm.speckleRange = 32;
//        sgbm.disp12MaxDiff = 1;
//        sgbm.fullDP = 1;

        sgbm.numberOfDisparities = 128;
        sgbm.preFilterCap = 31;
        sgbm.SADWindowSize = 9;
        sgbm.P1 = 8*1*sgbm.SADWindowSize*sgbm.SADWindowSize;
        sgbm.P2 = 32*1*sgbm.SADWindowSize*sgbm.SADWindowSize;
        sgbm.minDisparity = 0;
        sgbm.uniquenessRatio = 10;
        sgbm.speckleWindowSize = 100;
        sgbm.speckleRange = 32;
        sgbm.disp12MaxDiff = 1;
        sgbm.fullDP = 0;

        Mat disp16;
        sgbm(img_left, img_right, disp);

//        normalize(disp16, disp, 0, 255, CV_MINMAX, CV_8U);

//        disp = disp / 16.0;


        imwrite("disp_"+std::to_string(img_frame)+".png", disp);
    }

    void computeDisparityMiddlebury(const int img_frame, Mat& img_left, Mat& img_right,Mat& disp){

        std::vector<binocular_dense_stereo::middleburyPair> association_pairs = binocular_dense_stereo::ConfigLoader::get_instance().loadMiddleburyAssociations();

        // recover orginal image number
        int img1_num = association_pairs[img_frame].left_image;
        int img2_num = association_pairs[img_frame].left_image;

        Mat g1, g2;

        ///da provare
        cvtColor(img_left, g1, CV_BGR2GRAY);
        cvtColor(img_right, g2, CV_BGR2GRAY);

        // ROTATE IMAGES
        if (img1_num < 32)
            binocular_dense_stereo::rotate_clockwise(g1, g1, false);
        else
            binocular_dense_stereo::rotate_clockwise(g1, g1, true);

        if (img2_num < 32)
            binocular_dense_stereo::rotate_clockwise(g2, g2, false);
        else
            binocular_dense_stereo::rotate_clockwise(g2, g2, true);



        std::string tipo = "SBM";

        FILE_LOG(logDEBUG) << "USING SBM DISPARITY - ";

        StereoBM sbm;

        sbm.state->SADWindowSize = 7;
        sbm.state->numberOfDisparities = 64;
        sbm.state->preFilterSize = 15;
        sbm.state->preFilterCap = 63;
        sbm.state->minDisparity = -50;
        sbm.state->textureThreshold = 1;
        sbm.state->uniquenessRatio = 1;
        sbm.state->speckleWindowSize = 1;
        sbm.state->speckleRange = 27;
//        sbm.state->disp12MaxDiff = 0;


//        sbm.state->SADWindowSize = 5;
//        sbm.state->numberOfDisparities = 192;
//        sbm.state->preFilterSize = 5;
//        sbm.state->preFilterCap = 51;
//        sbm.state->minDisparity = -5;
//        sbm.state->textureThreshold = 182;
//        sbm.state->uniquenessRatio = 0;
//        sbm.state->speckleWindowSize = 0;
//        sbm.state->speckleRange = 0;
//        sbm.state->disp12MaxDiff = 0;

//        sbm.state->SADWindowSize = 5;
//        sbm.state->numberOfDisparities = 160;
//        sbm.state->preFilterSize = 5;
//        sbm.state->preFilterCap = 11;
//        sbm.state->minDisparity = 6;
//        sbm.state->textureThreshold = 173;
//        sbm.state->uniquenessRatio = 0;
//        sbm.state->speckleWindowSize = 0;
//        sbm.state->speckleRange = 0;
//        sbm.state->disp12MaxDiff = 1;

//        sbm.state->SADWindowSize = 5;
//        sbm.state->numberOfDisparities = 192;
//        sbm.state->preFilterSize = 5;
//        sbm.state->preFilterCap = 51;
//        sbm.state->minDisparity = 25;
//        sbm.state->textureThreshold = 223;
//        sbm.state->uniquenessRatio = 0;
//        sbm.state->speckleWindowSize = 0;
//        sbm.state->speckleRange = 0;
//        sbm.state->disp12MaxDiff = 0;

        sbm(g1, g2, disp, CV_16S);

        // RESTORE ROTATION
        if (img1_num < 32)
            binocular_dense_stereo::rotate_clockwise(disp, disp, true);
        else
            binocular_dense_stereo::rotate_clockwise(disp, disp, false);

        imwrite("disp_"+std::to_string(img_frame)+".png", disp);
    }

    /*
     *  GENERATE SINGLE POINT CLOUD
     */

    void createPointCloudOpenCV (Mat& img1, Mat& img2,  Mat& Q, Mat& disp, Mat& recons3D, pcl::PointCloud<pcl::PointXYZRGB>::Ptr &point_cloud_ptr) {

        cv::reprojectImageTo3D(disp, recons3D, Q, true);
        FILE_LOG(logDEBUG) << "disp - " << binocular_dense_stereo::infoMatrix(disp);

        FILE_LOG(logDEBUG) << "reconst - " << binocular_dense_stereo::infoMatrix(recons3D) << " img - " << binocular_dense_stereo::infoMatrix(img1);
        pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud_xyzrgb(new PointCloudRGB);
        pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_xyz(new pcl::PointCloud<pcl::PointXYZ>);
        for (int rows = 0; rows < recons3D.rows; ++rows) {

            for (int cols = 0; cols < recons3D.cols; ++cols) {

                cv::Point3f point = recons3D.at<cv::Point3f>(rows, cols);

                pcl::PointXYZ pcl_point(point.x, point.y, point.z); // normal PointCloud
                pcl::PointXYZRGB pcl_point_rgb;
                pcl_point_rgb.x = point.x;    // rgb PointCloud
                pcl_point_rgb.y = point.y;
                pcl_point_rgb.z = point.z;
                // image_left is the binocular_dense_stereo rectified image used in stere reconstruction
                cv::Vec3b intensity = img1.at<cv::Vec3b>(rows, cols); //BGR

                uint32_t rgb = (static_cast<uint32_t>(intensity[2]) << 16 | static_cast<uint32_t>(intensity[1]) << 8 | static_cast<uint32_t>(intensity[0]));

                pcl_point_rgb.rgb = *reinterpret_cast<float *>(&rgb);

                // filter erroneus points
                if (pcl_point_rgb.z < 0)
                    point_cloud_ptr->push_back(pcl_point_rgb);
            }


        }

        point_cloud_ptr->width = (int) point_cloud_ptr->points.size();
        point_cloud_ptr->height = 1;

        FILE_LOG(logDEBUG) << "Esco..";
    }

    void createPointCloudOpenCVKITTI (Mat& img1, Mat& img2,  Mat& Q, Mat& disp, Mat& recons3D, PointCloudRGB::Ptr &point_cloud_ptr) {

        cv::reprojectImageTo3D(disp, recons3D, Q, true);
        FILE_LOG(logDEBUG) << "disp - " << binocular_dense_stereo::infoMatrix(disp);

        FILE_LOG(logDEBUG) << "reconst - " << binocular_dense_stereo::infoMatrix(recons3D) << " img - " << binocular_dense_stereo::infoMatrix(img1);
        PointCloudRGB::Ptr cloud_xyzrgb(new PointCloudRGB);
        PointCloudRGB::Ptr cloud_xyz(new PointCloudRGB);
        for (int rows = 0; rows < recons3D.rows; ++rows) {

            for (int cols = 0; cols < recons3D.cols; ++cols) {

                cv::Point3f point = recons3D.at<cv::Point3f>(rows, cols);

                pcl::PointXYZ pcl_point(point.x, point.y, point.z); // normal PointCloud
                PointTRGB pcl_point_rgb;
                pcl_point_rgb.x = point.x;    // rgb PointCloud
                pcl_point_rgb.y = point.y;
                pcl_point_rgb.z = point.z;
                // image_left is the binocular_dense_stereo rectified image used in stere reconstruction
//                double intensity = img1.at<double>(rows, cols); //BGR
                cv::Vec3b intensity = img1.at<cv::Vec3b>(rows, cols); //BGR
                uint32_t rgb = (static_cast<uint32_t>(intensity[2]) << 16 | static_cast<uint32_t>(intensity[1]) << 8 | static_cast<uint32_t>(intensity[0]));

//                uint32_t rgb = (static_cast<uint32_t>(intensity) << 16 | static_cast<uint32_t>(intensity) << 8 | static_cast<uint32_t>(intensity));

                pcl_point_rgb.rgb = *reinterpret_cast<float *>(&rgb);

                // filter erroneus points
//                if (pcl_point_rgb.z < 0)
                point_cloud_ptr->push_back(pcl_point_rgb);
            }


        }

        point_cloud_ptr->width = (int) point_cloud_ptr->points.size();
        point_cloud_ptr->height = 1;

        FILE_LOG(logDEBUG) << "Esco..";
    }

    PointCloudRGB::Ptr generatePointCloudTsukuba(Ptr<cv::datasets::tsukuba_dataset> &dataset, const int frame_num, bool show){

        FILE_LOG(logINFO) << "Loading data.. frame" << frame_num;

        // load images data
        Ptr<cv::datasets::tsukuba_datasetObj> data_stereo_img =
                static_cast< Ptr<cv::datasets::tsukuba_datasetObj> >  (dataset->getTrain()[frame_num]);


        FILE_LOG(logINFO) << "Loading images..";
        // load images
        cv::Mat img_left, img_right;
        cv::datasets::FramePair tuple_img = dataset->load_stereo_images(frame_num+1);
        img_left = tuple_img.frame_left;
        img_right = tuple_img.frame_right;

        // init
        Mat R1,R2,P1,P2,Q;
        // zero distiorsions
        Mat D_left = Mat::zeros(1, 5, CV_64F);
        Mat D_right = Mat::zeros(1, 5, CV_64F);

        // load K and R from dataset info
        Mat M_left = Mat(data_stereo_img->k);
        Mat M_right = Mat(data_stereo_img->k);

        // Left image
        Mat r_left = Mat(data_stereo_img->r);
        Mat t_left = Mat(3, 1, CV_64FC1, &data_stereo_img->tl);

        // Right image
        Mat r_right = Mat(data_stereo_img->r);
        Mat t_right = Mat(3, 1, CV_64FC1, &data_stereo_img->tr);

        // rotation between left and right
        // use ground truth rotation (img are already rectified
        cv::Mat R = Mat::eye(3,3, CV_64F); //r_right*r_left.inv();
        // translation between img2 and img1
//        cv::Mat T = t_left - (R.inv()*t_right );
        // use ground truth translation
        cv::Mat T = Mat::zeros(3, 1, CV_64F);
        T.at<double>(0,0) = 10.;

        FILE_LOG(logINFO) << "translation between cameras: " << T;

        FILE_LOG(logINFO) << "Rectifying images...";
//        Rect roi1,roi2;

//        cv::datasets::FramePair tuple_img_rect = binocular_dense_stereo::rectifyImages(img_left, img_right, M_left, D_left, M_right, D_right, R, T, R1, R2, P1, P2, Q, roi1, roi2, 1.f);
        // get the rectified images
        // img_left = tuple_img_rect.frame_left;
        //        img_right = tuple_img_rect.frame_right;

        Mat disp;
        disp = dataset->load_disparity(frame_num+1);
        FILE_LOG(logINFO) << "Creating point cloud..";

        FILE_LOG(logINFO) << "imgsize " << binocular_dense_stereo::infoMatrix(img_left);
//        binocular_dense_stereo::computeDisparityTsukuba(frame_num, img_left, img_right, disp);
        FILE_LOG(logINFO) << "dispsize " << binocular_dense_stereo::infoMatrix(disp);

        float baseline = 10;
        Mat depth_image(disp.size(), CV_32F);
        binocular_dense_stereo::depthFromDisparity (disp, M_left.at<double>(0,0), baseline, 0, depth_image, true);
        PointCloudRGB::Ptr point_cloud_ptr (new PointCloudRGB);
        binocular_dense_stereo::pointcloudFromDepthImage (depth_image, img_left, M_left, point_cloud_ptr);


//
//        FILE_LOG(logINFO) << "Computing Disparity map Dense Stereo";
//        Mat disp(img_left.size(), CV_32F);
//
//        FILE_LOG(logDEBUG) << "imgsize " << binocular_dense_stereo::infoMatrix(img_left);
//        FILE_LOG(logDEBUG) << "dispsize " << binocular_dense_stereo::infoMatrix(disp);
//
////        binocular_dense_stereo::computeDisparityTsukuba(frame_num, img_left, img_right, disp,1,roi1,roi2);
        // load ground truth disparity
//
//        FILE_LOG(logINFO) << "Creating point cloud..";
//        Mat recons3D(disp.size(), CV_32FC3);
//        FILE_LOG(logINFO) << "recons3Dsize " << binocular_dense_stereo::infoMatrix(recons3D);
//        FILE_LOG(logINFO) << "disp " << binocular_dense_stereo::infoMatrix(disp);
//
//        pcl::PointCloud<pcl::PointXYZRGB>::Ptr point_cloud_ptr (new pcl::PointCloud<pcl::PointXYZRGB>);
//        binocular_dense_stereo::createPointCloudOpenCV(img_left, img_right, Q, disp, recons3D, point_cloud_ptr);

        if (show)
            binocular_dense_stereo::viewPointCloudRGB(point_cloud_ptr, "cloud ");

        return point_cloud_ptr;
    }

    PointCloudRGB::Ptr generatePointCloudKITTI(Ptr<cv::datasets::SLAM_kitti> &dataset, const int frame_num, bool show){

        FILE_LOG(logINFO) << "Loading data.. frame" << frame_num;

        // load images data
        Ptr<cv::datasets::SLAM_kittiObj> data_stereo_img =
                static_cast< Ptr<cv::datasets::SLAM_kittiObj> >  (dataset->getTrain()[0]);


        FILE_LOG(logINFO) << "Loading images..";
        // load images
        cv::Mat img_left, img_right;
        cv::datasets::FramePair tuple_img = dataset->load_stereo_images(frame_num);
        img_left = tuple_img.frame_left;
        img_right = tuple_img.frame_right;

        // init
        Mat R1,R2,P1,P2,Q;

        // zero distiorsions
        Mat D_left = Mat::zeros(1, 5, CV_64F);
        Mat D_right = Mat::zeros(1, 5, CV_64F);

        // load K and R from dataset info
        Mat M_left = Mat(data_stereo_img->calibArray[frame_num].k);
        Mat M_right = Mat(data_stereo_img->calibArray[frame_num].k);

        FILE_LOG(logINFO) << "intrinsic matrix left " << binocular_dense_stereo::infoMatrix(M_left) << M_left;
        FILE_LOG(logINFO) << "intrinsic matrix right " << binocular_dense_stereo::infoMatrix(M_right) << M_right;


        // Left image
        Mat r_left = Mat(data_stereo_img->calibArray[frame_num].r);
//        Mat t_left = Mat(3, 1, CV_64FC1, &data_stereo_img->calibArray[frame_num].tl);

        // Right image
        Mat r_right = Mat(data_stereo_img->calibArray[frame_num].r);
//        Mat t_right = Mat(3, 1, CV_64FC1, &data_stereo_img->calibArray[frame_num].tr);

        // rotation between left and right
        // use ground truth rotation (img are already rectified
        cv::Mat R = Mat::eye(3,3, CV_64F); //r_right*r_left.inv();
        // translation between img2 and img1
//        cv::Mat T = t_left - (R.inv()*t_right );
        // use ground truth translation
        cv::Mat T = Mat::zeros(3, 1, CV_64F);
        T.at<double>(0,0) = 0.5372;




//        FILE_LOG(logINFO) << "translation between cameras: " << T;
//        FILE_LOG(logINFO) << "Rectifying images...";
//        Rect roi1,roi2;
////        binocular_dense_stereo::rectifyImages(img_left, img_right, M_left, D_left, M_right, D_right, R, T, R1, R2, P1, P2, Q, roi1, roi2, 1.f);
//        cv::datasets::FramePair tuple_img_rect = binocular_dense_stereo::rectifyImages(img_left, img_right, M_left, D_left, M_right, D_right, R, T, R1, R2, P1, P2, Q, roi1, roi2, 1.f);
//        // get the rectified images
////       img_left = tuple_img_rect.frame_left;
////       img_right = tuple_img_rect.frame_right;
//        imwrite("rect_l_"+std::to_string(frame_num)+".png", img_left);
//        imwrite("rect_r_"+std::to_string(frame_num)+".png", img_right);

        FILE_LOG(logINFO) << "Computing Disparity map Dense Stereo";
//        Mat disp(img_left.size(), CV_32F);
        Mat disp;

        FILE_LOG(logINFO) << "imgsize " << binocular_dense_stereo::infoMatrix(img_left);
        binocular_dense_stereo::computeDisparityTsukuba(frame_num, img_left, img_right, disp);
//        binocular_dense_stereo::compute_disparity_graphcuts(frame_num, img_left, img_right, disp);
        FILE_LOG(logINFO) << "dispsize " << binocular_dense_stereo::infoMatrix(disp);

        Mat depth_image(disp.size(), CV_32F);
        binocular_dense_stereo::depthFromDisparity (disp, M_left.at<double>(0,0), 0.5372, 0, depth_image, false);
        PointCloudRGB::Ptr point_cloud_ptr (new PointCloudRGB);
        binocular_dense_stereo::pointcloudFromDepthImage (depth_image, img_left, M_left, point_cloud_ptr);

        // load ground truth disparity
//        disp = dataset->load_disparity(frame_num+1);

//        FILE_LOG(logINFO) << "Creating point cloud..";
//        Mat recons3D(disp.size(), CV_32FC3);
//        FILE_LOG(logINFO) << "recons3Dsize " << binocular_dense_stereo::infoMatrix(recons3D);
//        FILE_LOG(logINFO) << "disp " << binocular_dense_stereo::infoMatrix(disp);

//        binocular_dense_stereo::createPointCloudOpenCVKITTI(img_left, img_right, Q, disp, recons3D, point_cloud_ptr);

        if (show)
            binocular_dense_stereo::viewPointCloudRGB(point_cloud_ptr, "cloud ");

        return point_cloud_ptr;
    }

    PointCloudRGB::Ptr generatePointCloudMiddlebury(Ptr<cv::datasets::MSM_middlebury> &dataset, const int frame_num, bool show){

        FILE_LOG(logINFO) << "Loading data.. frame" << frame_num;

        FILE_LOG(logINFO) << "Loading associations.. ";
        std::vector<binocular_dense_stereo::middleburyPair> association_pairs = binocular_dense_stereo::ConfigLoader::get_instance().loadMiddleburyAssociations();

        // load images data
        Ptr<cv::datasets::MSM_middleburyObj> data_stereo_left_img =
                static_cast< Ptr<cv::datasets::MSM_middleburyObj> >  (dataset->getTrain()[association_pairs[frame_num].left_image]);
        Ptr<cv::datasets::MSM_middleburyObj> data_stereo_right_img =
                static_cast< Ptr<cv::datasets::MSM_middleburyObj> >  (dataset->getTrain()[association_pairs[frame_num].right_image]);

        FILE_LOG(logINFO) << "Loading images..";
        // load images
        cv::Mat img_left, img_right;
        cv::datasets::FramePair tuple_img = dataset->load_stereo_images(frame_num);
        img_left = tuple_img.frame_left;
        img_right = tuple_img.frame_right;

        // init
        Mat R1,R2,P1,P2,Q;

        // zero distiorsions
        Mat D_left = Mat::zeros(1, 5, CV_64F);
        Mat D_right = Mat::zeros(1, 5, CV_64F);

        // load K and R from dataset info
        Mat M_left = Mat(data_stereo_left_img->k);
        Mat M_right = Mat(data_stereo_right_img->k);

        FILE_LOG(logINFO) << "intrinsic matrix left " << binocular_dense_stereo::infoMatrix(M_left) << M_left;
        FILE_LOG(logINFO) << "intrinsic matrix right " << binocular_dense_stereo::infoMatrix(M_right) << M_right;

        // Left image
        Mat r_left = Mat(data_stereo_left_img->r);
        Mat t_left = Mat(3, 1, CV_64FC1, &data_stereo_left_img->t);

        // Right image
        Mat r_right = Mat(data_stereo_right_img->r);
        Mat t_right = Mat(3, 1, CV_64FC1, &data_stereo_right_img->t);

        // rotation between left and right
        cv::Mat R = r_right*r_left.t();
        // translation between img2 and img1
        cv::Mat T = t_left - (R.t()*t_right );


        FILE_LOG(logINFO) << "translation between cameras: " << T;
        FILE_LOG(logINFO) << "Rectifying images...";
        Rect roi1,roi2;
//        binocular_dense_stereo::rectifyImages(img_left, img_right, M_left, D_left, M_right, D_right, R, T, R1, R2, P1, P2, Q, roi1, roi2, 1.f);
        cv::datasets::FramePair tuple_img_rect = binocular_dense_stereo::rectifyImages(img_left, img_right, M_left, D_left, M_right, D_right, R, T, R1, R2, P1, P2, Q, roi1, roi2, 1.f);
        // get the rectified images
        img_left = tuple_img_rect.frame_left;
        img_right = tuple_img_rect.frame_right;
        imwrite("middle_rect_l_"+std::to_string(frame_num)+".png", img_left);
        imwrite("middle_rect_r_"+std::to_string(frame_num)+".png", img_right);

        FILE_LOG(logINFO) << "P1 after rectify " << P1;
        FILE_LOG(logINFO) << "P2 after rectify " << P2;
        FILE_LOG(logINFO) << "R1 after rectify " << R1;
        FILE_LOG(logINFO) << "R2 after rectify " << R2;



        float baseline = abs(P1.at<double>(1,3) - P2.at<double>(1,3)); //P1.at<double>(0,0);
        FILE_LOG(logINFO) << "baseline:"<<baseline<<" tx1: "<< P1.at<double>(1,3)<< " tx2: "<<P2.at<double>(1,3)<< " f: "<< P1.at<double>(0,0);

        FILE_LOG(logINFO) << "Computing Disparity map Dense Stereo";
//        Mat disp(img_left.size(), CV_32F);
        Mat disp;

        FILE_LOG(logINFO) << "imgsize " << binocular_dense_stereo::infoMatrix(img_left);
        binocular_dense_stereo::computeDisparityMiddlebury(frame_num, img_left, img_right, disp);
//        binocular_dense_stereo::compute_disparity_graphcuts(frame_num, img_left, img_right, disp);
        FILE_LOG(logINFO) << "dispsize " << binocular_dense_stereo::infoMatrix(disp);

//        disp = -disp;
        Mat depth_image(disp.size(), CV_32F);
        binocular_dense_stereo::depthFromDisparity (disp, M_left.at<double>(0,0), baseline, -1000, depth_image, false);
        PointCloudRGB::Ptr point_cloud_ptr (new PointCloudRGB);
        binocular_dense_stereo::pointcloudFromDepthImage (depth_image, img_left, M_left, point_cloud_ptr);

        // load ground truth disparity
//        disp = dataset->load_disparity(frame_num+1);

//        FILE_LOG(logINFO) << "Creating point cloud..";
//        Mat recons3D(disp.size(), CV_32FC3);
//        FILE_LOG(logINFO) << "recons3Dsize " << binocular_dense_stereo::infoMatrix(recons3D);
//        FILE_LOG(logINFO) << "disp " << binocular_dense_stereo::infoMatrix(disp);

//        binocular_dense_stereo::createPointCloudOpenCVKITTI(img_left, img_right, Q, disp, recons3D, point_cloud_ptr);

        // rotation between img2 and img1

//        Mat R_INV = R1.t();
//
//
//        Eigen::Matrix4d transformMatrix = Eigen::Matrix4d::Identity();
//
//        //        FILE_LOG(logINFO) << "R float: " <<  R.at<float>(0,0) << " double:" << R.at<double>(0,0);
//
//        transformMatrix (0,0) = R_INV.at<double>(0,0);
//        transformMatrix (0,1) = R_INV.at<double>(0,1);
//        transformMatrix (0,2) = R_INV.at<double>(0,2);
//
//        transformMatrix (1,0) = R_INV.at<double>(1,0);
//        transformMatrix (1,1) = R_INV.at<double>(1,1);
//        transformMatrix (1,2) = R_INV.at<double>(1,2);
//        transformMatrix (2,0) = R_INV.at<double>(2,0);
//        transformMatrix (2,1) = R_INV.at<double>(2,1);
//        transformMatrix (2,2) = R_INV.at<double>(2,2);
//        //
//        //        transformMatrix (0,3) = T.at<double>(0);
//        //        transformMatrix (1,3) = T.at<double>(1);
//        //        transformMatrix (2,3) = T.at<double>(2);
//        pcl::PointCloud<pcl::PointXYZRGB>::Ptr transformed_cloud (new pcl::PointCloud<pcl::PointXYZRGB> ());
//        pcl::transformPointCloud (*point_cloud_ptr, *transformed_cloud, transformMatrix);
////        return transformed_cloud;

        if (show)
            binocular_dense_stereo::viewPointCloudRGB(point_cloud_ptr, "cloud ");

        return point_cloud_ptr;
    }


    /*
     *  GENERATE ALL CLOUDS
     */

    void showCloudArraySum(std::vector<PointCloudRGB::Ptr> & clouds){
        pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud_sum(new pcl::PointCloud<pcl::PointXYZRGB>);

        for (int i=0; i<clouds.size();i++){
            if (i == 0)
                pcl::copyPointCloud(*clouds[i], *cloud_sum);
            else
                *cloud_sum+=*clouds[i];


        }
        binocular_dense_stereo::viewPointCloudRGB(cloud_sum, " dataset to world");
        pcl::io::savePLYFileASCII ("sum.ply", *cloud_sum);

    }

    void createAllCloudsTsukuba(Ptr<cv::datasets::tsukuba_dataset> &dataset, std::vector<pcl::PointCloud<pcl::PointXYZRGB>::Ptr> & clouds, int first_frame,  int last_frame, int step, bool show_single){

        pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud;

        int frame_num;
        for (int i=first_frame; i<last_frame; i+=step){

            frame_num = i;

            cloud = generatePointCloudTsukuba(dataset, frame_num, show_single);

            if(!(*cloud).empty()){

                Eigen::Matrix4d transf = binocular_dense_stereo::getTransformToWorldCoordinatesTsukuba(dataset, frame_num);
                // Executing the transformation
                pcl::PointCloud<pcl::PointXYZRGB>::Ptr transformed_cloud (new pcl::PointCloud<pcl::PointXYZRGB> ());
                // You can either apply transform_1 or transform_2; they are the same
                pcl::transformPointCloud (*cloud, *transformed_cloud, transf);
                clouds.push_back(transformed_cloud);
            }


        }

        FILE_LOG(logINFO) << "cloud size" <<clouds.size();

    }

    void createAllCloudsMiddelbury(Ptr<cv::datasets::MSM_middlebury> &dataset, std::vector<pcl::PointCloud<pcl::PointXYZRGB>::Ptr> & clouds, int first_frame,  int last_frame, int step, bool show_single){

        pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud;

        int frame_num;
        for (int i=first_frame; i<last_frame; i+=step){

            frame_num = i;

            FILE_LOG(logINFO) << "generating cloud : " << i;
            cloud = generatePointCloudMiddlebury(dataset, frame_num, show_single);

            if(!(*cloud).empty()){

                if (i!=first_frame){
                    Eigen::Matrix4d transf = binocular_dense_stereo::getTransformBetweenClouds(dataset, first_frame, frame_num);
                    // Executing the transformation
                    pcl::PointCloud<pcl::PointXYZRGB>::Ptr transformed_cloud (new pcl::PointCloud<pcl::PointXYZRGB> ());
                    // You can either apply transform_1 or transform_2; they are the same
                    pcl::transformPointCloud (*cloud, *transformed_cloud, transf);
                    clouds.push_back(transformed_cloud);
                }
                else {
                    clouds.push_back(cloud);

                }
////                Eigen::Matrix4d transf = binocular_dense_stereo::getTransformToWorldCoordinatesMiddlebury(dataset, frame_num);
//                Eigen::Matrix4d transf = binocular_dense_stereo::getTransformBetweenClouds(dataset, first_frame, frame_num);
//                // Executing the transformation
//                pcl::PointCloud<pcl::PointXYZRGB>::Ptr transformed_cloud (new pcl::PointCloud<pcl::PointXYZRGB> ());
//                // You can either apply transform_1 or transform_2; they are the same
//                pcl::transformPointCloud (*cloud, *transformed_cloud, transf);
//                clouds.push_back(transformed_cloud);

            }

        }

        FILE_LOG(logINFO) << "cloud size" <<clouds.size();

    }

    void createAllCloudsKITTI(Ptr<cv::datasets::SLAM_kitti> &dataset, std::vector<PointCloudRGB::Ptr> & clouds, int first_frame,  int last_frame, int step, bool show_single){

        PointCloudRGB::Ptr cloud(new PointCloudRGB), cloud_not_filtered(new PointCloudRGB);

        int frame_num;
        for (int i=first_frame; i<last_frame; i+=step){

            frame_num = i;

            cloud_not_filtered = generatePointCloudKITTI(dataset, frame_num, show_single);

            PointTRGB min_pt, max_pt;
            pcl::getMinMax3D	(	*cloud, 	min_pt, max_pt);
            FILE_LOG(logINFO) << " min pt:" << min_pt;
            FILE_LOG(logINFO) << " max pt:" << max_pt;


//            binocular_dense_stereo::viewPointCloud(cloud, " singola");

            FILE_LOG(logINFO) << "Doing first filtering ";

            PointCloudRGB::Ptr tmp(new PointCloudRGB);
            PointCloudRGB::Ptr tmp2(new PointCloudRGB);
//            binocular_dense_stereo::viewPointCloudRGB(cloud_not_filtered, "no filtered");

            pcl::PassThrough<PointTRGB> pass;
            pass.setInputCloud (cloud_not_filtered);
            pass.setFilterFieldName ("x");
            pass.setFilterLimits (-20.0, 20.0);
            pass.filter (*tmp2); // NPLACE SE RIUSI CAMBIA

            pass.setInputCloud (tmp2);
            pass.setFilterFieldName ("y");
            pass.setFilterLimits (-10.0, 2);
            pass.filter (*tmp); // NPLACE SE RIUSI CAMBIA

            pass.setInputCloud (tmp);
            pass.setFilterFieldName ("z");
            pass.setFilterLimits (0.0, 200.0);
            pass.filter (*cloud); // NPLACE SE RIUSI CAMBIA

//            binocular_dense_stereo::viewPointCloudRGB(cloud, "yes filtered");


            if(!(*cloud).empty()){

                Eigen::Matrix4d transf = binocular_dense_stereo::getTransformToWorldCoordinatesKITTI(dataset, frame_num);
                // Executing the transformation
                PointCloudRGB::Ptr transformed_cloud (new PointCloudRGB ());
                // You can either apply transform_1 or transform_2; they are the same
                pcl::transformPointCloud (*cloud, *transformed_cloud, transf);
                clouds.push_back(transformed_cloud);

                FILE_LOG(logINFO) << "cloud "+std::to_string(i)+" size" <<transformed_cloud->size();

//                binocular_dense_stereo::viewPointCloudRGB(transformed_cloud, "cloud "+std::to_string(i));

            }

        }

        FILE_LOG(logINFO) << "cloud size" <<clouds.size();

    }



    //GRAPHCUTS FUNCTIONS
    /// Max denominator for fractions. We need to approximate float values as
/// fractions since the max-flow is implemented using short integers. The
/// denominator multiplies the data term in Match::data_occlusion_penalty. To
/// avoid overflow, we have to make sure this stays below 2^15 (max short).
/// The data term can reach (CUTOFF=30<2^5)^2<2^10 if using L2 norm, so a
/// denominator up to 2^4 will reach 2^14 and not provoke overflow.
    static const int MAX_DENOM=1<<4;

/// Is the color image actually gray?
    bool isGray(RGBImage im) {
        const int xsize=imGetXSize(im), ysize=imGetYSize(im);
        for(int y=0; y<ysize; y++)
            for(int x=0; x<xsize; x++)
                if(imRef(im,x,y).c[0] != imRef(im,x,y).c[1] ||
                   imRef(im,x,y).c[0] != imRef(im,x,y).c[2])
                    return false;
        return true;
    }

/// Convert to gray level a color image (extract red channel)
    void convert_gray(GeneralImage& im) {
        const int xsize=imGetXSize(im), ysize=imGetYSize(im);
        GrayImage g = (GrayImage)imNew(IMAGE_GRAY, xsize, ysize);
        for(int y=0; y<ysize; y++)
            for(int x=0; x<xsize; x++)
                imRef(g,x,y) = imRef((RGBImage)im,x,y).c[0];
        imFree(im);
        im = (GeneralImage)g;
    }

/// Store in \a params fractions approximating the last 3 parameters.
///
/// They have the same denominator (up to \c MAX_DENOM), chosen so that the sum
/// of relative errors is minimized.
    void set_fractions(Match::Parameters& params,
                       float K, float lambda1, float lambda2) {
        float minError = std::numeric_limits<float>::max();
        for(int i=1; i<=MAX_DENOM; i++) {
            float e = 0;
            int numK=0, num1=0, num2=0;
            if(K>0)
                e += std::abs((numK=int(i*K+.5f))/(i*K) - 1.0f);
            if(lambda1>0)
                e += std::abs((num1=int(i*lambda1+.5f))/(i*lambda1) - 1.0f);
            if(lambda2>0)
                e += std::abs((num2=int(i*lambda2+.5f))/(i*lambda2) - 1.0f);
            if(e<minError) {
                minError = e;
                params.denominator = i;
                params.K = numK;
                params.lambda1 = num1;
                params.lambda2 = num2;
            }
        }
    }

/// Make sure parameters K, lambda1 and lambda2 are non-negative.
///
/// - K may be computed automatically and lambda set to K/5.
/// - lambda1=3*lambda, lambda2=lambda
/// As the graph requires integer weights, use fractions and common denominator.
    void fix_parameters(Match& m, Match::Parameters& params,
                        float& K, float& lambda, float& lambda1, float& lambda2) {
        if(K<0) { // Automatic computation of K
            m.SetParameters(&params);
            K = m.GetK();
        }
        if(lambda<0) // Set lambda to K/5
            lambda = K/5;
        if(lambda1<0) lambda1 = 3*lambda;
        if(lambda2<0) lambda2 = lambda;
        set_fractions(params, K, lambda1, lambda2);
        m.SetParameters(&params);
    }
///////////////
    void compute_disparity_graphcuts(const int img_frame, Mat& img_left, Mat& img_right, Mat& disp){


        string path("../dataset/KITTI/");

        std::ostringstream ss;
        ss << std::setw(6) << std::setfill('0') << img_frame;
        std::string img1_path = path + "sequences/07/image_0/"+ ss.str() + ".png";
        std::string img2_path = path + "sequences/07/image_1/"+ ss.str() + ".png";



        FILE_LOG(logINFO) << "michi disp "<< img1_path;

        Match::Parameters params = { // Default parameters
                Match::Parameters::L2, 1, // dataCost, denominator
                8, -1, -1, // edgeThresh, lambda1, lambda2 (smoothness cost)
                -1,        // K (occlusion cost)
                4, false   // maxIter, bRandomizeEveryIteration
        };


        float K=-1, lambda=-1, lambda1=-1, lambda2=-1;


        params.dataCost = Match::Parameters::L1;

//      params.dataCost = Match::Parameters::L2;

        GeneralImage im1 = (GeneralImage)imLoad(IMAGE_GRAY, img1_path.c_str());
        GeneralImage im2 = (GeneralImage)imLoad(IMAGE_GRAY, img2_path.c_str());


        bool color=false;
//        if(isGray((RGBImage)im1) && isGray((RGBImage)im2)) {
//            color=false;
//            convert_gray(im1);
//            convert_gray(im2);
//        }

        Match m(im1, im2, color);
//
//////    // Disparity
        int dMin=-15, dMax=0;
//
        m.SetDispRange(dMin, dMax);

        time_t seed = time(NULL);
        srand((unsigned int)seed);

        fix_parameters(m, params, K, lambda, lambda1, lambda2);

        m.KZ2();

//        m.SaveXLeft(argv[5]);

        m.SaveScaledXLeft("disp.png", false);

        disp = imread("disp.png");
//        imshow ("cacca", disp);
//        waitKey(0):



    }
}
