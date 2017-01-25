
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/video/video.hpp>

#include <algorithm>
#include <iomanip>

#include "frameanalysis.h"
#include "logging.h"
#include "coordinates.h"
#include "cv.h"

using namespace std;
using namespace chrono;
using namespace cv;
using namespace xfeatures2d;

FrameAnalysis::FrameAnalysis(const cv::Mat &frame, int frame_num, const std::chrono::time_point<FrameClock> &time, const std::chrono::time_point< std::chrono::system_clock > &acquisition_time, const std::chrono::time_point<steady_clock> &acquisition_steady_time, const FrameSettings &settings, const FrameCommands &commands, FrameContext &frame_ctx, ThreadContext &thread_ctx) :
    frame(frame), frame_num(frame_num), time(time), acquisition_time(acquisition_time), acquisition_steady_time(acquisition_steady_time), settings(settings), commands(commands), frame_ctx(frame_ctx), thread_ctx(thread_ctx) {
}

void FrameAnalysis::push_debug_frame(Mat &frame)
{
    this->debug.push_back(frame);
}

void FrameAnalysis::compute_table_ll() {
    // FIXME - Can this be expressed in terms of optimized lower level operations?
    // First addend is easy, but I do not know how to multiply components in the second
    this->table_ll = Mat(this->table_frame.size(), CV_32F);
    assert(this->table_ll.isContinuous());
    auto it = this->table_ll.begin< float >();
    auto it_end = this->table_ll.end< float >();
    auto it2 = this->table_frame_diff2.begin< Vec< float, 3 > >();
    auto it3 = this->table_frame_var.begin< Vec< float, 3 > >();
    for (; it != it_end; ++it) {
        //*it = -0.5 * ((*it2)[0] + (*it2)[1] + (*it2)[2]);
        *it = -0.5 * ((*it2)[0] / (*it3)[0] + (*it2)[1] / (*it3)[1] + (*it2)[2] / (*it3)[2]) - 0.5 * log(2 * M_PI * (*it3)[0] * (*it3)[1] * (*it3)[2]);
        ++it2;
        ++it3;
    }
    this->push_debug_frame(this->table_ll);
}

void FrameAnalysis::compute_objects_ll(int color) {
    Mat tmp;
    auto diff = this->float_table_frame - this->settings.objects_colors[color];
    float factor = -0.5f / (this->settings.objects_color_stddev[color] * this->settings.objects_color_stddev[color]);
    Matx< float, 1, 3 > t = { factor, factor, factor };
    transform(diff.mul(diff), tmp, t);
    this->objects_ll[color] = tmp - 3.0f/2.0f * log(2 * M_PI * this->settings.objects_color_stddev[color]);
}

// TODO - Implement rotation estimation
void FrameAnalysis::find_foosmen() {
    for (uint8_t rod = 0; rod < this->settings.rod_num; rod++) {
        FrameSettings::RodParams &params = this->settings.rod_configuration[rod];
        const Mat &ll = this->objects_ll[params.side];
        int x1 = floor(table_frame_rod_x(this->settings, this->intermediate_size, rod - 0.5 * this->settings.foosmen_strip_width));
        int x2 = ceil(table_frame_rod_x(this->settings, this->intermediate_size, rod + 0.5 * this->settings.foosmen_strip_width));
        x1 = max(x1, 0);
        x2 = min(x2, ll.cols-1);
        Mat strip = Mat(ll, Range::all(), Range(x1, x2+1)).clone();
        Mat reduced_strip;
        reduce(strip, reduced_strip, 1, REDUCE_AVG, -1);
        Mat blurred_strip;
        blur(reduced_strip, blurred_strip, Size(1, this->settings.foosmen_blur_size));
        double translation_len = convert_phys_to_table_frame_y(this->settings, this->intermediate_size, params.dist) - convert_phys_to_table_frame_y(this->settings, this->intermediate_size, 0.0);
        Mat replicated_strip = blurred_strip.clone();
        for (uint8_t i = 1; i < params.num; i++) {
            Mat translated_strip;
            Mat trans_mat = Mat(Matx< float, 2, 3 >(1.0, 0.0, 0.0, 0.0, 1.0, i*translation_len));
            warpAffine(blurred_strip, translated_strip, trans_mat, blurred_strip.size(), INTER_LINEAR | WARP_INVERSE_MAP, BORDER_CONSTANT, Scalar(numeric_limits< float >::lowest()));
            replicated_strip += translated_strip;
        }
        replicated_strip /= params.num;
        Point max_point;
        minMaxLoc(replicated_strip, NULL, NULL, NULL, &max_point);
        this->rods[rod].shift = convert_table_frame_to_phys_y(settings, this->intermediate_size, max_point.y) - foosman_y(this->settings, rod, 0);
        if (true && rod == 3) {
            this->push_debug_frame(strip);
            this->push_debug_frame(reduced_strip);
            this->push_debug_frame(blurred_strip);
            this->push_debug_frame(replicated_strip);
        }
    }
}

