#ifndef UTILITY_HPP_
#define UTILITY_HPP_

#include <sstream>
#include <string>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

template <typename T>
class Trackbar {
public:
	Trackbar(std::string category, std::string name, T initial = 0, T start = 0, T end = 100, T step = 1);
	virtual ~Trackbar();

	T get();

	virtual void onChange() {}
protected:
	std::string windowName, trackbarName;

	T start;
	T stop;
	T step;
};

template<typename T>
static void onTrackbarChange(int pos, void* userData) {
	Trackbar<T>* trackbar = static_cast<Trackbar<T>*>(userData);
	trackbar->onChange();
}

template <typename T>
Trackbar<T>::Trackbar(std::string category, std::string name, T initial, T start, T stop, T step)
	: start(start), stop(stop), step(step), windowName(category) {
	std::stringstream s;
	s << name << " " << start << "-" << stop;
	trackbarName = s.str();
	cv::namedWindow(windowName, CV_WINDOW_NORMAL);
	cv::createTrackbar(trackbarName, windowName, nullptr, int((stop - start) / step), onTrackbarChange<T>, this);
	cv::setTrackbarPos(trackbarName, windowName, int((initial - start) / step));
}

template <typename T>
Trackbar<T>::~Trackbar() {
	cv::createTrackbar(trackbarName, windowName, nullptr, int((stop - start) / step));
}

template <typename T>
T Trackbar<T>::get() {
	return start + cv::getTrackbarPos(trackbarName, windowName) * step;
}

void show(std::string name, cv::Mat image, int initialAlpha = 1000, int initialGamma = 0);

#endif /* UTILITY_HPP_ */
