#include "SubImageMatch.h"

int ustc_ConvertBgr2Gray(Mat bgrImg, Mat& grayImg) {
	int bgrcol = bgrImg.cols, bgrrow = bgrImg.rows;
	if (NULL == bgrImg.data || 3 != bgrImg.channels() || CV_8U != bgrImg.depth()) {
		return SUB_IMAGE_MATCH_FAIL;
	}
	grayImg.create(bgrrow, bgrcol, CV_8UC1);
	if (grayImg.isContinuous() && bgrImg.isContinuous()) {
		bgrcol = bgrrow * bgrcol * 3;
		bgrrow = 1;
	}
	else {
		bgrcol *= 3;
	}
	for (int i = 0; i < bgrrow; i++) {
		uchar *bgrpixel = bgrImg.ptr<uchar>(i);
		uchar *graypixel = grayImg.ptr<uchar>(i);
		for (int j = 0, k = 0; j < bgrcol; j += 3, k++) {
			graypixel[k] = (uchar)((bgrpixel[j] * (int)(0.114f * 1024) + bgrpixel[j + 1] * (int)(0.587f * 1024) + bgrpixel[j + 2] * (int)(0.299f * 1024)) >> 10);
		}
	}
	return SUB_IMAGE_MATCH_OK;
}

int ustc_CalcGrad(Mat grayImg, Mat& gradImg_x, Mat& gradImg_y) {
	int graycol = grayImg.cols, grayrow = grayImg.rows;
	if (NULL == grayImg.data || 1 != grayImg.channels() || CV_8U != grayImg.depth()) {
		return SUB_IMAGE_MATCH_FAIL;
	}
	gradImg_x.create(grayrow, graycol, CV_32FC1);
	gradImg_y.create(grayrow, graycol, CV_32FC1);
	int grayrow_1 = grayrow - 1;
	int graycol_1 = graycol - 1;
	if (grayrow >= 3 && graycol >= 3) {
		float *grad_xline = gradImg_x.ptr<float>(0);
		float *grad_yline = gradImg_y.ptr<float>(0);
		for (int i = 0; i < grayrow; i++) {
			grad_xline[i] = 0;
			grad_yline[i] = 0;
		}
		uchar *frontline;
		uchar *thisline = grayImg.ptr<uchar>(0);
		uchar *nextline = grayImg.ptr<uchar>(1);
		for (int i = 1; i < grayrow_1; i++) {
			frontline = thisline;
			thisline = nextline;
			nextline = grayImg.ptr<uchar>(i + 1);
			grad_xline = gradImg_x.ptr<float>(i);
			grad_yline = gradImg_y.ptr<float>(i);
			grad_xline[0] = 0;
			grad_yline[0] = 0;
			for (int j = 1; j < graycol_1; j++) {
				grad_xline[j] = -frontline[j - 1] + frontline[j + 1] - 2 * thisline[j - 1] + 2 * thisline[j + 1] - nextline[j - 1] + nextline[j + 1];
				grad_yline[j] = -frontline[j - 1] - 2 * frontline[j] - frontline[j + 1] + nextline[j - 1] + 2 * nextline[j] + nextline[j + 1];
			}
			grad_xline[graycol_1] = 0;
			grad_yline[graycol_1] = 0;
		}
		grad_xline = gradImg_x.ptr<float>(grayrow_1);
		grad_yline = gradImg_y.ptr<float>(grayrow_1);
		for (int i = 0; i < grayrow; i++) {
			grad_xline[i] = 0;
			grad_yline[i] = 0;
		}
	}
	else {
		gradImg_x.setTo(0);
		gradImg_y.setTo(0);
	}
	return SUB_IMAGE_MATCH_OK;
}