void FrameAnalysis::update_mean() {
    FrameWaiter waiter(frame_ctx.table_frame_waiter, this->frame_num);
    if (!this->frame_ctx.mean_started) {
        this->frame_ctx.table_frame_mean = Mat(this->intermediate_size, CV_32FC3, this->settings.initial_mean);
        this->frame_ctx.table_frame_var = Mat(this->intermediate_size, CV_32FC3, this->settings.initial_var);
        this->frame_ctx.mean_started = true;
    }

    // Compute a foosmen mask, so that we don't spoil the background with them
    Mat foosmen_mask = Mat(this->intermediate_size, CV_8U, Scalar(255));
    //Mat temp2 = this->table_frame.clone();
    double rect_length = 2.0 * (convert_phys_to_table_frame_x(this->settings, this->intermediate_size, this->settings.foosman_foot) - convert_phys_to_table_frame_x(this->settings, this->intermediate_size, 0.0));
    double rect_width = 2.0 * (convert_phys_to_table_frame_y(this->settings, this->intermediate_size, this->settings.foosman_width) - convert_phys_to_table_frame_y(this->settings, this->intermediate_size, 0.0));
    for (uint8_t rod = 0; rod < this->settings.rod_num; rod++) {
        FrameSettings::RodParams &params = this->settings.rod_configuration[rod];
        for (uint8_t fm = 0; fm < params.num; fm++) {
            Point fm_center = transform_point(foosman_coords(this->settings, rod, fm) + Point2f(0.0, this->rods[rod].shift), compute_physical_rectangle(this->settings), compute_table_frame_rectangle(this->intermediate_size));
            Point displ = Point(0.5 * rect_length, 0.5 * rect_width);
            rectangle(foosmen_mask, fm_center - displ, fm_center + displ, Scalar(0), CV_FILLED);
            //rectangle(temp2, fm_center - displ, fm_center + displ, Scalar(0.0, 0.0, 0.0), CV_FILLED);
        }
    }

    // Update the running average
    // FIXME - The accumulation coefficient should be expressed in terms of absolute time, not frame number
    accumulateWeighted(this->float_table_frame, this->frame_ctx.table_frame_mean, this->settings.accumul_coeff, foosmen_mask);
    auto diff = this->float_table_frame - this->frame_ctx.table_frame_mean;
    this->table_frame_diff2 = diff.mul(diff);
    accumulateWeighted(this->table_frame_diff2, this->frame_ctx.table_frame_var, this->settings.accumul_coeff, foosmen_mask);

    this->table_frame_mean = this->frame_ctx.table_frame_mean;
    this->table_frame_var = this->frame_ctx.table_frame_var;
    this->push_debug_frame(this->table_frame_mean);
    /*Mat temp = 10.0 * (foosmen_mask - 1.0);
    this->push_debug_frame(temp);
    this->push_debug_frame(temp2);*/
}

void FrameAnalysis::find_ball() {
    /*auto threshold_mask = this->objects_ll[color] <= this->settings.ball_threshold;
    this->objects_ll[color].setTo(Scalar(numeric_limits< float >::lowest()), threshold_mask);*/
    Mat thresholded_table_nll;
    threshold(-this->table_ll, thresholded_table_nll, this->settings.table_nll_threshold, 0.0, THRESH_TRUNC);
    Mat ball_ll = this->objects_ll[2] + thresholded_table_nll;
    //Mat ball_ll2 = ball_ll - this->objects_ll[0] - this->objects_ll[1];
    this->push_debug_frame(ball_ll);
    //this->push_debug_frame(ball_ll2);
    Mat blurred_ball_ll;
    blur(ball_ll, blurred_ball_ll, Size(this->settings.ball_blur_size, this->settings.ball_blur_size));
    this->push_debug_frame(blurred_ball_ll);
    auto spots = find_local_maxima(blurred_ball_ll, this->settings.maxima_radius, this->settings.maxima_radius, this->settings.maxima_count);
    for (const auto &spot : spots) {
        this->spots.push_back(make_pair(transform_point(spot.first, compute_table_frame_rectangle(this->intermediate_size), compute_physical_rectangle(this->settings)), spot.second));
    }

    /*Point maxPoint;
    minMaxLoc(blurred_ball_ll, NULL, NULL, NULL, &maxPoint);
    this->ball = transform_point(maxPoint, compute_table_frame_rectangle(this->intermediate_size), compute_physical_rectangle(this->settings));
    this->ball_is_present = true;*/
}

