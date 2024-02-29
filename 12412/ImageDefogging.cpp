#include "ImageDefogging.h"


int main()
{
	//Ч������
	double omega = 0.95;
	double numt = 0.3;
	int rectSize = 15;

	clock_t startTime, endTime;
	startTime = clock();//��ʱ��ʼ

	Mat src = imread("1.png");
	Mat dst;
	ImageDefogging(src, dst, rectSize, omega, numt);

	endTime = clock();				//��ʱ����
	cout << "The run time is: " << (double)(endTime - startTime) / CLOCKS_PER_SEC << "s" << endl;

	imwrite("dst.jpg", dst);

	/*float scale = 0.15;
	resize(src, src, Size(src.cols*scale, src.rows*scale));
	resize(dst, dst, Size(dst.cols*scale, dst.rows*scale));
	resize(dst1, dst1, Size(dst1.cols*scale, dst1.rows*scale));*/

	dst /= 255;
	imshow("src", src);
	imshow("dst", dst);

	waitKey();
	return 0;
}

void ImageDefogging(Mat src, Mat& dst, int rectSize, double omega, double numt)
{
	//��ԭͼ���й�һ��
	Mat I;
	src.convertTo(I, CV_32F);
	I /= 255;

	float A[3] = { 0 };
	Mat dark = DarkChannel(I, rectSize);
	AtmLight(I, dark, A);

	Mat te = TransmissionEstimate(I, A, rectSize, omega);
	Mat t = TransmissionRefine(src, te);
	dst = Defogging(I, t, A, numt);
}

Mat DarkChannel(Mat srcImg, int size)
{
	vector<Mat> chanels;
	split(srcImg, chanels);

	//��RGB��ͨ���е���С������ֵ
	Mat minChannel = (cv::min)((cv::min)(chanels[0], chanels[1]), chanels[2]);
	Mat kernel = getStructuringElement(MORPH_RECT, Size(size, size));

	Mat dark(minChannel.rows, minChannel.cols, CV_32FC1);
	erode(minChannel, dark, kernel);	//ͼ��ʴ
	return dark;
}

template<typename T> vector<int> argsort(const vector<T>& array)
{
	const int array_len(array.size());
	vector<int> array_index(array_len, 0);
	for (int i = 0; i < array_len; ++i)
		array_index[i] = i;

	sort(array_index.begin(), array_index.end(),
		[&array](int pos1, int pos2) {return (array[pos1] < array[pos2]); });

	return array_index;
}

void AtmLight(Mat src, Mat dark, float outA[3])
{
	int row = src.rows;
	int col = src.cols;
	int imgSize = row * col;

	//����ͼ���ԭͼתΪ������
	vector<int> darkVector = dark.reshape(1, imgSize);
	Mat srcVector = src.reshape(3, imgSize);

	//�������ȵĴ�Сȡǰ0.1%�����أ����ȸߣ�
	int numpx = int(max(floor(imgSize / 1000), 1.0));
	vector<int> indices = argsort(darkVector);
	vector<int> dstIndices(indices.begin() + (imgSize - numpx), indices.end());

	for (int i = 0; i < numpx; ++i)
	{
		outA[0] += srcVector.at<Vec3f>(dstIndices[i], 0)[0];
		outA[1] += srcVector.at<Vec3f>(dstIndices[i], 0)[1];
		outA[2] += srcVector.at<Vec3f>(dstIndices[i], 0)[2];
	}
	outA[0] /= numpx;
	outA[1] /= numpx;
	outA[2] /= numpx;
}

Mat TransmissionEstimate(Mat src, float outA[3], int size, float omega)
{
	Mat imgA = Mat::zeros(src.rows, src.cols, CV_32FC3);

	vector<Mat> chanels;
	split(src, chanels);
	for (int i = 0; i < 3; ++i)
	{
		chanels[i] = chanels[i] / outA[i];
	}

	merge(chanels, imgA);
	Mat transmission = 1 - omega * DarkChannel(imgA, size);	//����͸����Ԥ��ֵ
	return transmission;
}

Mat Guidedfilter(Mat src, Mat te, int r, float eps)
{
	Mat meanI, meanT, meanIT, meanII, meanA, meanB;
	boxFilter(src, meanI, CV_32F, Size(r, r));
	boxFilter(te, meanT, CV_32F, Size(r, r));
	boxFilter(src.mul(te), meanIT, CV_32F, Size(r, r));
	Mat covIT = meanIT - meanI.mul(meanT);

	boxFilter(src.mul(src), meanII, CV_32F, Size(r, r));
	Mat varI = meanII - meanI.mul(meanI);

	Mat a = covIT / (varI + eps);
	Mat b = meanT - a.mul(meanI);
	boxFilter(a, meanA, CV_32F, Size(r, r));
	boxFilter(b, meanB, CV_32F, Size(r, r));

	Mat t = meanA.mul(src) + meanB;

	return t;
}

Mat TransmissionRefine(Mat src, Mat te)
{
	Mat gray;
	cvtColor(src, gray, CV_BGR2GRAY);
	gray.convertTo(gray, CV_32F);
	gray /= 255;

	int r = 60;
	float eps = 0.0001;
	Mat t = Guidedfilter(gray, te, r, eps);
	return t;
}

Mat Defogging(Mat src, Mat t, float outA[3], float tx)
{
	Mat dst = Mat::zeros(src.rows, src.cols, CV_32FC3);
	t = (cv::max)(t, tx);				//������ֵ��Ͷ��ͼt ��ֵ��Сʱ���ᵼ��ͼ��������׳�����

	vector<Mat> chanels;
	split(src, chanels);
	for (int i = 0; i < 3; ++i)
	{
		chanels[i] = (chanels[i] - outA[i]) / t + outA[i];
	}
	merge(chanels, dst);

	dst *= 255;				//��һ����ԭ
	return dst;
}

void GetFiles(string path, vector<string>& files, string fileType)
{
	intptr_t   hFile = 0;
	struct _finddata_t fileinfo;
	string p;
	if ((hFile = _findfirst(p.assign(path).append("\\" + fileType).c_str(), &fileinfo)) != -1)
	{
		do
		{
			if ((fileinfo.attrib & _A_SUBDIR))
			{
				if (strcmp(fileinfo.name, ".") != 0 && strcmp(fileinfo.name, "..") != 0)
					GetFiles(p.assign(path).append("\\").append(fileinfo.name), files);
			}
			else
			{
				files.push_back(p.assign(path).append("\\").append(fileinfo.name));
			}
		} while (_findnext(hFile, &fileinfo) == 0);
		_findclose(hFile);

		//�ļ�����
		sort(files.begin(), files.end(),
			[](const string& a, const string& b) {
				return a.length() < b.length() || a.length() == b.length() && a < b;
			});
	}
}

