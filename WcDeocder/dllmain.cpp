// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include <stdio.h>
#include <fstream>

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

const std::string md_DetectPrototxt = "models\\detect.prototxt";
const std::string md_DetectCaffeModel = "models\\detect.caffemodel";
const std::string md_SrPrototxt = "models\\sr.prototxt";
const std::string md_SrCaffeModel = "models\\sr.caffemodel";

static cv::Ptr<cv::wechat_qrcode::WeChatQRCode> wechatQRCode = nullptr;

static std::string last_error = "";

struct POINTv2
{
    int x;
    int y;

    POINTv2(int x, int y) : x(x), y(y) { }

    POINTv2() = default;
};
struct WRESULT
{
    UINT data_len;
    UINT points_len;
    POINTv2* points;
    char* data;

public:
    void release()
    {
        if (points != nullptr)
        {
            delete[] points;
            points = nullptr;
        }

        if (data != nullptr)
        {
            delete[] data;
            data = nullptr;
        }
    }
};

struct CRESULT
{
    UINT data_len;
    BYTE* data;
};

void compress_img_to_gray(cv::Mat& img, int quality)
{
    if (quality <= 0 || quality > 100) quality = 100;

    std::vector<uchar> buf;
    std::vector<int> params;
    params.push_back(cv::IMWRITE_JPEG_QUALITY);
    params.push_back(quality);

    if (!cv::imencode(".jpg", img, buf, params))
    {
        // Handle imencode failure
        return;
    }

    cv::Mat compressed_img = cv::imdecode(buf, cv::IMREAD_GRAYSCALE);
    if (compressed_img.empty())
    {
        // Handle imdecode failure
        return;
    }

    img = compressed_img;
}

void cpr(const UINT& compr_quality, cv::Mat& img)
{
    // 转换为灰度图

    if (compr_quality > 0 && compr_quality < 100) {
        compress_img_to_gray(img, compr_quality);
    }
    else if (compr_quality == 0) { // auto
        const auto bytes = img.total() * img.elemSize();
        const auto mb = bytes / 1024 / 1024;
        if (mb >= 5) {
            compress_img_to_gray(img, 90);
        }
        else {
            cv::cvtColor(img, img, cv::COLOR_BGR2GRAY);
        }
    }
    else {
        if (img.channels() > 1) cv::cvtColor(img, img, cv::COLOR_BGR2GRAY);
    }
}

size_t decode_mat_internal(cv::Mat& img, WRESULT* result_buffer, size_t buffer_size) {
    std::vector<cv::Mat> out;

    memset(result_buffer, 0, buffer_size); // 清空缓冲区

    const size_t max_count = buffer_size / sizeof(WRESULT);
    size_t count = 0;

    auto vs = wechatQRCode->detectAndDecode(img, out);
    count = vs.size();

    for (size_t i = 0; i < vs.size(); ++i)
    {
        if (i + 1 > max_count)
        {
            last_error = "more data";
            break;
        }

        auto result = result_buffer + i;

        auto& p_texts = vs[i];
        auto& p_scale = out[i];

        if (p_texts.empty())
        {
            continue;
        }

        result->data_len = p_texts.size();
        result->data = new char[result->data_len + 1];
        memcpy_s(result->data, result->data_len, p_texts.data(), p_texts.size());
        result->data[result->data_len] = '\0';

        cv::Point pt1 = cv::Point((int)p_scale.at<float>(0, 0), (int)p_scale.at<float>(0, 1));
        cv::Point pt2 = cv::Point((int)p_scale.at<float>(1, 0), (int)p_scale.at<float>(1, 1));
        cv::Point pt3 = cv::Point((int)p_scale.at<float>(2, 0), (int)p_scale.at<float>(2, 1));
        cv::Point pt4 = cv::Point((int)p_scale.at<float>(3, 0), (int)p_scale.at<float>(3, 1));

        result->points_len = 4;
        result->points = new POINTv2[result->points_len];
        result->points[0] = POINTv2(pt1.x, pt1.y);
        result->points[1] = POINTv2(pt2.x, pt2.y);
        result->points[2] = POINTv2(pt3.x, pt3.y);
        result->points[3] = POINTv2(pt4.x, pt4.y);
    }

    return count;
}


