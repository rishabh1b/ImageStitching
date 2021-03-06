#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/nonfree/nonfree.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <string>
#include <sstream>
#include <iostream>
#include <stdlib.h>
#include <vector>
#include <algorithm>
#include <math.h>
// #include "Eigen/SVD"

using namespace cv;

static const std::string MATCHES_WINDOW = "Matches found";
#define MIN_MATCH_COUNT 20
#define THRESH_HOMO 10
#define PADDING 30

/**
 * @brief Function distance transform of the input by setting 0 values at the border
 * @details Distance to the borders using chessboard metric is returned
 * 
 * @param img Mat object for which distance transform is to be evaluated
 * @param img_dist Mat object on which distance transform results will be written
 */
void distance2Border(const Mat& img, Mat& img_dist) {
	// --Calculate the Distance to the borders for blending Operation
	Mat img_gray;

	if (img.channels() > 1)
		cvtColor(img, img_gray, CV_BGR2GRAY);
	else
		img_gray = img.clone();


	img_dist.setTo(255, img_gray != 0);

    img_dist.col(0).setTo(Scalar(0));
	img_dist.col(img_dist.cols - 1).setTo(Scalar(0));
	img_dist.row(0).setTo(Scalar(0));
	img_dist.row(img_dist.rows - 1).setTo(Scalar(0));

	distanceTransform(img_dist, img_dist, CV_DIST_C, 3); 
}

/**
 * @brief Function calculates homography using Direct Linear Transformation
 * @details Homography is calculated using SVD and using four candidates
 * 
 * @param new_candidates Points on the image to be transformed
 * @param base_pts_candidates Points on the base image
 * @param H_curr Homography matrix which will be returned from this function
 */
void getHomography(std::vector<Point2f> base_pts_candidates, std::vector<Point2f> new_candidates, Mat& H_curr){
	// -- Get Homography by solving Direct Linear Transformation
	Mat A = Mat::zeros(base_pts_candidates.size() * 2, 9, CV_64F);
	size_t j = 0;

	// -- Lamba function for constructing elements of the A matrix 
	auto constructA = [](const Point2f& base_pts_pt, const Point2f& new_pt, Mat& ax, Mat& ay){
		ax = (Mat_<double>(1,9) << -base_pts_pt.x, -base_pts_pt.y, -1, 0, 0, 0, (base_pts_pt.x * new_pt.x), (new_pt.x * base_pts_pt.y), new_pt.x);
		ay = (Mat_<double>(1,9) << 0, 0, 0, -base_pts_pt.x, -base_pts_pt.y, -1, (base_pts_pt.x * new_pt.y), (base_pts_pt.y * new_pt.y), new_pt.y);

	};

	for(size_t i = 0; i < base_pts_candidates.size(); i++) {
		Mat ax, ay;
		constructA(base_pts_candidates[i], new_candidates[i], ax, ay);
		ax.copyTo(A.row(j++));
		// A.row(j) = ay;
		ay.copyTo(A.row(j++));
	}

	Mat h, w, u, vt;
	SVD::compute(A, w, u, vt, SVD::FULL_UV); // FULL_UV flag is important
	h = vt.row(8);
	int count = 0;	

	for (size_t i = 0; i < 3; i++) {
		for (size_t k = 0; k < 3; k++) {
			H_curr.at<double>(i,k) = h.at<double>(count);
			++count;
		}
	}

	// Eigen Attempt at SVD - Possibly to speed up computation - gave the same result
	// Eigen::MatrixXd m(8,9);
	// m << A.at<double>(0,0), A.at<double>(0,1), A.at<double>(0,2), A.at<double>(0,3), A.at<double>(0,4), A.at<double>(0,5), A.at<double>(0,6), A.at<double>(0,7), A.at<double>(0,8),
	// 	 A.at<double>(1,0), A.at<double>(1,1), A.at<double>(1,2), A.at<double>(1,3), A.at<double>(1,4), A.at<double>(1,5), A.at<double>(1,6), A.at<double>(1,7), A.at<double>(1,8),
	// 	 A.at<double>(2,0), A.at<double>(2,1), A.at<double>(2,2), A.at<double>(2,3), A.at<double>(2,4), A.at<double>(2,5), A.at<double>(2,6), A.at<double>(2,7), A.at<double>(2,8),
	// 	 A.at<double>(3,0), A.at<double>(3,1), A.at<double>(3,2), A.at<double>(3,3), A.at<double>(3,4), A.at<double>(3,5), A.at<double>(3,6), A.at<double>(3,7), A.at<double>(3,8),
	// 	 A.at<double>(4,0), A.at<double>(4,1), A.at<double>(4,2), A.at<double>(4,3), A.at<double>(4,4), A.at<double>(4,5), A.at<double>(4,6), A.at<double>(4,7), A.at<double>(4,8),
	// 	 A.at<double>(5,0), A.at<double>(5,1), A.at<double>(5,2), A.at<double>(5,3), A.at<double>(5,4), A.at<double>(5,5), A.at<double>(5,6), A.at<double>(5,7), A.at<double>(5,8),
	// 	 A.at<double>(6,0), A.at<double>(6,1), A.at<double>(6,2), A.at<double>(6,3), A.at<double>(6,4), A.at<double>(6,5), A.at<double>(6,6), A.at<double>(6,7), A.at<double>(6,8),
	// 	 A.at<double>(7,0), A.at<double>(7,1), A.at<double>(7,2), A.at<double>(7,3), A.at<double>(7,4), A.at<double>(7,5), A.at<double>(7,6), A.at<double>(7,7), A.at<double>(7,8);

	// Eigen::JacobiSVD<Eigen::MatrixXd> svd(m, Eigen::ComputeThinU | Eigen::ComputeThinV);
	// std::cout << "Its right singular vectors are the columns of the thin V matrix:" << std::endl << svd.matrixV() << std::endl;

}
/**
 * @brief Function Calculates Homography using RANSAC
 * @details Homography is calculated using RANSAC for outliers rejection
 * @param next_image_pts Matched Points in the image to be transformed
 * @param base_pts Matched Points in the base image where points will be transformed 
 * @param H [description]
 */
