
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

void FrameAnalysis::compute_objects_ll(int color) {
    Mat tmp;
    Mat diff = this->float_table_frame - this->settings.objects_colors[color];
    float factor = -0.5f / (this->settings.objects_color_stddev[color] * this->settings.objects_color_stddev[color]);
    Matx< float, 1, 3 > t = { factor, factor, factor };
    transform(diff.mul(diff), tmp, t);
    this->objects_ll[color] = tmp - 3.0f/2.0f * log(2 * M_PI * this->settings.objects_color_stddev[color]);
    //this->objects_ll[color].convertTo(this->viz_objects_ll[color], CV_8UC1, 10.0, 255.0);
}

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
            // FIXME - Use infinity, so we do not depend on another constant
            //static_assert(numeric_limits< float >::has_infinity);
            //warpAffine(blurred_strip, translated_strip, trans_mat, blurred_strip.size(), INTER_LINEAR | WARP_INVERSE_MAP, BORDER_CONSTANT, Scalar(numeric_limits< float >::infinity()));
            warpAffine(blurred_strip, translated_strip, trans_mat, blurred_strip.size(), INTER_LINEAR | WARP_INVERSE_MAP, BORDER_CONSTANT, Scalar(-1000000.0));
            replicated_strip += translated_strip;
        }
        replicated_strip /= params.num;
        Point max_point;
        minMaxLoc(replicated_strip, NULL, NULL, NULL, &max_point);
        this->rods[rod].shift = convert_table_frame_to_phys_y(settings, this->intermediate_size, max_point.y) - foosman_y(this->settings, rod, 0);
        if (rod == 3) {
            this->push_debug_frame(strip);
            this->push_debug_frame(reduced_strip);
            this->push_debug_frame(blurred_strip);
            this->push_debug_frame(replicated_strip);
        }
    }
}

void FrameAnalysis::do_things()
{
    this->begin_steady_time = steady_clock::now();
    this->begin_time = system_clock::now();

    this->track_table();

    // If we do not have a fix, there is nothing useful we can do with this frame
    if (this->frame_ctx.have_fix) {
        // Warp table frame
        this->intermediate_size = compute_intermediate_size(settings);
        Mat homography = getPerspectiveTransform(compute_table_frame_rectangle(this->intermediate_size), compute_frame_rectangle(this->frame_ctx.frame_corners));
        warpPerspective(this->frame, this->table_frame, homography, this->intermediate_size, INTER_LINEAR | WARP_INVERSE_MAP);
        this->table_frame.convertTo(this->float_table_frame, CV_32FC3, 1.0/255.0);

        // Compute likelihoods
        for (int color = 0; color < 3; color++) {
            this->compute_objects_ll(color);
        }

        this->find_foosmen();

        // Draw rendering
        this->frame.copyTo(this->frame_rendering);
        this->table_frame.copyTo(this->table_frame_rendering);
        for (int i = 0; i < 4; i++) {
            line(this->frame_rendering, this->frame_ctx.frame_corners[i], this->frame_ctx.frame_corners[(i+1)%4], Scalar(0, 0, 255), 2);
        }
        for (uint8_t rod = 0; rod < this->settings.rod_num; rod++) {
            auto coords = transform_pair(rod_coords(this->settings, rod), compute_physical_rectangle(this->settings), compute_table_frame_rectangle(intermediate_size));
            line(this->table_frame_rendering, coords.first, coords.second, Scalar(0, 0, 255), 1);
            coords = transform_pair(rod_coords(this->settings, rod), compute_physical_rectangle(this->settings), compute_frame_rectangle(this->frame_ctx.frame_corners));
            line(this->frame_rendering, coords.first, coords.second, Scalar(0, 0, 255), 1);
            for (uint8_t fm = 0; fm < this->settings.rod_configuration[rod].num; fm++) {
                auto orig_point = foosman_coords(this->settings, rod, fm);
                orig_point += Point2f(0.0, this->rods[rod].shift);
                auto point = transform_point(orig_point, compute_physical_rectangle(this->settings), compute_table_frame_rectangle(intermediate_size));
                circle(this->table_frame_rendering, point, 3, Scalar(255, 0, 0), -1);
                point = transform_point(orig_point, compute_physical_rectangle(this->settings), compute_frame_rectangle(this->frame_ctx.frame_corners));
                circle(this->frame_rendering, point, 3, Scalar(255, 0, 0), -1);
            }
        }
        if (this->ball_is_present) {
            auto point = transform_point(this->ball, compute_physical_rectangle(this->settings), compute_table_frame_rectangle(intermediate_size));
            circle(this->table_frame_rendering, point, 3, Scalar(0, 255, 0), -1);
            point = transform_point(this->ball, compute_physical_rectangle(this->settings), compute_frame_rectangle(this->frame_ctx.frame_corners));
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
