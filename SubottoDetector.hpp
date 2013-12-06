#ifndef SUBOTTODETECTOR_H_
#define SUBOTTODETECTOR_H_

#include <opencv2/core/core.hpp>
#include <opencv2/videostab/videostab.hpp>

#include <memory>

struct SubottoInfo {
	cv::Mat frame;
	cv::Mat subottoTransform;
	cv::Mat subottoTransformInv;

	cv::Mat referenceImage;
	std::vector<cv::KeyPoint> referenceImageKeyPoints;
	std::vector<cv::KeyPoint> frameKeyPoints;
	std::vector<cv::DMatch> matches;
};

class SubottoDetector {
public:
	SubottoDetector(cv::VideoCapture cap, cv::Mat referenceImage, cv::Mat referenceImageMask);
	virtual ~SubottoDetector();

	std::unique_ptr<SubottoInfo> next();

private:
	cv::VideoCapture cap;

	std::unique_ptr<cv::FeatureDetector> featureDetector;
	std::unique_ptr<cv::DescriptorExtractor> descriptorExtractor;
	std::unique_ptr<cv::DescriptorMatcher> descriptorMatcher;

	cv::Mat referenceImage;
	cv::Mat referenceImageMask;
	cv::Mat referenceDescriptors;
	std::vector<cv::KeyPoint> referenceKeyPoints;

	bool hasPreviousFrame;
	cv::Mat previousFrame;
	cv::Mat previousSubottoTransform;

	cv::Mat estimateGlobalMotion(const cv::Mat& frame, std::vector<cv::KeyPoint>& frameKeyPoints, std::vector<cv::DMatch>& matches);
};

void drawSubottoBorders(cv::Mat& outImage, SubottoInfo const& info, cv::Scalar color);

#endif /* SUBOTTODETECTOR_H_ */