void getHomographyRANSAC(std::vector<Point2f> base_pts, std::vector<Point2f> next_image_pts, Mat& H) {
	std::vector<int> random_numbers;
	std::vector<Point2f> chosen_candidates_b;
	std::vector<Point2f> chosen_candidates_n;

    size_t sz = base_pts.size();
	int rnd = rand() % sz;
	int max_inliers = 0, inliers = 0;

	Mat H_t = Mat::zeros(3,3, CV_64F);

	auto ssd = [](const Mat& vect_t, const Mat& vec) -> double{
		Mat squared_diff;
		pow((vect_t - vec), 2, squared_diff);
		return sum(squared_diff).val[0];
	};

    // -- Perform RANSAC for 1000 iterations
    for (size_t i = 0; i < 1000; i++) {
	//-- Choose Some Random Values
	while(random_numbers.size() < 4) {
		while (std::find(random_numbers.begin(), random_numbers.end(), rnd) != random_numbers.end()) {
			rnd = rand() % sz;
		}
		random_numbers.push_back(rnd);
	}

	// -- Get the Points For Estimating Homography
	for (auto rnd_num : random_numbers) {
		chosen_candidates_b.emplace_back(Point2f(base_pts[rnd_num].x, base_pts[rnd_num].y));
		chosen_candidates_n.emplace_back(Point2f(next_image_pts[rnd_num].x, next_image_pts[rnd_num].y));
	}

	getHomography(chosen_candidates_n, chosen_candidates_b, H_t);

	// std::cout << "RANSAC iteration" << std::endl;
	for (size_t j = 0; j < sz; j++) {
		Mat vec = (Mat_<double>(3,1) << next_image_pts[j].x, next_image_pts[j].y, 1);
		vec = H_t * vec;
		vec = vec / vec.at<double>(2,0);

        Mat vec_t = (Mat_<double>(3,1) << base_pts[j].x, base_pts[j].y, 1);

        if (ssd(vec_t, vec) < THRESH_HOMO)
        	++inliers;
	}


	// std::cout << "The number of Inliers are" << inliers << std::endl;
	if (inliers > max_inliers) {
		max_inliers = inliers;
		H = H_t.clone();
	}

    random_numbers.clear();
    chosen_candidates_b.clear();
    chosen_candidates_n.clear();
    inliers = 0;
	}

}

/**
 * @brief Get Matching 2D points from two images
 * @details Feature Points are extracted using SIFT features
 * 
 * @param image_mid Base Image
 * @param image_next Next Image
 * @param next_image_pts Image points that are matched in the next image
 * @param base_pts Image Points that are matched in the base image
 * @param showMatches Optional Boolean for showing matches
 */