void FrameAnalysis::do_things()
{
    this->begin_steady_time = steady_clock::now();
    this->begin_time = system_clock::now();

    // Undistort the input frame (disabled)
    if (false) {
        Mat tmp = this->frame.clone();
        remap(tmp, this->frame, this->settings.calibration_map1, this->settings.calibration_map2, INTER_LINEAR);
    }

    this->track_table();

    // If we do not have a fix, there is nothing useful we can do with this frame
    if (this->frame_ctx.have_fix) {
        // Warp table frame
        this->intermediate_size = compute_intermediate_size(settings);
        Mat homography = getPerspectiveTransform(compute_table_frame_rectangle(this->intermediate_size), compute_frame_rectangle(this->frame_corners));
        warpPerspective(this->frame, this->table_frame, homography, this->intermediate_size, INTER_LINEAR | WARP_INVERSE_MAP);
        this->table_frame.convertTo(this->float_table_frame, CV_32FC3, 1.0/255.0);

        for (int color = 0; color < 3; color++) {
            this->compute_objects_ll(color);
        }
        this->find_foosmen();
        this->update_mean();
        this->compute_table_ll();
        this->find_ball();

        // Draw rendering
        this->frame.copyTo(this->frame_rendering);
        this->table_frame.copyTo(this->table_frame_rendering);
        for (int i = 0; i < 4; i++) {
            line(this->frame_rendering, this->frame_corners[i], this->frame_corners[(i+1)%4], Scalar(0, 0, 255), 2);
        }
        for (uint8_t rod = 0; rod < this->settings.rod_num; rod++) {
            auto coords = transform_pair(rod_coords(this->settings, rod), compute_physical_rectangle(this->settings), compute_table_frame_rectangle(intermediate_size));
            line(this->table_frame_rendering, coords.first, coords.second, Scalar(0, 0, 255), 1);
            coords = transform_pair(rod_coords(this->settings, rod), compute_physical_rectangle(this->settings), compute_frame_rectangle(this->frame_corners));
            line(this->frame_rendering, coords.first, coords.second, Scalar(0, 0, 255), 1);
            for (uint8_t fm = 0; fm < this->settings.rod_configuration[rod].num; fm++) {
                auto orig_point = foosman_coords(this->settings, rod, fm);
                orig_point += Point2f(0.0, this->rods[rod].shift);
                auto point = transform_point(orig_point, compute_physical_rectangle(this->settings), compute_table_frame_rectangle(intermediate_size));
                circle(this->table_frame_rendering, point, 3, Scalar(255, 0, 0), -1);
                point = transform_point(orig_point, compute_physical_rectangle(this->settings), compute_frame_rectangle(this->frame_corners));
                circle(this->frame_rendering, point, 3, Scalar(255, 0, 0), -1);
            }
        }
        for (const auto &x : this->spots) {
            const Point2f &p = x.first;
            auto point = transform_point(p, compute_physical_rectangle(this->settings), compute_table_frame_rectangle(intermediate_size));
            circle(this->table_frame_rendering, point, 3, Scalar(0, 255, 0), -1);
            point = transform_point(p, compute_physical_rectangle(this->settings), compute_frame_rectangle(this->frame_corners));
            circle(this->frame_rendering, point, 3, Scalar(0, 255, 0), -1);
        }
        this->push_debug_frame(this->frame_rendering);
        this->push_debug_frame(this->table_frame_rendering);
    }

    this->end_steady_time = steady_clock::now();
    this->end_time = system_clock::now();
}

// For historical reasons, the actual CSV format we use is pretty funny; here we make the necessary conversions
string FrameAnalysis::gen_csv_line() const
{
    stringstream buf;
    buf << setiosflags(ios::fixed) << setprecision(5);

    buf << this->time.to_double();
    if (this->ball_is_present) {
        buf << "," << this->ball.x << "," << this->ball.y;
    } else {
        buf << ",,";
    }
    for (uint8_t team = 0; team < FrameSettings::team_num; team++) {
        for (uint8_t num = 0; num < FrameSettings::rod_per_team; num++) {
            const RodPos &rod = this->rods[this->settings.resolve_rod(team, team == 0 ? num : 3 - num)];
            buf << "," << rod.shift << "," << rod.rot;
        }
    }
    return buf.str();
}

bool FrameAnalysis::does_have_fix() const
{
    return this->have_fix;
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