int ustc_CalcAngleMag(Mat gradImg_x, Mat gradImg_y, Mat& angleImg, Mat& magImg) {
	int gradcol = gradImg_x.cols, gradrow = gradImg_x.rows;
	if (NULL == gradImg_y.data || NULL == gradImg_x.data || gradImg_y.rows != gradrow || gradImg_y.cols != gradcol || 1 != gradImg_y.channels() || 1 != gradImg_x.channels() || CV_32F != gradImg_x.depth() || CV_32F != gradImg_y.depth()) {
		return SUB_IMAGE_MATCH_FAIL;
	}
	angleImg.create(gradrow, gradcol, CV_32FC1);
	magImg.create(gradrow, gradcol, CV_32FC1);
	for (int i = 0; i < gradrow; i++) {
		float *grad_x_pixel = gradImg_x.ptr<float>(i);
		float *grad_y_pixel = gradImg_y.ptr<float>(i);
		float *angle_pixel = angleImg.ptr<float>(i);
		float *mag_pixel = magImg.ptr<float>(i);
		for (int j = 0; j < gradcol; j++) {
			float dx = grad_x_pixel[j], dy = grad_y_pixel[j];
			float ax = (dx > 0) ? dx : -dx, ay = (dy > 0) ? dy : -dy;
			float max = ax, min = ay;
			if (max < min) {
				max = ay;
				min = ax;
			}
			float a = min / (max + (float)DBL_EPSILON);
			float s = a*a;
			float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
			if (ay > ax) {
				r = 1.57079637f - r;
			}
			if (dx < 0) {
				r = 3.14159274f - r;
			}
			if (dy < 0) {
				r = 6.28318548f - r;
			}
			angle_pixel[j] = r / 3.14159274f * 180;
			float x = dx * dx + dy * dy;
			float aa = x;
			unsigned int t = *(unsigned int *)&x;
			t = (t + 0x3f76cf62) >> 1;
			x = *(float *)&t;
			mag_pixel[j] = (x + aa / x) * 0.5;
		}
	}
	return SUB_IMAGE_MATCH_OK;
}

int ustc_Threshold(Mat grayImg, Mat& binaryImg, int th) {
	int graycol = grayImg.cols, grayrow = grayImg.rows;
	if (NULL == grayImg.data || 1 != grayImg.channels() || CV_8U != grayImg.depth()) {
		return SUB_IMAGE_MATCH_FAIL;
	}
	binaryImg.create(grayrow, graycol, CV_8UC1);
	if (grayImg.isContinuous() && binaryImg.isContinuous()) {
		graycol = grayrow * graycol;
		grayrow = 1;
	}
	for (int i = 0; i < grayrow; i++) {
		uchar *graypixel = grayImg.ptr<uchar>(i);
		uchar *binarypixel = binaryImg.ptr<uchar>(i);
		for (int j = 0; j < graycol; j++) {
			binarypixel[j] = (((th - graypixel[j]) >> 31) & 1) * 255;
		}
	}
	return SUB_IMAGE_MATCH_OK;
}

int ustc_CalcHist(Mat grayImg, int* hist, int hist_len) {
	int graycol = grayImg.cols, grayrow = grayImg.rows;
	if (NULL == grayImg.data || 1 != grayImg.channels() || NULL == hist || hist_len <= 0 || CV_8U != grayImg.depth()) {
		return SUB_IMAGE_MATCH_FAIL;
	}
	if (grayImg.isContinuous()) {
		graycol = grayrow * graycol;
		grayrow = 1;
	}
	for (int i = 0; i < hist_len; i++) {
		hist[i] = 0;
	}
	for (int i = 0; i < grayrow; i++) {
		uchar *graypixel = grayImg.ptr<uchar>(i);
		for (int j = 0; j < graycol; j++) {
			uchar temp = graypixel[j];
			if (temp < hist_len) {
				hist[temp]++;
			}
		}
	}
	return SUB_IMAGE_MATCH_OK;
}