void getMatchingPoints(const Mat& image_mid, const Mat& image_next, std::vector<Point2f>& base_pts, std::vector<Point2f>& next_image_pts, bool showMatches = false) {
	// Get Matching Features
    std::vector<KeyPoint> keypoints_b, keypoints_i;
	SiftDescriptorExtractor extractor;

	//-- Step 1: Detect the keypoints in the scene using SIFT Detector
	SiftFeatureDetector detector;
	detector.detect( image_mid, keypoints_b);
	detector.detect( image_next, keypoints_i);

	//-- Step 2: Calculate descriptors (feature vectors)
	Mat descriptors_b, descriptors_i;
	extractor.compute( image_mid, keypoints_b, descriptors_b );
	extractor.compute( image_next, keypoints_i, descriptors_i );

	//-- Step 3: Matching descriptor vectors using FLANN matcher
  	FlannBasedMatcher matcher;
  	std::vector< vector<DMatch> > matches;
	matcher.knnMatch(descriptors_b,descriptors_i, matches,2);

	//-- Draw only "good" matches (i.e. the ones in accordance to the Lowe's Ratio Test )
  	std::vector< DMatch > good_matches;

  	for(size_t i = 0; i < matches.size(); i++)
  	{
    if (matches[i].size() == 2 && (matches[i][0].distance < 0.7 * matches[i][1].distance))
        good_matches.push_back(matches[i][0]);

	}
	//-- Confirm whether the size of the matching pairs is greater than threshold
    if (good_matches.size() > MIN_MATCH_COUNT)
	{
		std::cout << "Success!" << std::endl;
	}	

    // Show Matches
    if (showMatches) {
	    Mat img_matches;

	    drawMatches(image_mid, keypoints_b, image_next, keypoints_i, good_matches, img_matches, Scalar::all(-1), Scalar::all(-1), vector<char>(), DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS);

	    // namedWindow("Display Image", WINDOW_AUTOSIZE );
	    // imshow("Display Image", image_mid);	

	    imshow(MATCHES_WINDOW, img_matches);
	}

     for(size_t i = 0;i < good_matches.size();i++){
	//-- Get the keypoints from the good matches
		base_pts.push_back(keypoints_b[good_matches[i].queryIdx].pt);
		next_image_pts.push_back(keypoints_i[good_matches[i].trainIdx].pt);
	}
}
/**
 * @brief Perform Image Concatenation
 * 
 * @param image_mid Base Image for Concatenation
 * @param image_next Next Image for Concatentation
 */
