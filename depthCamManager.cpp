/**
 * Author: Adam Mooers
 *
 * Implements the library found in depthCamManager.h.
 */

#include "depthCamManager.h"
#include <opencv2/imgproc/imgproc.hpp>
#include <iostream>
#include <list>

bool depth_cam::depth_cam_init() try
{
    if (ctx == nullptr)
    {
        ctx = new rs::context();
    }

    // Are any realsense devices connected?
    if(ctx->get_device_count() == 0) 
    {
        throw std::runtime_error( "No realsense devices are connected to the system at this time." );
    }

    dev = ctx->get_device(0);

    // Configure the input stream
    dev->enable_stream(rs::stream::depth, rs::preset::best_quality);

    return true;
}
catch(const rs::error & e)
{
    // Method calls against librealsense objects may throw exceptions of type rs::error
    printf("rs::error was thrown when calling %s(%s):\n", e.get_failed_function().c_str(), e.get_failed_args().c_str());
    printf("    %s\n", e.what());

    dev = nullptr;
    return false;
}

void depth_cam::start_stream( void )
{
    if (dev) {
        dev->start();
    }
}

void depth_cam::capture_next_frame( void )
{
    // Use polling to capture the next frame
    dev->wait_for_frames();

    // Update depth frame meta info
    depth_intrin = dev->get_stream_intrinsics(rs::stream::depth);

    // Retrieve a reference to the raw depth frame
    srcImg = (const uint16_t *)dev->get_frame_data(rs::stream::depth);

    // Convert to an OpenCV style matrix for consistency
    cv::Mat sourceInMatForm(depth_intrin.height, depth_intrin.width, CV_16UC1, (void *)srcImg);

    // Make a copy of the depth frame for editing
    sourceInMatForm.copyTo(cur_src);

    cv::resize(sourceInMatForm, cur_src, cv::Size(0, 0), scale_factor, scale_factor);

    // Old depth frame is no longer valid
    cloud.clear();
}

void depth_cam::to_depth_frame(void)
{
    float scale = dev->get_depth_scale();

    uint16_t* p;
    
    for( int i = 0; i < cur_src.rows; ++i)
    {
        p = cur_src.ptr<uint16_t>(i);
        for ( int j = 0; j < cur_src.cols; ++j)
        {
            if (p[j] != 0)  // For each non-zero cell
            {
                // Deproject depth pixel into 3D space and add it to the point cloud
                rs::float2 depth_pixel = {(float)j/scale_factor, (float)i/scale_factor};
                float depth_in_meters = p[j] * scale;
                rs::float3 depth_point = depth_intrin.deproject(depth_pixel, depth_in_meters);

                // Convert to opencv vector format first

                float point_arr[] = {depth_point.x, depth_point.y, depth_point.z};

                cv::Mat depth_point_cv(1, 3, CV_32FC1, &point_arr);

                cloud.add_point(depth_point_cv);
            }
        }
    }
}

void depth_cam::filter_background(float maxDist, int manhattan)
{
    // Info about the largest index
    int largest_ind = -1;
    int largest_ind_area = 0;

    int current_ind = 0;

    uint16_t* p;

    // Create a new mask for the image
    cv::Mat subject_mask;
    cur_src.copyTo(subject_mask);

    cv::Mat clustered = cv::Mat::zeros(cur_src.rows, cur_src.cols, CV_32SC1);

    for( int i = 0; i < subject_mask.rows; ++i)
    {
        p = subject_mask.ptr<uint16_t>(i);
        for ( int j = 0; j < subject_mask.cols; ++j)
        {
            if (p[j] != 0)  // For each non-zero pixel
            {
                int current_ind_area = img_BFS(j, i, current_ind, subject_mask, clustered, maxDist, manhattan);

                // Is the new cluster bigger
                if (current_ind_area > largest_ind_area)
                {
                    largest_ind_area = current_ind_area;
                    largest_ind = current_ind;
                }
                current_ind++;
            }
        }
    }

    // Remove background in the original depth source
    mask_by_cluster_id(clustered, largest_ind, cur_src);
}

int depth_cam::img_BFS( int x, int y, int cluster_id, cv::Mat& input_img, cv::Mat& cluster_img, float maxDist, int manhattan)
{
    float scale = dev->get_depth_scale();
    int cluster_area  = 1;

    std::list<cv::Vec3i> neighbors;

    uint16_t centerDepth = input_img.ptr<uint16_t>(y)[x];

    while (true) 
    {
        cluster_area++;

        // Iterate through all squares within the manhattan distance of the origin
        for (   int y_ind = std::max(-manhattan + y, 0); 
                y_ind <= std::min(manhattan + y, input_img.rows-1); 
                y_ind++ )
        {
            uint16_t* in_p = input_img.ptr<uint16_t>(y_ind);    // Pointer to the current row
            int32_t* clust_p = cluster_img.ptr<int32_t>(y_ind);

            // Calculate the horizontal manhattan distance
            int dxLim = std::abs(y_ind-y)-manhattan;

            // For each manhattan column
            for (   int x_ind = std::max(dxLim + x, 0); 
                    x_ind <= std::min(-dxLim + x, input_img.cols-1); 
                    x_ind++)
            {

                // Skip any out-of-range pixels
                if (in_p[x_ind] == 0 || std::abs(centerDepth-in_p[x_ind])*scale > maxDist )
                {
                    continue;
                }

                
                // Add the current pixel to the list
                neighbors.push_back({x_ind, y_ind, in_p[x_ind]});

                // Add current item to the visited list
                in_p[x_ind] = 0;

                // Set group of current pixel
                clust_p[x_ind] = cluster_id;
            }
        }

        // Get the next item to evaluate, while more items exist
        if (!neighbors.empty())
        {
            x = neighbors.front()[0];
            y = neighbors.front()[1];
            centerDepth = neighbors.front()[2];
            neighbors.pop_front();
        }
        else
        {
            break;
        }

    }

    return cluster_area;
}

int depth_cam::mask_by_cluster_id(cv::Mat& cluster_img, int32_t cluster_id, cv::Mat& output_img)
{
    int32_t* p_cl;
    uint16_t* p_out;

    for( int i = 0; i < cluster_img.rows; ++i)
    {
        p_cl = cluster_img.ptr<int32_t>(i);
        p_out = output_img.ptr<uint16_t>(i);
        for ( int j = 0; j < cluster_img.cols; ++j)
        {
            // Zero all values except those in the target cluster
            p_out[j] = (p_cl[j] == cluster_id) ? p_out[j] : 0;
        }
    }
}

depth_cam::depth_cam( float scale_factor )
{
    depth_cam::scale_factor = scale_factor;

    // Only display warnings (avoid verbosity)
    rs::log_to_console(rs::log_severity::warn);
}

depth_cam::~depth_cam( void )
{
    if (ctx != nullptr)
    {
        delete ctx;
    }
}