int ustc_SubImgMatch_gray(Mat grayImg, Mat subImg, int* x, int* y) {
	int graycol = grayImg.cols, grayrow = grayImg.rows, subcol = subImg.cols, subrow = subImg.rows;
	int stepcol = graycol - subcol + 1, steprow = grayrow - subrow + 1;
	if (NULL == grayImg.data || NULL == subImg.data || 1 != grayImg.channels() || 1 != subImg.channels() || stepcol <= 0 || steprow <= 0 || NULL == x || NULL == y || CV_8U != grayImg.depth() || CV_8U != subImg.depth()) {
		return SUB_IMAGE_MATCH_FAIL;
	}
	int min_total_dif = INT_MAX;
	*x = 0, *y = 0;
	for (int i = 0; i < steprow; i++) {
		for (int j = 0; j < stepcol; j++) {
			int total_dif = 0;
			for (int sub_i = 0; sub_i < subrow; sub_i++) {
				uchar *graypixel = grayImg.ptr<uchar>(i + sub_i);
				uchar *subpixel = subImg.ptr<uchar>(sub_i);
				for (int sub_j = 0; sub_j < subcol; sub_j++) {
					int pixel_dif = graypixel[j + sub_j] - subpixel[sub_j];
					if (pixel_dif > 0) {
						total_dif += pixel_dif;
					}
					else {
						total_dif += -pixel_dif;
					}
				}
			}
			if (total_dif < min_total_dif) {
				*x = j, *y = i;
				min_total_dif = total_dif;
			}
		}
	}
	return SUB_IMAGE_MATCH_OK;
}

int ustc_SubImgMatch_bgr(Mat colorImg, Mat subImg, int* x, int* y) {
	int colorcol = colorImg.cols, colorrow = colorImg.rows, subcol = subImg.cols, subrow = subImg.rows;
	int stepcol = colorcol - subcol + 1, steprow = colorrow - subrow + 1;
	if (NULL == colorImg.data || NULL == subImg.data || 3 != colorImg.channels() || 3 != subImg.channels() || stepcol <= 0 || steprow <= 0 || NULL == x || NULL == y || CV_8U != colorImg.depth() || CV_8U != subImg.depth()) {
		return SUB_IMAGE_MATCH_FAIL;
	}
	stepcol *= 3;
	subcol *= 3;
	int min_total_dif = INT_MAX;
	*x = 0, *y = 0; 
	for (int i = 0; i < steprow; i++) {
		for (int j = 0; j < stepcol; j += 3) {
			int total_dif = 0;
			for (int sub_i = 0; sub_i < subrow; sub_i++) {
				uchar *graypixel = colorImg.ptr<uchar>(i + sub_i);
				uchar *subpixel = subImg.ptr<uchar>(sub_i);
				for (int sub_j = 0; sub_j < subcol; sub_j += 3) {
					int pixel_dif = graypixel[j + sub_j] - subpixel[sub_j];
					if (pixel_dif > 0) {
						total_dif += pixel_dif;
					}
					else {
						total_dif += -pixel_dif;
					}
					pixel_dif = graypixel[j + sub_j + 1] - subpixel[sub_j + 1];
					if (pixel_dif > 0) {
						total_dif += pixel_dif;
					}
					else {
						total_dif += -pixel_dif;
					}
					pixel_dif = graypixel[j + sub_j + 2] - subpixel[sub_j + 2];
					if (pixel_dif > 0) {
						total_dif += pixel_dif;
					}
					else {
						total_dif += -pixel_dif;
					}
				}
			}
			if (total_dif < min_total_dif) {
				*x = j / 3, *y = i;
				min_total_dif = total_dif;
			}
		}
	}
	return SUB_IMAGE_MATCH_OK;
}