void concatenate( Mat& image_mid, Mat& image_next) {

    Mat image_mid_d, image_next_d;

    image_mid.convertTo(image_mid_d, CV_64FC3);
    image_next.convertTo(image_next_d, CV_64FC3);

    std::vector<Point2f> base_pts;
    std::vector<Point2f> next_image_pts;

    getMatchingPoints(image_mid, image_next, base_pts, next_image_pts); 

    //-- Calculate the Homography and the corresponding inliers
	Mat H  = Mat::eye(3,3, CV_64F);
	getHomographyRANSAC(base_pts, next_image_pts, H);

    // std::cout << "Got homography : " << std::endl;
    // std::cout << H << std::endl;

    // Comparison with existing Homography function 
    // Mat H2 = findHomography(next_image_pts, base_pts, CV_RANSAC, 10);
    // std::cout << "Homography inbuilt: " << H2 << std::endl;

    // std::vector<Point2f> obj_corners(4);
    // obj_corners[0] = cvPoint(0, 0);
    // obj_corners[1] = cvPoint(image_next.cols, 0);
    // obj_corners[2] = cvPoint(image_next.cols, image_next.rows);
    // obj_corners[3] = cvPoint(0, image_next.rows);
    // std::vector<Point2f> scene_corners(4);
	// perspectiveTransform(obj_corners, scene_corners, H2);
	// std::cout << "scene Corners: " << scene_corners << std::endl;

    // -- Get the Distances to the Borders For Blending - Will not be used for final implementation
    Mat img_dist_b(image_mid.rows, image_mid.cols, CV_8U, Scalar::all(0));
    Mat img_dist_i(image_next.rows, image_next.cols, CV_8U, Scalar::all(0));

    distance2Border(image_next, img_dist_i);
    distance2Border(image_mid, img_dist_b);

    // Get the Extremes of the Transformed Image
    Mat1d ul = (Mat1d(3,1) << 0,0,1);
    Mat1d ur = (Mat1d(3,1) << image_next.cols - 1,0,1);
    Mat1d bl = (Mat1d(3,1) << 0,image_next.rows - 1,1);
    Mat1d br = (Mat1d(3,1) << image_next.cols - 1, image_next.rows - 1, 1);

    ul = H * ul;
    ul = ul / ul.at<double>(2,0);

    ur = H * ur;
    ur = ur / ur.at<double>(2,0);

    bl = H * bl;
    bl = bl / bl.at<double>(2,0);

    br = H * br;
    br = br / br.at<double>(2,0);

    // -- Apply Necessary Padding to support concatenation
    int pad_right = 0, pad_down = 0, pad_left = 0, pad_up = 0;

    if (std::max(br(0),ur(0)) > image_mid.cols)
		pad_right = int(std::max(br(0),ur(0))) - image_mid.cols + PADDING;

    if (std::max(br(1), bl(1)) > image_mid.rows)
    	pad_down = int(std::max(bl(1),br(1))) - image_mid.rows + PADDING;

    if (std::min(ul(0), bl(0)) <= 0)
    	pad_left = -int(std::min(bl(0),ul(0))) + PADDING;

    if (std::min(ul(1), ur(1)) <= 0)
		pad_up = -int(std::min(ul(1),ur(1))) + PADDING;


    int new_cols = image_mid.cols + pad_left + pad_right;
    int new_rows = image_mid.rows + pad_up + pad_down;

    Mat img_mid_new = Mat::zeros(new_rows, new_cols, CV_64FC3);
    image_mid_d.copyTo(img_mid_new(Rect(pad_left, pad_up, image_mid.cols, image_mid.rows)));

    // -- Extract Points from the base_pts Image for inverse Homography computation
    // --  A parallel to meshgrid in MATLAB is written
    std::vector<int> range_y, range_x;

    int start_y = int(pad_up + std::min(ul(1), ur(1)));
    int end_y = int(pad_up + std::max(bl(1), br(1))); 
    for (size_t i = start_y; i < end_y; i++) {
    	range_y.push_back(i);
    }

    int start_x = int(pad_left + std::min(ul(0), bl(0)));
    int end_x = int(pad_left + std::max(br(0),ur(0)));
    for (size_t i = start_x; i < end_x; i++) {
    	range_x.push_back(i);
    }

    Mat1i temp_y(range_y);
    Mat1i temp_x(range_x);
    Mat1i X_b, Y_b;

    repeat(temp_y.reshape(1,1).t(), 1, temp_x.total(), Y_b);
    repeat(temp_x.reshape(1,1), temp_y.total(), 1, X_b);

    X_b = X_b.reshape(1,X_b.total());
    Y_b = Y_b.reshape(1,Y_b.total());


    // -- Form Matrix for Inverse Homography Computation
   	Mat o = Mat::ones(X_b.rows,1, CV_32S);
   	Mat Pad_left = o.clone();
   	Mat Pad_up = o.clone();
   	Pad_left = pad_left * Pad_left;
   	Pad_up = pad_up * Pad_up;
   	Mat1i x_int = X_b - Pad_left;
   	Mat1i y_int = Y_b - Pad_up;

   	Mat internal_points = Mat::zeros(X_b.rows,3,CV_32S);
   	x_int.copyTo(internal_points.col(0));
   	y_int.copyTo(internal_points.col(1));
   	o.copyTo(internal_points.col(2));
   	internal_points.convertTo(internal_points, CV_64F);

   	Mat XY = H.inv() * (internal_points.t());

   	// -- Normalize using the last value
	Mat lastRow = XY.row(2);
	Mat tmp;
	repeat(lastRow, 3, 1, tmp);
	XY = XY / tmp;
	XY = XY.t();

	Mat X_i = XY.col(0);
	Mat Y_i = XY.col(1);
	Mat y_int_2, x_int_2;
	y_int.convertTo(y_int_2, CV_64F);
	x_int.convertTo(x_int_2, CV_64F);	

	// -- Get Indices for points that are in common to both base and next image
    Mat1b indices = X_i >= 0 & X_i < image_next.cols & Y_i >= 0 & Y_i < image_next.rows & 
    				y_int_2 >= 0 & y_int_2 < img_dist_b.rows & x_int_2 >=0 & x_int_2 < img_dist_b.cols;


    int numVals = indices.rows;

    double alpha;
    // -- Get the Data Pointers of the Mat Object that is supposed to be manipulated
    double* input = (double*)(img_mid_new.data);
    double* next_image = (double*)(image_next_d.data);
    unsigned char* dist_b = (unsigned char*)(img_dist_b.data);
    unsigned char* dist_i = (unsigned char*)(img_dist_i.data);
    int channels = img_mid_new.channels();

    // TODO(Rishabh) : Assert Continuity of the Matrix

    // -- Perform Real Concatenation
    for (int i = 0; i < numVals; i++) {
    	if (indices.at<uchar>(i) != 255)
    		continue;

    	// TODO(Rishabh): Fix Blending using Distance from the borders of the image is not working on expected lines
    	// alpha = dist_i[img_dist_i.cols * int((Y_i.at<double>(i))) + int((X_i.at<double>(i)))] + dist_b[img_dist_b.cols * int((y_int_2.at<double>(i))) + int((x_int_2.at<double>(i)))];
    	// if (alpha < 0.00001)		
    	// 	alpha = 0;
    	// else {
	    // 	alpha = (dist_i[img_dist_i.cols * int((Y_i.at<double>(i))) + int((X_i.at<double>(i)))]) / alpha;
    	// }
    	
    	// Use a simple averaging and absolute values from the next image for blending, based on where the point is
    	if (input[img_mid_new.cols * channels * int(Y_b.at<int>(i)) + int(X_b.at<int>(i))*channels] == 0)
    		alpha = 1;
    	else
    		alpha = 0.3;

    	// -- Modification of Values
    	int image_mid_ind = img_mid_new.cols * channels * int(Y_b.at<int>(i)) + int(X_b.at<int>(i))*channels;
    	int image_next_ind = image_next.cols * channels * int(round(Y_i.at<double>(i))) + int(round(X_i.at<double>(i))) * channels;

	    input[image_mid_ind]  = (alpha) * next_image[image_next_ind] + (1 - alpha) * input[image_mid_ind];
	    input[image_mid_ind + 1]  = (alpha) * next_image[image_next_ind + 1] + (1 - alpha) * input[image_mid_ind + 1];
	    input[image_mid_ind + 2]  = (alpha) * next_image[image_next_ind + 2] + (1 - alpha) * input[image_mid_ind + 2];
    }

    // -- Now, Fetch the regions that exceed the base image boundary from the next image
    indices = X_i >= 0 & X_i < image_next.cols & Y_i >= 0 & Y_i < image_next.rows & 
    		  (y_int_2 <= 0 | y_int_2 >= img_dist_b.rows | x_int_2 <= 0 | x_int_2 >= img_dist_b.cols);
    numVals = indices.rows;

    for (int i = 0; i < numVals; i++) {
    	if (indices.at<uchar>(i) == 0)
    		continue;

    	int image_mid_ind = img_mid_new.cols * channels * int(Y_b.at<int>(i)) + int(X_b.at<int>(i)) * channels;
    	int image_next_ind = image_next.cols * channels * int(round(Y_i.at<double>(i))) + int(round(X_i.at<double>(i))) * channels;

	    input[image_mid_ind]  = next_image[image_next_ind];
	    input[image_mid_ind + 1]  = next_image[image_next_ind + 1]; 
	    input[image_mid_ind + 2]  = next_image[image_next_ind + 2];
    }

    // -- Convert it back for the next iteration
    img_mid_new.convertTo(image_mid, CV_8UC3);

}

int main(int argc, char** argv )
{

	// Mat image_mid = imread("images/image1_2.jpg", 1 );
    // Mat image_left = imread("images/image1_3.jpg", 1 );
    // Mat image_right = imread("images/image1_1.jpg", 1 );

	// Mat image_mid = imread("images/image2_1.jpg", 1 );
    // Mat image_left = imread("images/image2_2.jpg", 1 );
    // Mat image_right = imread("images/image2_3.jpg", 1 );

    Mat image_mid = imread("images/image3_1.jpg", 1 );
    Mat image_left = imread("images/image3_2.jpg", 1 );
    Mat image_right = imread("images/image3_3.jpg", 1 );
    Mat image_right_2 = imread("images/image3_4.jpg", 1 );


    if ( !image_left.data )
    {
        printf("No image data \n");
        return -1;
    }

    std::cout << " Stitching Images...." << std::endl;
    concatenate(image_mid, image_left);
    concatenate(image_mid, image_right);
    concatenate(image_mid, image_right_2);

    std::cout << " Stitching Images Done" << std::endl;

    imwrite("Robot.jpg", image_mid);
    namedWindow("Concatenated Image", WINDOW_AUTOSIZE );
    imshow("Concatenated Image", image_mid);		
    waitKey(0);

}
