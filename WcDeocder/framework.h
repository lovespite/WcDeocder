#pragma once

#define WIN32_LEAN_AND_MEAN             // 从 Windows 头文件中排除极少使用的内容
// Windows 头文件
#include <windows.h>
#include <Shlwapi.h>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <opencv2/wechat_qrcode.hpp>

#pragma comment(lib, "Shlwapi.lib")