int ustc_SubImgMatch_corr(Mat grayImg, Mat subImg, int* x, int* y) {
	int graycol = grayImg.cols, grayrow = grayImg.rows, subcol = subImg.cols, subrow = subImg.rows;
	int stepcol = graycol - subcol + 1, steprow = grayrow - subrow + 1;
	if (NULL == grayImg.data || NULL == subImg.data || 1 != grayImg.channels() || 1 != subImg.channels() || stepcol <= 0 || steprow <= 0 || NULL == x || NULL == y || CV_8U != grayImg.depth() || CV_8U != subImg.depth()) { 
		return SUB_IMAGE_MATCH_FAIL;
	}
	float max_corr = -1;
	*x = 0, *y = 0; 
	for (int i = 0; i < steprow; i++) {
		for (int j = 0; j < stepcol; j++) {
			float sum_st = 0;
			float sum_ss = 0;
			float sum_tt = 0;
			for (int sub_i = 0; sub_i < subrow; sub_i++) {
				uchar *graypixel = grayImg.ptr<uchar>(i + sub_i);
				uchar *subpixel = subImg.ptr<uchar>(sub_i);
				for (int sub_j = 0; sub_j < subcol; sub_j++) {
					sum_st += graypixel[j + sub_j] * subpixel[sub_j];
					sum_ss += graypixel[j + sub_j] * graypixel[j + sub_j];
					sum_tt += subpixel[sub_j] * subpixel[sub_j];
				}
			}
			float xx = sum_ss*sum_tt;
			float xhalf = 0.5f * xx;
			int ii = *(int*)&xx;
			ii = 0x5f3759df - (ii >> 1);
			xx = *(float*)&ii;
			xx = xx * (1.5f - xhalf * xx * xx);
			float corr = sum_st*xx;
			if (corr > max_corr) {
				*x = j, *y = i;
				max_corr = corr;
			}
		}
	}
	return SUB_IMAGE_MATCH_OK;
}

int ustc_SubImgMatch_angle(Mat grayImg, Mat subImg, int* x, int* y) {
	int graycol = grayImg.cols, grayrow = grayImg.rows, subcol = subImg.cols, subrow = subImg.rows;
	int stepcol = graycol - subcol + 1, steprow = grayrow - subrow + 1;
	if (stepcol <= 0 || steprow <= 0 || NULL == x || NULL == y) { 
		return SUB_IMAGE_MATCH_FAIL;
	}
	Mat gradImg_x, gradImg_y, subgradImg_x, subgradImg_y;
	if (SUB_IMAGE_MATCH_FAIL == ustc_CalcGrad(grayImg, gradImg_x, gradImg_y) || SUB_IMAGE_MATCH_FAIL == ustc_CalcGrad(subImg, subgradImg_x, subgradImg_y)) {
		return SUB_IMAGE_MATCH_FAIL;
	}
	Mat angleImg, magImg, subangleImg, submagImg;
	if (SUB_IMAGE_MATCH_FAIL == ustc_CalcAngleMag(gradImg_x, gradImg_y, angleImg, magImg) || SUB_IMAGE_MATCH_FAIL == ustc_CalcAngleMag(subgradImg_x, subgradImg_y, subangleImg, submagImg)) {
		return SUB_IMAGE_MATCH_FAIL;
	}
	int min_total_dif = INT_MAX;
	*x = 0, *y = 0; 
	for (int i = 0; i < steprow; i++) {
		for (int j = 0; j < stepcol; j++) {
			int total_dif = 0;
			for (int sub_i = 0; sub_i < subrow; sub_i++) {
				float *anglepixel = angleImg.ptr<float>(i + sub_i);
				float *subanglepixel = subangleImg.ptr<float>(sub_i);
				for (int sub_j = 0; sub_j < subcol; sub_j++) {
					int pixel_dif = (int)(anglepixel[j + sub_j]) - (int)(subanglepixel[sub_j]); 
					if (pixel_dif < 0) {
						pixel_dif = -pixel_dif;
					}
					if (pixel_dif < 180) {
						total_dif += pixel_dif;
					}
					else {
						total_dif += 360 - pixel_dif;
					}
				}
			}
			if (total_dif < min_total_dif) {
				*x = j, *y = i;
				min_total_dif = total_dif;
			}
		}
	}
	return SUB_IMAGE_MATCH_OK;
}

