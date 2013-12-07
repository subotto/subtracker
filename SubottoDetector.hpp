#ifndef SUBOTTODETECTOR_H_
#define SUBOTTODETECTOR_H_

#include <opencv2/core/core.hpp>
#include <opencv2/videostab/videostab.hpp>

#include <memory>

struct SubottoInfo {
	cv::Mat frame;
	cv::Mat subottoTransform;
};

struct SubottoDetectorParams {
	int preliminaryReferenceLevels = 3;
	int preliminaryReferenceFeatures = 50;
	int preliminaryFrameLevels = 3;
	int preliminaryFrameFeatures = 500;

	int preliminaryRansacThreshold = 1000;
	int preliminaryRansacOutliersRatio = 60;

	int secondaryReferenceLevels = 1;
	int secondaryReferenceFeatures = 200;
	int secondaryFrameLevels = 1;
	int secondaryFrameFeatures = 500;

	int secondaryRansacThreshold = 200;

	int flowReferenceFeatures = 100;

	int stabReferenceFeatures = 100;

	int ransacReprojThreshold = 300;
};

class SubottoDetector {
public:
	SubottoDetector(cv::VideoCapture cap, cv::Mat referenceImage, cv::Mat referenceImageMask, std::shared_ptr<SubottoDetectorParams> params);
	virtual ~SubottoDetector();

	std::unique_ptr<SubottoInfo> next();

private:
	cv::VideoCapture cap;

	cv::Mat referenceImage;
	cv::Mat referenceImageMask;

	std::shared_ptr<SubottoDetectorParams> params;

	bool hasPreviousFrame;
	cv::Mat previousFrame;
	cv::Mat previousTransform;

	cv::Mat estimateGlobalMotion(const cv::Mat& frame, std::vector<cv::KeyPoint>& frameKeyPoints, std::vector<cv::DMatch>& matches);
	cv::Mat changeCoordinateSystem(const cv::Mat& estimatedTransform);

	cv::Mat computePreliminaryTransform(const cv::Mat& frame);
	cv::Mat computeSecondaryCorrection(const cv::Mat& frame);
	cv::Mat computeOpticalFlowCorrection(const cv::Mat& frame);

	cv::Mat stabilize(const cv::Mat& frame, const cv::Mat& estimatedTransform);
	cv::Mat applySecondaryCorrection(const cv::Mat& preliminaryTransform, const cv::Mat& frame);
	cv::Mat applyOpticalFlowCorrection(const cv::Mat& secondaryTransform, const cv::Mat& frame);
};

void drawSubottoBorders(cv::Mat& outImage, SubottoInfo const& info, cv::Scalar color);

#endif /* SUBOTTODETECTOR_H_ */
