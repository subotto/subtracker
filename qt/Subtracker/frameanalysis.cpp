
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/calib3d/calib3d.hpp>

#include <algorithm>

#include "frameanalysis.h"
#include "tabletracking.h"
#include "logging.h"
#include "coordinates.h"

using namespace std;
using namespace chrono;
using namespace cv;
using namespace xfeatures2d;

FrameAnalysis::FrameAnalysis(const cv::Mat &frame, int frame_num, const std::chrono::time_point< std::chrono::system_clock > &time, const std::chrono::time_point< std::chrono::system_clock > &acquisition_time, const std::chrono::time_point<steady_clock> &acquisition_steady_time, const FrameSettings &settings, const FrameCommands &commands, FrameContext &frame_ctx, ThreadContext &thread_ctx) :
    frame(frame), frame_num(frame_num), time(time), acquisition_time(acquisition_time), acquisition_steady_time(acquisition_steady_time), settings(settings), commands(commands), frame_ctx(frame_ctx), thread_ctx(thread_ctx) {

}

void FrameAnalysis::compute_objects_ll(int color) {
    Mat tmp;
    Mat diff = this->float_table_frame - this->settings.objects_colors[color];
    float factor = -0.5f / (this->settings.objects_color_stddev[color] * this->settings.objects_color_stddev[color]);
    Matx< float, 1, 3 > t = { factor, factor, factor };
    transform(diff.mul(diff), tmp, t);
    this->objects_ll[color] = tmp - 3.0f/2.0f * log(2 * M_PI * this->settings.objects_color_stddev[color]);
    this->objects_ll[color].convertTo(this->viz_objects_ll[color], CV_8UC1, 10.0, 255.0);
}

void FrameAnalysis::track_table()
{
    FrameWaiter waiter(frame_ctx.table_tracking_waiter, this->frame_num);
    if (this->commands.retrack_table) {
        this->frame_ctx.have_fix = false;
    }
    if (this->commands.regen_feature_detector || this->frame_ctx.surf_detector == NULL) {
        this->frame_ctx.surf_detector = SURF::create(this->settings.feats_hessian_threshold, this->settings.feats_n_octaves);
    }
    bool redetect_ref = false;
    if (this->commands.new_ref || (this->frame_ctx.ref_image.empty() && !this->settings.ref_image.empty())) {
        this->frame_ctx.ref_image = this->settings.ref_image;
        redetect_ref = true;
    }
    if (this->commands.new_mask || (this->frame_ctx.ref_mask.empty() && !this->settings.ref_mask.empty())) {
        cvtColor(this->settings.ref_mask, this->frame_ctx.ref_mask, cv::COLOR_RGB2GRAY);
        redetect_ref = true;
    }
    this->ref_image = this->frame_ctx.ref_image;
    this->ref_mask = this->frame_ctx.ref_mask;
    if (this->ref_image.empty() || this->ref_mask.empty()) {
        this->frame_ctx.ref_kps.clear();
        this->frame_ctx.ref_descr = Mat();
    } else if (redetect_ref) {
        this->frame_ctx.ref_kps.clear();
        this->frame_ctx.ref_descr = Mat();
        this->frame_ctx.surf_detector->detectAndCompute(this->ref_image, this->ref_mask, this->frame_ctx.ref_kps, this->frame_ctx.ref_descr);
    }
    if (!this->frame_ctx.have_fix && !this->frame_ctx.ref_kps.empty()) {
        // We have both sets of keypoints and we can try a matching
        vector< KeyPoint > frame_kps;
        Mat frame_descr;
        this->frame_ctx.surf_detector->detectAndCompute(this->frame, Mat(), frame_kps, frame_descr);
        //drawKeypoints(this->frame, frame_kps, this->frame_keypoints, Scalar::all(-1), DrawMatchesFlags::DEFAULT);
        BFMatcher matcher(cv::NORM_L2);
        vector< DMatch > matches;
        matcher.match(this->frame_ctx.ref_descr, frame_descr, matches);

        // Select good matches, i.e., those with distance no bigger than 3 times the minimum distance, and draw them
        double min_dist = min_element(matches.begin(), matches.end(), [](const DMatch &a, const DMatch &b)->bool{ return a.distance < b.distance; })->distance;
        double thresh_dist = 3 * min_dist;
        matches.erase(remove_if(matches.begin(), matches.end(), [&](const DMatch&a)->bool{ return a.distance > thresh_dist; }), matches.end());
        drawMatches(this->ref_image, this->frame_ctx.ref_kps, this->frame, frame_kps, matches, this->frame_ctx.frame_matches);

        // Run RANSAC to find the underlying homography and finally project known reference corners
        vector< Point2f > ref_points;
        vector< Point2f > frame_points;
        for (auto &match : matches) {
            ref_points.push_back(this->frame_ctx.ref_kps[match.queryIdx].pt);
            frame_points.push_back(frame_kps[match.trainIdx].pt);
        }
        Mat homography = findHomography(ref_points, frame_points, RANSAC, settings.ransac_threshold);

        // We expect the resuling matrix to be rather similar to the identity; it the determinant is too small we know that something has gone wrong and we reject the result
        // OpenCV docs guarantees that the matrix is already normalized with h_33 = 1
        float det = determinant(homography);
        //BOOST_LOG_TRIVIAL(info) << "Found homography with determinant " << det;
        if (det > settings.det_threshold) {
            perspectiveTransform(this->settings.ref_corners, this->frame_ctx.frame_corners, homography);
            this->frame_ctx.have_fix = true;
        }
    }
    this->frame_matches = this->frame_ctx.frame_matches;
}

void FrameAnalysis::do_things()
{
    (void) frame_ctx;
    (void) thread_ctx;

    this->begin_steady_time = steady_clock::now();
    this->begin_time = system_clock::now();

    this->track_table();

    // If we do not have a fix, there is nothing useful we can do with this frame
    if (this->frame_ctx.have_fix) {
        Size intermediate_size = compute_intermediate_size(settings);
        vector< Point2f > image_corners = { { 0.0, (float) intermediate_size.height },
                                     { (float) intermediate_size.width, (float) intermediate_size.height },
                                     { (float) intermediate_size.width, 0.0 },
                                     { 0.0, 0.0 } };
        Mat trans = getPerspectiveTransform(image_corners, this->frame_ctx.frame_corners);
        warpPerspective(this->frame, this->table_frame, trans, intermediate_size, INTER_NEAREST | WARP_INVERSE_MAP);

        this->table_frame_on_main = frame.clone();
        for (int i = 0; i < 4; i++) {
            line(this->table_frame_on_main, this->frame_ctx.frame_corners[i], this->frame_ctx.frame_corners[(i+1)%4], Scalar(0, 0, 255), 2);
        }

        this->table_frame.convertTo(this->float_table_frame, CV_32FC3, 1.0/255.0);

        for (int color = 0; color < 3; color++) {
            this->compute_objects_ll(color);
        }
    }

    this->end_steady_time = steady_clock::now();
    this->end_time = system_clock::now();
}

std::chrono::steady_clock::duration FrameAnalysis::total_processing_time() {
    return this->end_steady_time - this->begin_steady_time;
}

ThreadContext::ThreadContext() :
    tj_dec(tjInitDecompress())
{
}

ThreadContext::~ThreadContext()
{
    tjDestroy(this->tj_dec);
}