extern "C" __declspec(dllexport) int __stdcall init_model()
{
    last_error = "";
    if (wechatQRCode != nullptr)
    {
        return 0;
    }

    if (!PathFileExistsA(md_DetectPrototxt.c_str())) {
        last_error = md_DetectPrototxt + ": not found";
        return -1;
    }

    if (!PathFileExistsA(md_DetectCaffeModel.c_str())) {
        last_error = md_DetectCaffeModel + ": not found";
        return -2;
    }

    if (!PathFileExistsA(md_SrPrototxt.c_str())) {
        last_error = md_SrPrototxt + ": not found";
        return -3;
    }

    if (!PathFileExistsA(md_SrCaffeModel.c_str())) {
        last_error = md_SrCaffeModel + ": not found";
        return -4;
    }

    try {
        wechatQRCode = cv::makePtr<cv::wechat_qrcode::WeChatQRCode>(
            md_DetectPrototxt,
            md_DetectCaffeModel,
            md_SrPrototxt,
            md_SrCaffeModel
        );

        if (wechatQRCode == nullptr)
        {
            last_error = "create model failed";
            return -5;
        }

        wechatQRCode->setScaleFactor(1.0f);
    }
    catch (const std::exception& e) {
        last_error = e.what();
        return -5;
    }

    return 0;
}

extern "C" __declspec(dllexport) void release_model()
{
    last_error = "";
    if (wechatQRCode != nullptr)
        wechatQRCode.release();
}

extern "C" __declspec(dllexport) size_t __stdcall decode(const char* img_path, WRESULT * result_buffer, size_t buffer_size, UINT compr_quality = 100)
{
    last_error = "";
    if (wechatQRCode == nullptr)
    {
        last_error = "model not initialized";
        return 0;
    }

    if (img_path == nullptr)
    {
        last_error = "imgPath is null";
        return 0;
    }

    if (result_buffer == nullptr)
    {
        last_error = "result is null";
        return 0;
    }

    if (buffer_size < sizeof(WRESULT))
    {
        last_error = "buffer_size is too small";
        return 0;
    }

    // 读取图片
    cv::Mat img = cv::imread(img_path);
    if (img.empty())
    {
        last_error = "img is empty";
        return 0;
    }

    // 压缩图片
    cpr(compr_quality, img);

    size_t count = 0;

    try {
        count = decode_mat_internal(img, result_buffer, buffer_size);
    }
    catch (const std::exception& e) {
        last_error = e.what();
        count = 0;
    }

    return count;
}

extern "C" __declspec(dllexport) size_t __stdcall decode_bitmap(
    BYTE * imgBuffer, // 图像数据的指针
    uint width,       // 图像的宽度 
    uint height,      // 图像的高度
    uint stride,      // 图像的行跨度（每行的字节数）
    uint nChannels,   // 图像的通道数 
    WRESULT * result_buffer, // 识别结果的缓冲区
    size_t buffer_size      // 缓冲区的大小
)
{
    if (imgBuffer == nullptr)
    {
        last_error = "imgBuffer is null";
        return 0;
    }

    if (result_buffer == nullptr)
    {
        last_error = "result is null";
        return 0;
    }

    if (buffer_size < sizeof(WRESULT))
    {
        last_error = "buffer_size is too small";
        return 0;
    }

    if (width * height <= 0)
    {
        last_error = "width or height is zero";
        return 0;
    }

    if (nChannels <= 0)
    {
        last_error = "nChannels is zero";
        return 0;
    }

    // 从BYTE*创建cv::Mat对象
    cv::Mat img(height, width, CV_8UC(nChannels), imgBuffer, stride);

    // cpr(0, img); // auto compress

    // cv::imwrite(".cache\\bitmap.jpg", img);

    size_t count = 0;

    try {
        count = decode_mat_internal(img, result_buffer, buffer_size);
    }
    catch (const std::exception& e) {
        last_error = e.what();
        count = 0;
    }

    return count;
}

