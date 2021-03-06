#include <boost/make_shared.hpp>
#include <pcl/visualization/pcl_visualizer.h>
#include <pcl/impl/point_types.hpp>

// local include
#include "viewer.hpp"


// Include logging facilities
#include "../logger/log.h"

namespace binocular_dense_stereo {


    using pcl::visualization::PointCloudColorHandlerGenericField;
    using pcl::visualization::PointCloudColorHandlerCustom;


    void viewPointCloud(PointCloud::Ptr point_cloud_ptr, std::string title) {
        //Create visualizer
        boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer;
        viewer = createVisualizer( point_cloud_ptr, title);
        viewer->resetCamera();
        viewer->resetCameraViewpoint ("reconstruction");
        //Main loop
        while ( !viewer->wasStopped())
        {
            viewer->spinOnce(100);
            boost::this_thread::sleep (boost::posix_time::microseconds (100000));
        }
        viewer->close();
    }
    void viewPointCloudRGB(PointCloudRGB::Ptr point_cloud_ptr, std::string title) {
        //Create visualizer
        boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer;
        viewer = createVisualizerRGB( point_cloud_ptr, title);
        viewer->resetCamera();
        viewer->resetCameraViewpoint ("reconstruction");
//        viewer->resetCamera();

        //Main loop
        while ( !viewer->wasStopped())
        {
            viewer->spinOnce(100);
            boost::this_thread::sleep (boost::posix_time::microseconds (100000));
        }
        viewer->close();
    }
//
//    inline PointT Eigen2PointXYZRGB(Eigen::Vector3f v, Eigen::Vector3f rgb) { PointT p(rgb[0],rgb[1],rgb[2]); p.x = v[0]; p.y = v[1]; p.z = v[2]; return p; }
//
//    pcl::PolygonMesh visualizerGetCameraMesh(const Eigen::Matrix3f& R, const Eigen::Vector3f& t, float r, float g, float b,
//            Eigen::Vector3f& vforward, Eigen::Vector3f& rgb, double s = 0.01
//    )
//    {
//        Eigen::Vector3f vright = R.row(0).normalized() * s;
//        Eigen::Vector3f vup = -R.row(1).normalized() * s;
//        vforward = R.row(2).normalized() * s;
//
//        rgb = Eigen::Vector3f(r,g,b);
//
//        PointCloud mesh_cld;
//        mesh_cld.push_back(Eigen2PointXYZRGB(t,rgb));
//        mesh_cld.push_back(Eigen2PointXYZRGB(t + vforward + vright/2.0 + vup/2.0,rgb));
//        mesh_cld.push_back(Eigen2PointXYZRGB(t + vforward + vright/2.0 - vup/2.0,rgb));
//        mesh_cld.push_back(Eigen2PointXYZRGB(t + vforward - vright/2.0 + vup/2.0,rgb));
//        mesh_cld.push_back(Eigen2PointXYZRGB(t + vforward - vright/2.0 - vup/2.0,rgb));
//
//        pcl::PolygonMesh pm;
//        pm.polygons.resize(6);
//        for(int i=0;i<6;i++)
//            for(int _v=0;_v<3;_v++)
//                pm.polygons[i].vertices.push_back(ipolygon[i*3 + _v]);
////        pcl::toROSMsg(mesh_cld, pm.cloud);
//
//        return pm;
//    }


    void viewDoublePointCloud(PointCloud::Ptr point_cloud_ptr, PointCloud::Ptr point_cloud_ptr2,  std::string title) {
        //Create visualizer
        boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer;
        viewer = createDoubleVisualizer(point_cloud_ptr, point_cloud_ptr2, title);

        //Main loop
        while ( !viewer->wasStopped())
        {
            viewer->spinOnce(100);
            boost::this_thread::sleep (boost::posix_time::microseconds (100000));
        }
        viewer->close();
    }
    void viewDoublePointCloudRGB(PointCloudRGB::Ptr point_cloud_ptr, PointCloudRGB::Ptr point_cloud_ptr2,  std::string title) {
        //Create visualizer
        boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer;
        viewer = createDoubleVisualizerRGB(point_cloud_ptr, point_cloud_ptr2, title);

        //Main loop
        while ( !viewer->wasStopped())
        {
            viewer->spinOnce(100);
            boost::this_thread::sleep (boost::posix_time::microseconds (100000));
        }
        viewer->close();
    }

