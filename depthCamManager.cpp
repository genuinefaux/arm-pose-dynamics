/**
 * Author: Adam Mooers
 *
 * Implements the libraries found in depthCamManager.h.
 */

#include "depthCamManager.h"
#include <cstdio>

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
    sourceInMatForm.copyTo(modifiedSrc);
}

depth_cam::depth_cam( void )
{
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