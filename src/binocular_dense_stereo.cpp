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


#include "includes.h"

// custom includes
#include "dataset/msm_middlebury.hpp"
#include "dataset/kitti_dataset.h"
#include "matching_reproject/stereo_matching.hpp"
#include "utils/util.hpp"
#include "stereo_viewer/viewer.hpp"
#include "registration/registration.hpp"

// Include logging facilities
#include "logger/log.h"
#include "dataset/tsukuba_dataset.h"

#include "config/config.hpp"

using namespace std;
using namespace cv;
using namespace cv::datasets;


int main(int argc, char *argv[])
{

    FILE_LOG(logINFO) << "Binocular Dense Stereo";

//    string dataset_type = "middlebury";
//    string dataset_type = "tsukuba";
//    string dataset_type = "kitti";


//    string path("../dataset/dataset_templeRing_segm/");
//    Ptr<MSM_middlebury> dataset = MSM_middlebury::create();
//    int datasetType = binocular_dense_stereo::datasetType::MIDDLEBURY;
//
//    string path("../dataset/NTSD-200/");
//    Ptr<tsukuba_dataset> dataset = tsukuba_dataset::create();
//    int datasetType = binocular_dense_stereo::datasetType::TSUKUBA;

//
    string path("../dataset/KITTI/");
    Ptr<SLAM_kitti> dataset = SLAM_kitti::create();
    int datasetType = binocular_dense_stereo::datasetType::KITTI;





    dataset->load(path);
    // dataset contains camera parameters for each image.
    FILE_LOG(logINFO) << "images number: " << (unsigned int)dataset->getTrain().size();


    FILE_LOG(logINFO) << "Loading params from config.cfg.. ";
    binocular_dense_stereo::configPars pars = binocular_dense_stereo::ConfigLoader::get_instance().loadGeneralConfiguration(datasetType);

    bool load_clouds = pars.load_clouds;
    int load_n_clouds = pars.load_n_clouds;
    int first_frame = pars.first_frame;
    int last_frame = pars.last_frame;
    int step = pars.step;
    bool incremental = pars.incremental;
    int cloud_num_1 = pars.reg_cloud_1;
    int cloud_num_2 = pars.reg_cloud_2;

    // override from cmdline
    if (pcl::console::find_switch (argc, argv, "-load")) {
        pcl::console::parse (argc, argv, "-load", load_clouds);
        if (pcl::console::find_switch (argc, argv, "-n_clouds"))
            pcl::console::parse (argc, argv, "-n_clouds", load_n_clouds);

    } else {
        if (pcl::console::find_switch (argc, argv, "-start")) {
            if (pcl::console::find_switch (argc, argv, "-end")) {
                pcl::console::parse (argc, argv, "-start", first_frame);
                pcl::console::parse (argc, argv, "-end", last_frame);
                if (pcl::console::find_switch (argc, argv, "-step"))
                    pcl::console::parse (argc, argv, "-step", step);

            }
        }
    }
    if (pcl::console::find_switch (argc, argv, "-incremental")) {
        pcl::console::parse (argc, argv, "-incremental", incremental);

        if (not incremental) {
            if (pcl::console::find_switch (argc, argv, "-cloud1")) {
                pcl::console::parse (argc, argv, "-cloud1", cloud_num_1);
            }
            if (pcl::console::find_switch (argc, argv, "-cloud2")) {
                pcl::console::parse (argc, argv, "-cloud2", cloud_num_2);
            }
        }
    }
    // end cmdline

    FILE_LOG(logINFO) << "\tload_clouds: " << (load_clouds ? "True" : "False");
    if (load_clouds){
        FILE_LOG(logINFO) << "\t\tload_n_clouds: " << load_n_clouds;
    } else {
        FILE_LOG(logINFO) << "\t\tfirst_frame: " << first_frame;
        FILE_LOG(logINFO) << "\t\tlast_frame: " << last_frame;
        FILE_LOG(logINFO) << "\t\tstep: " << step;
    }
    FILE_LOG(logINFO) << "\tincremental: " << (incremental ? "True" : "False");
    if (not incremental) {
        FILE_LOG(logINFO) << "\t\tcloud_num_1:" << cloud_num_1;
        FILE_LOG(logINFO) << "\t\tcloud_num_2: " << cloud_num_2;
    }



    if (incremental) {

        std::vector< PointCloudRGB::Ptr> clouds;
        if (load_clouds) {

            clouds = binocular_dense_stereo::loadVectorCloudsFromPCDRGB("./original-", load_n_clouds);
            FILE_LOG(logINFO) << "Loading : " << load_n_clouds << " saved clouds";
        } else {

            FILE_LOG(logINFO) << "Generating clouds from frame : " << first_frame << " to frame " << last_frame <<
                              " with step " << step;

//            binocular_dense_stereo::createAllCloudsTsukuba(dataset, clouds, first_frame, last_frame, step, pars.show_single_cloud);
            binocular_dense_stereo::createAllCloudsKITTI(dataset, clouds, first_frame, last_frame, step, pars.show_single_cloud);
//            binocular_dense_stereo::createAllCloudsMiddelbury(dataset, clouds, first_frame, last_frame, step, pars.show_single_cloud);

            if (pars.save_generated_clouds){
                binocular_dense_stereo::saveVectorCloudsToPCDRGB(clouds, "original");
                binocular_dense_stereo::saveVectorCloudsToPLYRGB(clouds, "original");
            }

            if (pars.show_sum_cloud)
                binocular_dense_stereo::showCloudArraySum(clouds);

            FILE_LOG(logINFO) << "We have used: " << clouds.size() << " clouds from dataset. From images: ";
            for (int i=first_frame; i<last_frame; i+=step){
                FILE_LOG(logINFO) << "image: " << i+1;
            }

        }

        // TOTAL REGISTRATION using incremental
        FILE_LOG(logINFO) << "Doing incremental registration ";
        binocular_dense_stereo::registrationParams pars = binocular_dense_stereo::ConfigLoader::get_instance().loadRegistrationParams(datasetType);

        PointCloudRGB::Ptr final_cloud = binocular_dense_stereo::register_incremental_clouds(clouds, pars);

//        pcl::VoxelGrid<PointTRGB> grid;
//        float leafSize = 0.05;
//        grid.setLeafSize (leafSize, leafSize, leafSize);
//        grid.setInputCloud (final_cloud);
//        FILE_LOG(logINFO) << "finalcloud leafSize: " << leafSize<< " original size :" << final_cloud->size();
//        grid.filter (*final_cloud);
//        FILE_LOG(logINFO) << "finalcloud leafSize: " << leafSize<< " downsampled size :" << final_cloud->size();

//        binocular_dense_stereo::viewPointCloudRGB(final_cloud,  " registration");

        FILE_LOG(logINFO) << "Doing filtering ";

        PointCloudRGB::Ptr cloud_filtered(new PointCloudRGB);
        pcl::StatisticalOutlierRemoval<pcl::PointXYZRGB> sor;
        sor.setInputCloud (final_cloud);
        sor.setMeanK (150);
        sor.setStddevMulThresh (1.0);
        sor.filter (*cloud_filtered);
        binocular_dense_stereo::viewPointCloudRGB(cloud_filtered,  " registration filtered");
        pcl::io::savePLYFileASCII ("reg_filtered.ply", *cloud_filtered);

        // END REGISTRATION using incremental
    } else {

        // TEST TWO CLOUD REGISTRATION
        FILE_LOG(logINFO) << "Doing clouds " << cloud_num_1 << " and " << cloud_num_2 << " registration ";

        PointCloudRGB::Ptr cloud1 (new PointCloudRGB);
        PointCloudRGB::Ptr cloud2 (new PointCloudRGB);

        if (load_clouds) {

            pcl::io::loadPCDFile("./original-"+std::to_string(cloud_num_1)+".pcd", *cloud1);
            pcl::io::loadPCDFile("./original-"+std::to_string(cloud_num_2)+".pcd", *cloud2);

        } else {

            std::vector< PointCloudRGB::Ptr> clouds;

            FILE_LOG(logINFO) << "Generating clouds from frame : " << first_frame << " to frame " << last_frame <<
                              " with step " << step;

//            binocular_dense_stereo::createAllCloudsTsukuba(dataset, clouds, first_frame, last_frame, step, pars.show_single_cloud);
            binocular_dense_stereo::createAllCloudsKITTI(dataset, clouds, first_frame, last_frame, step, pars.show_single_cloud);
//            binocular_dense_stereo::createAllCloudsMiddelbury(dataset, clouds, first_frame, last_frame, step, pars.show_single_cloud);

            if (pars.save_generated_clouds) {
                binocular_dense_stereo::saveVectorCloudsToPCDRGB(clouds, "original");
                binocular_dense_stereo::saveVectorCloudsToPLYRGB(clouds, "original");
            }

            if (pars.show_sum_cloud)
                binocular_dense_stereo::showCloudArraySum(clouds);
            
            FILE_LOG(logINFO) << "We have used: " << clouds.size() << " clouds from dataset. From images: ";
            for (int i=first_frame; i<last_frame; i+=step){
                FILE_LOG(logINFO) << "image: " << i+1;
            }

            cloud1 = clouds[cloud_num_1];
            cloud2 = clouds[cloud_num_2];

        }

        PointCloudRGB::Ptr batch_cloud_sum(new PointCloudRGB);
        pcl::copyPointCloud(*cloud1, *batch_cloud_sum);
        binocular_dense_stereo::registrationParams pars = binocular_dense_stereo::ConfigLoader::get_instance().loadRegistrationParams(datasetType);

        binocular_dense_stereo::CloudAlignment cloud_align = binocular_dense_stereo::registerSourceToTarget(cloud2, cloud1, pars);
        PointCloudRGB::Ptr cloud_source_in_target_space(new PointCloudRGB);
        pcl::transformPointCloud (*cloud2, *cloud_source_in_target_space, cloud_align.transformMatrix);
        *batch_cloud_sum += *cloud_source_in_target_space;
        binocular_dense_stereo::viewPointCloudRGB(batch_cloud_sum, std::to_string(cloud_num_1)+" - "+std::to_string(cloud_num_2));

    }





    // TEST A COPPIE
//    int destination = 0;
//    int to_move = 0;
//    for (int i = 0; i<10 ; i ++){
//        destination =i*2;
//        to_move =i*2+1;
//
//        PointCloud::Ptr batch_cloud_sum2(new PointCloud);
//        pcl::copyPointCloud(*clouds[destination], *batch_cloud_sum2);
//        binocular_dense_stereo::CloudAlignment cloud_align2 = binocular_dense_stereo::registerSourceToTarget(clouds[to_move], clouds[destination]);
//        PointCloud::Ptr cloud_source_in_target_space2(new PointCloud);
//        pcl::transformPointCloud (*clouds[to_move], *cloud_source_in_target_space2, cloud_align2.transformMatrix);
//        *batch_cloud_sum2 += *cloud_source_in_target_space2;
//        binocular_dense_stereo::viewPointCloud(batch_cloud_sum2, std::to_string(destination)+"-"+std::to_string(to_move));
//
//    }


//    // TOTAL REGISTRATION using batch
//    std::vector< PointCloud::Ptr> clouds_array = clouds;// = binocular_dense_stereo::register_clouds_in_batches(clouds, 2);
//    int k = 0;
//    while (clouds.size() != 1) {
//        FILE_LOG(logINFO) << "Iterazione: " << k;
//        clouds = binocular_dense_stereo::register_clouds_in_batches(clouds, 2);
////        binocular_dense_stereo::saveVectorCloudsToPLYRGB(clouds, "register-step-"+std::to_string(k));
////        binocular_dense_stereo::viewPointCloud(clouds_array[0], " step "+std::to_string(k));
//
//        k++;
//    }
//    FILE_LOG(logINFO) << " post size :" << clouds[0]->size() << " ; total clouds = " << clouds.size();
////        binocular_dense_stereo::viewPointCloud(clouds_array[0], " step "+std::to_string(k));
//    // END TOTAL REGISTRATION using batch





    return 0;

}