    boost::shared_ptr<pcl::visualization::PCLVisualizer> createVisualizer (PointCloud::ConstPtr cloud, std::string title) {

        boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer (new pcl::visualization::PCLVisualizer(title));
        viewer->setBackgroundColor (0.3, 0.3, 0.3);

//        pcl::visualization::PointCloudColorHandlerRGBField<PointT> rgb(cloud);
        viewer->addPointCloud<PointT> (cloud, "reconstruction");
        //viewer->setPointCloudRenderingProperties (pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 3, "reconstruction");
        viewer->addCoordinateSystem ( 1.0 );
        viewer->initCameraParameters ();
        //viewer->spin();
        return (viewer);
    }

    boost::shared_ptr<pcl::visualization::PCLVisualizer> createVisualizerRGB (PointCloudRGB::ConstPtr cloud, std::string title) {

        boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer (new pcl::visualization::PCLVisualizer(title));
        viewer->setBackgroundColor (0.3, 0.3, 0.3);

        pcl::visualization::PointCloudColorHandlerRGBField<PointTRGB> rgb(cloud);
        viewer->addPointCloud<PointTRGB> (cloud, rgb, "reconstruction");
        //viewer->setPointCloudRenderingProperties (pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 3, "reconstruction");
        viewer->addCoordinateSystem ( 1.0 );
        viewer->initCameraParameters ();
        //viewer->spin();
        return (viewer);
    }
    boost::shared_ptr<pcl::visualization::PCLVisualizer> createDoubleVisualizer (
            PointCloud::ConstPtr cloud, PointCloud::ConstPtr cloud2,  std::string title) {

        // --------------------------------------------------------
        // -----Open 3D viewer and add point cloud and normals-----
        // --------------------------------------------------------
        boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer (new pcl::visualization::PCLVisualizer (title));
        viewer->initCameraParameters ();

        int v1(0);
        viewer->createViewPort(0.0, 0.0, 0.5, 1.0, v1);
        viewer->setBackgroundColor (0.3, 0.3, 0.3, v1);
        viewer->addText("Radius: 0.01", 10, 10, "v1 text", v1);
        viewer->addPointCloud<PointT> (cloud, "sample cloud1", v1);

        int v2(0);
        viewer->createViewPort(0.5, 0.0, 1.0, 1.0, v2);
        viewer->setBackgroundColor (0.3, 0.3, 0.3, v2);
        viewer->addText("Radius: 0.1", 10, 10, "v2 text", v2);
        viewer->addPointCloud<PointT> (cloud2, "sample cloud2", v2);

        viewer->setPointCloudRenderingProperties (pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 3, "sample cloud1");
        viewer->setPointCloudRenderingProperties (pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 3, "sample cloud2");
        viewer->addCoordinateSystem (1.0);


        return (viewer);

    }

    boost::shared_ptr<pcl::visualization::PCLVisualizer> createDoubleVisualizerRGB (
            PointCloudRGB::ConstPtr cloud, PointCloudRGB::ConstPtr cloud2,  std::string title) {

        // --------------------------------------------------------
        // -----Open 3D viewer and add point cloud and normals-----
        // --------------------------------------------------------
        boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer (new pcl::visualization::PCLVisualizer (title));
        viewer->initCameraParameters ();

        int v1(0);
        viewer->createViewPort(0.0, 0.0, 0.5, 1.0, v1);
        viewer->setBackgroundColor (0.3, 0.3, 0.3, v1);
        viewer->addText("Radius: 0.01", 10, 10, "v1 text", v1);
        pcl::visualization::PointCloudColorHandlerRGBField<PointTRGB> rgb(cloud);
        viewer->addPointCloud<PointTRGB> (cloud, rgb, "sample cloud1", v1);

        int v2(0);
        viewer->createViewPort(0.5, 0.0, 1.0, 1.0, v2);
        viewer->setBackgroundColor (0.3, 0.3, 0.3, v2);
        viewer->addText("Radius: 0.1", 10, 10, "v2 text", v2);
        pcl::visualization::PointCloudColorHandlerRGBField<PointTRGB> rgb2(cloud2);
        viewer->addPointCloud<PointTRGB> (cloud2, rgb2, "sample cloud2", v2);

        viewer->setPointCloudRenderingProperties (pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 3, "sample cloud1");
        viewer->setPointCloudRenderingProperties (pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 3, "sample cloud2");
        viewer->addCoordinateSystem (1.0);


        return (viewer);

    }
}