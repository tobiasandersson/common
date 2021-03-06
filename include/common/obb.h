#ifndef COMMON_OBB_H
#define COMMON_OBB_H

#include <ros/ros.h>
#include <pcl/common/pca.h>
#include <pcl/common/common.h>
#include <common/types.h>

namespace common {

    class OrientedBoundingBox {
    public:

        typedef boost::shared_ptr<OrientedBoundingBox> Ptr;
        typedef boost::shared_ptr<const OrientedBoundingBox> ConstPtr;

        OrientedBoundingBox() {}

        OrientedBoundingBox(const Eigen::Vector3f t,
                            const Eigen::Quaternionf r,
                            float w, float h, float d)
            :translation(t)
            ,rotation(r)
            ,width(w)
            ,height(h)
            ,depth(d)
        {}

        /**
          * Creates bounding box around given pointcloud.
          * Adapted from
          * http://www.pcl-users.org/Finding-oriented-bounding-box-of-a-cloud-td4024616.html
          */
        OrientedBoundingBox(const SharedPointCloudRGB& cloud);

        const Eigen::Vector3f& get_translation() { return translation; }
        const Eigen::Quaternionf& get_rotation() { return rotation; }
        float get_width() { return width; }
        float get_height() { return height; }
        float get_depth() { return depth; }

        void serialize(std::vector<float>& target) const;
        static OrientedBoundingBox deserialize(const std::vector<float>& source);

    protected:
        Eigen::Vector3f translation;
        Eigen::Quaternionf rotation;
        float width, height, depth;
    };

    OrientedBoundingBox::OrientedBoundingBox(const SharedPointCloudRGB &cloud)
    {
        using namespace pcl;

        // compute principal direction
        Eigen::Vector4f centroid;
        pcl::compute3DCentroid(*cloud, centroid);

        Eigen::Matrix3f covariance;
        computeCovarianceMatrixNormalized(*cloud, centroid, covariance);
        Eigen::SelfAdjointEigenSolver<Eigen::Matrix3f> eigen_solver(covariance, Eigen::ComputeEigenvectors);
        Eigen::Matrix3f eigDx = eigen_solver.eigenvectors();
        eigDx.col(2) = eigDx.col(0).cross(eigDx.col(1));

        // move the points to the that reference frame
        Eigen::Matrix4f p2w(Eigen::Matrix4f::Identity());
        p2w.block<3,3>(0,0) = eigDx.transpose();
        p2w.block<3,1>(0,3) = -1.f * (p2w.block<3,3>(0,0) * centroid.head<3>());
        pcl::PointCloud<PointXYZRGB> cPoints;
        pcl::transformPointCloud(*cloud, cPoints, p2w);

        PointXYZRGB min_pt, max_pt;
        pcl::getMinMax3D(cPoints, min_pt, max_pt);
        const Eigen::Vector3f mean_diag = 0.5f*(max_pt.getVector3fMap() + min_pt.getVector3fMap());

        // final transform
        rotation = Eigen::Quaternionf(eigDx);
        translation = eigDx*mean_diag + centroid.head<3>();

        width = max_pt.x - min_pt.x;
        height = max_pt.y - min_pt.y;
        depth = max_pt.z - min_pt.z;
    }    

    void OrientedBoundingBox::serialize(std::vector<float>& target) const
    {
        target.reserve(3 + 4 + 3);

        target.push_back(translation(0));
        target.push_back(translation(1));
        target.push_back(translation(2));

        target.push_back(rotation.w());
        target.push_back(rotation.x());
        target.push_back(rotation.y());
        target.push_back(rotation.z());

        target.push_back(width);
        target.push_back(height);
        target.push_back(depth);
    }

    OrientedBoundingBox OrientedBoundingBox::deserialize(const std::vector<float> &source)
    {
        Eigen::Vector3f translation(source[0],source[1],source[2]);
        Eigen::Quaternionf rotation(source[3],source[4],source[5],source[6]);
        double width = source[7];
        double height = source[8];
        double depth = source[9];

        return OrientedBoundingBox(translation, rotation, width, height, depth);
    }

}

#endif // COMMON_OBB_H
