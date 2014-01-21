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

    int zero = 0;
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
	cv::namedWindow(windowName);
	cv::createTrackbar(trackbarName, windowName, &zero, int((stop - start) / step), onTrackbarChange<T>, this);
	cv::setTrackbarPos(trackbarName, windowName, int((initial - start) / step));
}

template <typename T>
Trackbar<T>::~Trackbar() {
	cv::createTrackbar(trackbarName, windowName, &zero, int((stop - start) / step));
}

template <typename T>
T Trackbar<T>::get() {
	return start + cv::getTrackbarPos(trackbarName, windowName) * step;
}

void show(std::string name, cv::Mat image, int initialAlpha = 1000, int initialGamma = 0);

class ColorPicker {
public:
	ColorPicker(std::string name, cv::Scalar initial = cv::Scalar(0.f, 0.f, 0.f))
	:
		b(name, "blue", initial[0], 0.f, 1.f, 0.01f),
		g(name, "green", initial[1], 0.f, 1.f, 0.01f),
		r(name, "red", initial[2], 0.f, 1.f, 0.01f)
	{}

	cv::Scalar get() {
		return cv::Scalar(b.get(), g.get(), r.get());
	}
private:
	Trackbar<float> r, g, b;
};

class ColorQuadraticForm {
public:
	ColorQuadraticForm(std::string name, cv::Matx<float, 1, 6> initial = cv::Matx<float, 1, 6>(1.f, 1.f, 1.f, 0.f, 0.f, 0.f))
	:
		bb(name, "blue2",      initial.val[0],    0.f, 100.f, 0.01f),
		gg(name, "green2",     initial.val[1],    0.f, 100.f, 0.01f),
		rr(name, "red2",       initial.val[2],    0.f, 100.f, 0.01f),
		bg(name, "blue-green", initial.val[3], -100.f, 100.f, 0.01f),
		gr(name, "green-red",  initial.val[4], -100.f, 100.f, 0.01f),
		rb(name, "red-blue",   initial.val[5], -100.f, 100.f, 0.01f)
	{}

	cv::Matx<float, 1, 6> getScatterTransform() {
		return cv::Matx<float, 1, 6>(bb.get(), gg.get(), rr.get(), bg.get(), gr.get(), rb.get());
	}
private:
	Trackbar<float> bb, rr, gg, bg, gr, rb;
};

#endif /* UTILITY_HPP_ */