int ustc_SubImgMatch_mag(Mat grayImg, Mat subImg, int* x, int* y) {
	int graycol = grayImg.cols, grayrow = grayImg.rows, subcol = subImg.cols, subrow = subImg.rows;
	int stepcol = graycol - subcol + 1, steprow = grayrow - subrow + 1;
	if (stepcol <= 0 || steprow <= 0 || NULL == x || NULL == y) { 
		return SUB_IMAGE_MATCH_FAIL;
	}
	Mat gradImg_x, gradImg_y, subgradImg_x, subgradImg_y;
	if (SUB_IMAGE_MATCH_FAIL == ustc_CalcGrad(grayImg, gradImg_x, gradImg_y) || SUB_IMAGE_MATCH_FAIL == ustc_CalcGrad(subImg, subgradImg_x, subgradImg_y)) {
		return SUB_IMAGE_MATCH_FAIL;
	}
	Mat angleImg, magImg, subangleImg, submagImg;
	if (SUB_IMAGE_MATCH_FAIL == ustc_CalcAngleMag(gradImg_x, gradImg_y, angleImg, magImg) || SUB_IMAGE_MATCH_FAIL == ustc_CalcAngleMag(subgradImg_x, subgradImg_y, subangleImg, submagImg)) {
		return SUB_IMAGE_MATCH_FAIL;
	}
	int min_total_dif = INT_MAX;
	*x = 0, *y = 0; 
	for (int i = 0; i < steprow; i++) {
		for (int j = 0; j < stepcol; j++) {
			int total_dif = 0;
			for (int sub_i = 0; sub_i < subrow; sub_i++) {
				float *magpixel = magImg.ptr<float>(i + sub_i);
				float *submagpixel = submagImg.ptr<float>(sub_i);
				for (int sub_j = 0; sub_j < subcol; sub_j++) {
					int pixel_dif = (int)(magpixel[j + sub_j]) - (int)(submagpixel[sub_j]);
					if (pixel_dif > 0) {
						total_dif += pixel_dif;
					}
					else {
						total_dif += -pixel_dif;
					}
				}
			}
			if (total_dif < min_total_dif) {
				*x = j, *y = i;
				min_total_dif = total_dif;
			}
		}
	}
	return SUB_IMAGE_MATCH_OK;
}

int ustc_SubImgMatch_hist(Mat grayImg, Mat subImg, int* x, int* y) {
	int graycol = grayImg.cols, grayrow = grayImg.rows, subcol = subImg.cols, subrow = subImg.rows;
	int stepcol = graycol - subcol + 1, steprow = grayrow - subrow + 1;
	if (NULL == grayImg.data || 1 != grayImg.channels() || stepcol <= 0 || steprow <= 0 || NULL == x || NULL == y || CV_8U != grayImg.depth()) { 
		return SUB_IMAGE_MATCH_FAIL;
	}
	int* hist_temp = new int[256];
	int* sub_hist_temp = new int[256];
	if (SUB_IMAGE_MATCH_FAIL == ustc_CalcHist(subImg, sub_hist_temp, 256)) {
		return SUB_IMAGE_MATCH_FAIL;
	}
	int min_total_dif = INT_MAX;
	*x = 0, *y = 0; 
	for (int i = 0; i < steprow; i++) {
		for (int j = 0; j < stepcol; j++) {
			for (int k = 0; k < 256; k++) {
				hist_temp[k] = 0;
			}
			for (int sub_i = 0; sub_i < subrow; sub_i++) {
				uchar *graypixel = grayImg.ptr<uchar>(i + sub_i);
				for (int sub_j = 0; sub_j < subcol; sub_j++) {
					hist_temp[graypixel[j + sub_j]]++;
				}
			}
			int total_dif = 0;
			for (int k = 0; k < 256; k++) {
				int hist_dif = hist_temp[k] - sub_hist_temp[k];
				if (hist_dif > 0) {
					total_dif += hist_dif;
				}
				else {
					total_dif += -hist_dif;
				}
			}
			if (total_dif < min_total_dif) {
				*x = j, *y = i;
				min_total_dif = total_dif;
			}
		}
	}
	delete[] hist_temp;
	delete[] sub_hist_temp;
	return SUB_IMAGE_MATCH_OK;
}