extern "C" __declspec(dllexport) int __stdcall get_last_error(char* err, int len)
{
    if (last_error.empty())
        return 0;

    if (err == nullptr || len == 0) return last_error.size();

    if (len < last_error.length()) return -1;

    memcpy_s(err, len, last_error.c_str(), last_error.length());
    return last_error.size();
}

float distanceOf(const cv::Point2f& p1, const cv::Point2f& p2)
{
    float dx = p1.x - p2.x;
    float dy = p1.y - p2.y;
    return sqrt(dx * dx + dy * dy);
}

struct ImageInfo {
    BYTE* imgBuffer;  // 图像数据的指针
    BYTE* imgResult;  // 识别结果的缓冲区
    size_t resultDataSize;  // 缓冲区的大小
    uint width;       // 图像的宽度
    uint height;      // 图像的高度
    uint stride;      // 图像的行跨度（每行的字节数）
    uint nChannels;   // 图像的通道数
};

struct ContourParameter {
    float area_threshold; // 最小边长, default: 1000
    float binarize;  // 二值化阈值: default: 225
    float canny_low; // canny 边缘检测的低阈值, default: 50
    float canny_high; // canny 边缘检测的高阈值, default: 100
};

void find_bbox(
    std::vector<std::vector<cv::Point>>& contours,
    cv::RotatedRect& rotatedRect,
    std::vector<cv::Point2f>& vertices,
    cv::Rect& bbox,
    const ContourParameter& param
)
{
    for (const auto& cnt : contours) {
        // 计算最小外接矩形
        rotatedRect = cv::minAreaRect(cnt);
        rotatedRect.points(vertices.data());

        // 获取矩形区域
        bbox = rotatedRect.boundingRect();

        auto sz = rotatedRect.size;
        if (sz.width < param.area_threshold || sz.height < param.area_threshold) continue;
        // 找到需要处理的区域
        break;
    }
}

bool rotate_crop_slice(cv::RotatedRect& rotatedRect, const cv::Mat& crop, cv::Mat& rotated)
{
    float angle = rotatedRect.angle;

    // 仿射变换
    if (std::abs(angle) > 0 && std::abs(angle) <= 45) {
        angle = angle;
    }
    else if (std::abs(angle) > 45 && std::abs(angle) < 90) {
        angle = angle - 90;
    }
    else
        angle = 0; // 无需旋转 

    if (angle == 0)
    {
        rotated = crop;
        return false; // no need re-process
    }

    // 获取旋转矩阵 
    cv::Mat rot_mat = cv::getRotationMatrix2D(rotatedRect.center, angle, 1.0);

    // 旋转图像 
    cv::warpAffine(crop, rotated, rot_mat, crop.size(), cv::INTER_LINEAR);

    return true; // need re-process
}

void save_to(cv::Mat& rotated, ImageInfo& info)
{
    // 将裁剪后的区域保存到 BYTE*
    std::vector<BYTE> buffer;
    cv::imencode(".png", rotated, buffer);

    info.resultDataSize = buffer.size(); // 保存结果的缓冲区大小  
    info.imgResult = new BYTE[info.resultDataSize]; // 保存结果的缓冲区  
    memcpy_s(info.imgResult, info.resultDataSize, buffer.data(), info.resultDataSize);
}

