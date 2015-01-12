#include "median.hpp"

MedianComputer::MedianComputer(int r, int c) {
    n_values = r * c * channels;
    median = Mat(r, c, CV_8UC(channels));
    for(int l=0; l<(256 >> shift); l++) {
        frames[l] = new uint32_t[n_values];
        last[l] = new uint32_t[n_values];
        for(int i=0; i<n_buckets; i++) {
            buckets[i][l] = new uint32_t[n_values];
        }
    }
    num_less = new uint32_t[n_values];
    num_frames = cur_bucket = 0;
}

MedianComputer::~MedianComputer() {
    for(int l=0; l<(256 >> shift); l++) {
        delete[] frames[l];
        delete[] last[l];
        for(int i=0; i<n_buckets; i++) {
            delete[] buckets[i][l];
        }
    }
    delete[] num_less;
}

void MedianComputer::update(Mat frame) {
    for(uint32_t i=0; i<n_values; i++) {
        uint8_t col = frame.data[i] >> shift;
        uint8_t med = median.data[i] >> shift;
        frames[col][i]++;
        if(col < med) num_less[i]++;
        if(num_less[i] > num_frames/2) {
            median.data[i] -= 1 << shift;
            num_less[i] -= frames[med - 1][i];
        } else if(num_less[i] + frames[med][i] < num_frames/2) {
            num_less[i] += frames[med][i];
            median.data[i] += 1 << shift;
        }
    }
    num_frames++;
    if(median_decay) {
        if(num_frames % bucket_size == 0) {
            if(num_frames >= bucket_size * n_buckets) num_frames -= bucket_size;
            uint32_t next_bucket = (cur_bucket+1) % n_buckets;
            for(uint32_t i=0; i<n_values; i++) {
                for(int l=0; l<(256 >> shift); l++) {
                    buckets[cur_bucket][l][i] = frames[l][i] - last[l][i];
                    frames[l][i] -= buckets[next_bucket][l][i];
                    last[l][i] = frames[l][i];
                }
                for(int l=0; l<(median.data[i] >> shift); l++) {
                    num_less[i] -= buckets[next_bucket][l][i];
                }
            }
            cur_bucket = next_bucket;
        }
    }
}

Mat MedianComputer::getMedian() {
    return median;
}
