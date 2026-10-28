#include "Yolov8_Utils.h"

using namespace cv;
using namespace std;


bool CheckParams(int netHeight, int netWidth, const int* netStride, int strideSize) {
    if (netHeight % netStride[strideSize - 1] != 0 || netWidth % netStride[strideSize - 1] != 0)
    {
        cout << "Error:_netHeight and _netWidth must be multiple of max stride " << netStride[strideSize - 1] << "!" << endl;
        return false;
    }
    return true;
}
bool CheckModelPath(std::string modelPath) {
    if (0 != _access(modelPath.c_str(), 0)) {
        cout << "Model path does not exist,  please check " << modelPath << endl;
        return false;
    }
    else
        return true;

}
void LetterBox(const cv::Mat& image, cv::Mat& outImage, cv::Vec4d& params, const cv::Size& newShape,
               bool autoShape, bool scaleFill, bool scaleUp, int stride, const cv::Scalar& color)
{
    if (false) {
        int maxLen = MAX(image.rows, image.cols);
        outImage = Mat::zeros(Size(maxLen, maxLen), CV_8UC3);
        image.copyTo(outImage(Rect(0, 0, image.cols, image.rows)));
        params[0] = 1;
        params[1] = 1;
        params[3] = 0;
        params[2] = 0;
    }

    cv::Size shape = image.size();
    float r = std::min((float)newShape.height / (float)shape.height,
                       (float)newShape.width / (float)shape.width);
    if (!scaleUp)
        r = std::min(r, 1.0f);

    float ratio[2]{ r, r };
    int new_un_pad[2] = { (int)std::round((float)shape.width * r),(int)std::round((float)shape.height * r) };

    auto dw = (float)(newShape.width - new_un_pad[0]);
    auto dh = (float)(newShape.height - new_un_pad[1]);

    if (autoShape)
    {
        dw = (float)((int)dw % stride);
        dh = (float)((int)dh % stride);
    }
    else if (scaleFill)
    {
        dw = 0.0f;
        dh = 0.0f;
        new_un_pad[0] = newShape.width;
        new_un_pad[1] = newShape.height;
        ratio[0] = (float)newShape.width / (float)shape.width;
        ratio[1] = (float)newShape.height / (float)shape.height;
    }

    dw /= 2.0f;
    dh /= 2.0f;

    if (shape.width != new_un_pad[0] && shape.height != new_un_pad[1])
    {
        cv::resize(image, outImage, cv::Size(new_un_pad[0], new_un_pad[1]));
    }
    else {
        outImage = image.clone();
    }

    int top = int(std::round(dh - 0.1f));
    int bottom = int(std::round(dh + 0.1f));
    int left = int(std::round(dw - 0.1f));
    int right = int(std::round(dw + 0.1f));
    params[0] = ratio[0];
    params[1] = ratio[1];
    params[2] = left;
    params[3] = top;
    cv::copyMakeBorder(outImage, outImage, top, bottom, left, right, cv::BORDER_CONSTANT, color);
}

void GetMask(const cv::Mat& maskProposals, const cv::Mat& maskProtos, std::vector<OutputParams>& output, const MaskParams& maskParams) {
    //cout << maskProtos.size << endl;

    int net_width = maskParams.netWidth;
    int net_height = maskParams.netHeight;
    int seg_channels = maskProtos.size[1];
    int seg_height = maskProtos.size[2];
    int seg_width = maskProtos.size[3];
    float mask_threshold = maskParams.maskThreshold;
    Vec4f params = maskParams.params;
    Size src_img_shape = maskParams.srcImgShape;

    Mat protos = maskProtos.reshape(0, { seg_channels,seg_width * seg_height });

    Mat matmul_res = (maskProposals * protos).t();
    Mat masks = matmul_res.reshape(output.size(), { seg_width,seg_height });
    vector<Mat> maskChannels;
    split(masks, maskChannels);
    for (int i = 0; i < output.size(); ++i) {
        Mat dest, mask;
        //sigmoid
        cv::exp(-maskChannels[i], dest);
        dest = 1.0 / (1.0 + dest);

        Rect roi(int(params[2] / net_width * seg_width), int(params[3] / net_height * seg_height), int(seg_width - params[2] / 2), int(seg_height - params[3] / 2));
        dest = dest(roi);
        resize(dest, mask, src_img_shape, INTER_NEAREST);

        //crop
        /*if (output[i].box.x == 0 && output[i].box.y == 0 && output[i].box.width == 0 && output[i].box.height == 0)
            continue;*/

        Rect temp_rect = output[i].box;
        mask = mask(temp_rect) > mask_threshold;
        output[i].boxMask = mask;
    }
}