void draw_debug(std::vector<cv::Point2f>& vertices, cv::Mat& image, cv::RotatedRect& rotatedRect, cv::Rect& bbox)
{
    // 绘制矩形，用于调试
    std::vector<std::vector<cv::Point> > pts;
    pts.push_back(std::vector<cv::Point>(vertices.begin(), vertices.end()));

    // 绘制中心点
    cv::circle(image, rotatedRect.center, 5, cv::Scalar(0, 0, 255), 5);

    // 绘制第一个顶点
    cv::circle(image, vertices[0], 5, cv::Scalar(0, 0, 255), 5);
    std::string a("angle: " + std::to_string(rotatedRect.angle));
    cv::putText(image, a, cv::Point(vertices[0].x, vertices[0].y - 25), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 255), 2);

    cv::polylines(image, pts, true, cv::Scalar(0, 0, 255), 4);
    cv::rectangle(image, bbox, cv::Scalar(0, 255, 0), 2);

    std::string text = std::to_string(bbox.size().width) + "x" + std::to_string(bbox.size().height);
    cv::putText(image, text, cv::Point(bbox.x, bbox.y - 5), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 255), 2);

    cv::imwrite(".cache/contours.jpg", image);
}

void binarize(cv::Mat& image, cv::Mat& bin, const ContourParameter& param)
{
    cv::cvtColor(image, bin, cv::COLOR_BGR2GRAY); // 转换为灰度图像  
    cv::threshold(bin, bin, param.binarize, 255, cv::THRESH_BINARY); // 二值化 
}

size_t extract_contours_internal(cv::Mat& src, cv::Mat& image, ImageInfo& info, const ContourParameter& param)
{
    // std::ofstream ofs(".cache/internal_log.txt", std::ios::out | std::ios::trunc);

    cv::Mat edges;
    cv::Mat rotated;

    int processCount = 0; // 处理的轮数
    size_t count = 0;

    cv::RotatedRect rotatedRect;
    cv::Rect bbox;
    std::vector<cv::Point2f> vertices(4);
    std::vector<std::vector<cv::Point>> contours;

canny:
    cv::Canny(src, edges, param.canny_low, param.canny_high, 3);
    cv::findContours(edges, contours, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

    find_bbox(contours, rotatedRect, vertices, bbox, param);

    //ofs << "rotatedRect.angle: " << rotatedRect.angle << std::endl;
    //ofs << "bbox: " << bbox << std::endl;
    //ofs << "image: " << image.size() << std::endl;

    auto b_sz = bbox.size();
    auto i_sz = image.size();

    if (bbox.x < 0 || bbox.y < 0 || bbox.x + b_sz.width > i_sz.width || bbox.y + b_sz.height > i_sz.height) {
        bbox = cv::Rect(0, 0, i_sz.width, i_sz.height);
    }

    cv::Mat cropped = image(bbox); // 裁剪出的矩形区域

    // ofs << "cropped: " << cropped.size() << std::endl;
    bool reproc_required = rotate_crop_slice(rotatedRect, cropped, rotated); // 旋转（如果有必要）裁剪出的矩形区域   

    if (reproc_required && ++processCount <= 2) {
        binarize(rotated, src, param);
        image = rotated;
        goto canny;
    }

    save_to(rotated, info);
    draw_debug(vertices, image, rotatedRect, bbox);
    return ++count;
}

extern "C" __declspec(dllexport) size_t __stdcall prune(
    ImageInfo & info, // 图像信息
    const ContourParameter & param  // 参数 
)
{
    std::ofstream ofs(".cache/log.txt", std::ios::out | std::ios::trunc);

    cv::Mat image(info.height, info.width, CV_8UC(info.nChannels), info.imgBuffer, info.stride);

    cv::Mat bin;
    binarize(image, bin, param);

    // cv::imwrite(".cache/bin.jpg", bin);

    try {
        size_t count = extract_contours_internal(bin, image, info, param);
        return count;
    }
    catch (const std::exception& e) {
        ofs << e.what() << std::endl;
        return 0;
    }

    ofs.close();
}
