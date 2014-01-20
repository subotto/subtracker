#include "framereader.hpp"
#include <iostream>
using namespace std;

int main() {
    FrameReader f;
    namedWindow("Frame", WINDOW_NORMAL);
    while(waitKey(1) != 'e') {
        auto v = f.get();
//        cout << v.first.time_since_epoch().count() << endl;
        imshow("Frame", v.data);
//        this_thread::sleep_for(milliseconds(50));
    }
}
