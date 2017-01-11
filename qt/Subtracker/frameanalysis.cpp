
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/video/video.hpp>

#include <algorithm>

#include "frameanalysis.h"
#include "tabletracking.h"
#include "logging.h"
#include "coordinates.h"

using namespace std;
using namespace chrono;
using namespace cv;
using namespace xfeatures2d;

// Taken from http://stackoverflow.com/a/12336381/807307
// take number image type number (from cv::Mat.type()), get OpenCV's enum string.
string getImgType(int imgTypeInt)
{
    int numImgTypes = 35; // 7 base types, with five channel options each (none or C1, ..., C4)

    int enum_ints[] =       {CV_8U,  CV_8UC1,  CV_8UC2,  CV_8UC3,  CV_8UC4,
                             CV_8S,  CV_8SC1,  CV_8SC2,  CV_8SC3,  CV_8SC4,
                             CV_16U, CV_16UC1, CV_16UC2, CV_16UC3, CV_16UC4,
                             CV_16S, CV_16SC1, CV_16SC2, CV_16SC3, CV_16SC4,
                             CV_32S, CV_32SC1, CV_32SC2, CV_32SC3, CV_32SC4,
                             CV_32F, CV_32FC1, CV_32FC2, CV_32FC3, CV_32FC4,
                             CV_64F, CV_64FC1, CV_64FC2, CV_64FC3, CV_64FC4};

    string enum_strings[] = {"CV_8U",  "CV_8UC1",  "CV_8UC2",  "CV_8UC3",  "CV_8UC4",
                             "CV_8S",  "CV_8SC1",  "CV_8SC2",  "CV_8SC3",  "CV_8SC4",
                             "CV_16U", "CV_16UC1", "CV_16UC2", "CV_16UC3", "CV_16UC4",
                             "CV_16S", "CV_16SC1", "CV_16SC2", "CV_16SC3", "CV_16SC4",
                             "CV_32S", "CV_32SC1", "CV_32SC2", "CV_32SC3", "CV_32SC4",
                             "CV_32F", "CV_32FC1", "CV_32FC2", "CV_32FC3", "CV_32FC4",
                             "CV_64F", "CV_64FC1", "CV_64FC2", "CV_64FC3", "CV_64FC4"};

    for(int i=0; i<numImgTypes; i++)
    {
        if(imgTypeInt == enum_ints[i]) return enum_strings[i];
    }
    return "unknown image type";
}

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

void FrameAnalysis::check_table_inversion() {
    // This is not terribly optimized, but we do not care since this code should be executed rather rarely
    Mat float_frame;
    this->frame.convertTo(float_frame, CV_32FC3, 1.0/255.0);

    // Compute red mask
    vector< Point > red_corners = { this->frame_ctx.frame_corners[0],
                                      0.75 * this->frame_ctx.frame_corners[0] + 0.25 * this->frame_ctx.frame_corners[1],
                                      0.75 * this->frame_ctx.frame_corners[3] + 0.25 * this->frame_ctx.frame_corners[2],
                                      this->frame_ctx.frame_corners[3] };
    Mat red_mask = Mat::zeros(this->frame.rows, this->frame.cols, CV_32FC1);
    fillConvexPoly(red_mask, red_corners, 1.0f, LINE_8);

    // Compute blue mask
    vector< Point > blue_corners = { this->frame_ctx.frame_corners[2],
                                       0.75 * this->frame_ctx.frame_corners[2] + 0.25 * this->frame_ctx.frame_corners[3],
                                       0.75 * this->frame_ctx.frame_corners[1] + 0.25 * this->frame_ctx.frame_corners[0],
                                       this->frame_ctx.frame_corners[1] };
    Mat blue_mask = Mat::zeros(this->frame.rows, this->frame.cols, CV_32FC1);
    fillConvexPoly(blue_mask, blue_corners, 1.0f, LINE_8);

    // Compute redness
    Mat red_diff = float_frame - this->settings.objects_colors[0];
    float red_factor = -0.5f / (this->settings.objects_color_stddev[0] * this->settings.objects_color_stddev[0]);
    Matx< float, 1, 3 > red_t = { red_factor, red_factor, red_factor };
    Mat redness;
    transform(red_diff.mul(red_diff), redness, red_t);
    redness -= 3.0f/2.0f * log(2 * M_PI * this->settings.objects_color_stddev[0]);

    // Compute blueness
    Mat blue_diff = float_frame - this->settings.objects_colors[1];
    float blue_factor = -0.5f / (this->settings.objects_color_stddev[1] * this->settings.objects_color_stddev[1]);
    Matx< float, 1, 3 > blue_t = { blue_factor, blue_factor, blue_factor };
    Mat blueness;
    transform(blue_diff.mul(blue_diff), blueness, blue_t);
    blueness -= 3.0f/2.0f * log(2 * M_PI * this->settings.objects_color_stddev[1]);

    // Compute scores and decide what to do
    float red_on_red = sum(red_mask.mul(redness))[0];
    float blue_on_red = sum(red_mask.mul(blueness))[0];
    float red_on_blue = sum(blue_mask.mul(redness))[0];
    float blue_on_blue = sum(blue_mask.mul(blueness))[0];
    if (blue_on_red + red_on_blue > red_on_red + blue_on_blue) {
        this->frame_ctx.frame_corners = { this->frame_ctx.frame_corners[2],
                                          this->frame_ctx.frame_corners[3],
                                          this->frame_ctx.frame_corners[0],
                                          this->frame_ctx.frame_corners[1] };
    }
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

    // Detection via features
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

            // There is good possibility that the table is inverted, i.e., red and blue sides are switched;
            // since this would confuse later processing stages, we implement a basic color detector and switch sides if needed
            this->check_table_inversion();
        }
    }

    // Following via ECC maximization
    if (this->frame_ctx.have_fix) {
        Mat frame_grey;
        Mat ref_grey;
        cvtColor(this->frame, frame_grey, CV_BGR2GRAY);
        cvtColor(this->frame_ctx.ref_image, ref_grey, CV_BGR2GRAY);
        Mat homography = getPerspectiveTransform(this->settings.ref_corners,
                                                 this->frame_ctx.frame_corners);
        Mat float_homography;
        homography.convertTo(float_homography, CV_32F);
        //BOOST_LOG_TRIVIAL(debug) << "homography type: " << getImgType(float_homography.type());
        findTransformECC(ref_grey, frame_grey, float_homography, MOTION_HOMOGRAPHY, TermCriteria(TermCriteria::COUNT+TermCriteria::EPS, 10, 0.1), this->frame_ctx.ref_mask);
        perspectiveTransform(this->settings.ref_corners, this->frame_ctx.frame_corners, float_homography);
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
