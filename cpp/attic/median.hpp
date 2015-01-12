#include <opencv2/highgui/highgui.hpp>
#include <opencv2/video/background_segm.hpp>
using namespace cv;


class MedianComputer {
private:
    static const uint32_t channels = 3;
    static const uint32_t shift = 2;
    static const uint32_t n_buckets = 4;
    // Dovrebbe essere più veloce se è una potenza di 2
    static const uint32_t bucket_size = 1024;
    static const bool median_decay = true;
    uint32_t n_values;
    uint32_t *frames[256 >> shift];
    uint32_t *last[256 >> shift];
    uint32_t *buckets[n_buckets][256 >> shift];
    uint32_t *num_less;
    uint32_t num_frames;
    uint32_t cur_bucket;
    Mat median;
public:
    MedianComputer(int, int);
    ~MedianComputer();
    void update(Mat);
    Mat getMedian();
};