void GetMask2(const Mat& maskProposals, const Mat& maskProtos, OutputParams& output, const MaskParams& maskParams) {
    int net_width = maskParams.netWidth;
    int net_height = maskParams.netHeight;
    int seg_channels = maskProtos.size[1];
    int seg_height = maskProtos.size[2];
    int seg_width = maskProtos.size[3];
    float mask_threshold = maskParams.maskThreshold;
    Vec4f params = maskParams.params;
    //Size src_img_shape = maskParams.srcImgShape;

    Rect temp_rect = output.box;


    //crop from mask_protos
    int rang_x = floor((temp_rect.x * params[0] + params[2]) / net_width * seg_width);
    int rang_y = floor((temp_rect.y * params[1] + params[3]) / net_height * seg_height);
    int rang_w = ceil(((temp_rect.x + temp_rect.width) * params[0] + params[2]) / net_width * seg_width) - rang_x;
    int rang_h = ceil(((temp_rect.y + temp_rect.height) * params[1] + params[3]) / net_height * seg_height) - rang_y;


    rang_w = MAX(rang_w, 1);
    rang_h = MAX(rang_h, 1);
    if (rang_x + rang_w > seg_width) {
        if (seg_width - rang_x > 0)
            rang_w = seg_width - rang_x;
        else
            rang_x -= 1;
    }
    if (rang_y + rang_h > seg_height) {
        if (seg_height - rang_y > 0)
            rang_h = seg_height - rang_y;
        else
            rang_y -= 1;
    }

    vector<Range> roi_rangs;
    roi_rangs.push_back(Range(0, 1));
    roi_rangs.push_back(Range::all());
    roi_rangs.push_back(Range(rang_y, rang_h + rang_y));
    roi_rangs.push_back(Range(rang_x, rang_w + rang_x));

    //crop
    Mat temp_mask_protos = maskProtos(roi_rangs).clone();
    Mat protos = temp_mask_protos.reshape(0, { seg_channels,rang_w * rang_h });
    Mat matmul_res = (maskProposals * protos).t();
    Mat masks_feature = matmul_res.reshape(1, { rang_h,rang_w });
    Mat dest, mask;

    //sigmoid
    cv::exp(-masks_feature, dest);
    dest = 1.0 / (1.0 + dest);

    int left = floor((net_width / seg_width * rang_x - params[2]) / params[0]);
    int top = floor((net_height / seg_height * rang_y - params[3]) / params[1]);
    int width = ceil(net_width / seg_width * rang_w / params[0]);
    int height = ceil(net_height / seg_height * rang_h / params[1]);

    resize(dest, mask, Size(width, height), INTER_NEAREST);
    Rect mask_rect = temp_rect - Point(left, top);
    mask_rect &= Rect(0, 0, width, height);
    mask = mask(mask_rect) > mask_threshold;
    if (mask.rows != temp_rect.height || mask.cols != temp_rect.width)
    {
        resize(mask, mask, temp_rect.size(), INTER_NEAREST);
    }
    output.boxMask = mask;

    // 计算 mask 的最小外接矩形（OBB 框）和旋转角度
    cv::RotatedRect originalMaskOBBRect;
    cv::Rect bigBox;
    // 计算 mask 的最小外接矩形
    std::vector<std::vector<Point>> contours;
    cv::findContours(mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
    if (!contours.empty())
    {
        if (contours.size() > 1)
        {
            std::sort(contours.begin(), contours.end(),
                      [&](std::vector<Point>& it1, std::vector<Point>& it2)
                      {
                          return it1.size() > it2.size();
                      });
        }

        cv::RotatedRect maskOBBRect = minAreaRect(contours[0]); // 获取裁剪后 mask 的最小外接矩形
        //originalMaskOBBRect = minAreaRect(contours[0]); // 获取裁剪后 mask 的最小外接矩形

        if (maskOBBRect.size.width > maskOBBRect.size.height)
        {
            maskOBBRect.size.width = maskOBBRect.size.width + 16;
        }
        else
        {
            maskOBBRect.size.height = maskOBBRect.size.height + 16;
        }

        // 将最小外接矩形转换为原始图像坐标系中的坐标
        cv::Point2f vertices[4];
        maskOBBRect.points(vertices);
        for (int i = 0; i < 4; ++i)
        {
            vertices[i].x += temp_rect.x; // 转换为原始图像坐标系中的 x 坐标
            vertices[i].y += temp_rect.y; // 转换为原始图像坐标系中的 y 坐标

            if (vertices[i].x < 0) vertices[i].x = 0;
            if (vertices[i].y < 0) vertices[i].y = 0;
            if (vertices[i].x > 2048) vertices[i].x = 2048;
            if (vertices[i].y > 1024) vertices[i].y = 1024;
        }
        vector<cv::Point2f> vet(vertices, vertices + sizeof(vertices) / sizeof(vertices[0]));
        originalMaskOBBRect = minAreaRect(vet); // 获取最小外接矩形

        //获取大矩形框
        int minX = vet[0].x, maxX = vet[0].x;
        int minY = vet[0].y, maxY = vet[0].y;
        for (int i = 0; i < 4; i++)
        {
            minX = vet[i].x < minX ? vet[i].x : minX;
            minY = vet[i].y < minY ? vet[i].y : minY;
            maxX = vet[i].x > maxX ? vet[i].x : maxX;
            maxY = vet[i].y > maxY ? vet[i].y : maxY;
            if (minX < 0) minX = 0;
            if (minY < 0) minY = 0;
            if (maxX > 2048) maxX = 2048;
            if (maxY > 1024) maxY = 1024;
        }

        Rect bRect(minX, minY, maxX - minX, maxY - minY);
        bigBox = bRect;
    }
    // 将最小外接矩形和旋转角度存储在 output 中
    output.rotatedBox = originalMaskOBBRect;
    output.bigBox = bigBox;
}


int BBox2Obb(float centerX, float centerY, float boxW, float boxH, float angle, RotatedRect& rotatedRect) {
    rotatedRect = RotatedRect(Point2f(centerX, centerY), Size2f(boxW, boxH),angle);
    return 0;
}
void DrawRotatedBox(cv::Mat &srcImg, cv::RotatedRect box, cv::Scalar color, int thinkness) {
    Point2f p[4];
    box.points(p);
    for (int l = 0; l < 4; ++l) {
        line(srcImg, p[l], p[(l + 1) % 4], color, thinkness, 8);
    }
}

void DrawPred(Mat& img, vector<OutputParams> result, std::vector<std::string> classNames, vector<Scalar> color, bool isVideo) {
    Mat mask = img.clone();
    for (int i = 0; i < result.size(); i++) {
        int left, top;

        //int color_num = i;
        if (result[i].box.area() > 0) {
            rectangle(img, result[i].box, color[result[i].id], 2, 8);
            left = result[i].box.x;
            top = result[i].box.y;
        }
        if (result[i].rotatedBox.size.width * result[i].rotatedBox.size.height > 0) {
            DrawRotatedBox(img, result[i].rotatedBox, color[result[i].id], 2);
            left = result[i].rotatedBox.center.x;
            top = result[i].rotatedBox.center.y;
        }
        if (result[i].boxMask.rows && result[i].boxMask.cols > 0)
            mask(result[i].box).setTo(color[result[i].id], result[i].boxMask);
        string label = classNames[result[i].id] + ":" + to_string(result[i].confidence);
        int baseLine;
        Size labelSize = getTextSize(label, FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);
        top = max(top, labelSize.height);
        //rectangle(frame, Point(left, top - int(1.5 * labelSize.height)), Point(left + int(1.5 * labelSize.width), top + baseLine), Scalar(0, 255, 0), FILLED);
        putText(img, label, Point(left, top), FONT_HERSHEY_SIMPLEX, 1, color[result[i].id], 2);

        //		cv::imwrite("mask.jpg", mask);
    }
    addWeighted(img, 0.5, mask, 0.5, 0, img); //add mask to src

    /*imshow("1", img);
    if (!isVideo)
        waitKey();*/
    //destroyAllWindows();

}
