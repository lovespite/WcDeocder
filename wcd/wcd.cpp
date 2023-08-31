// wcd.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream> 
#include <Windows.h>
#include <Shlwapi.h>

#pragma comment(lib, "Shlwapi.lib")

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

extern "C" __declspec(dllimport) int init_model();
extern "C" __declspec(dllimport) void release_model();
extern "C" __declspec(dllimport) int get_last_error(char* buf, int len);
extern "C" __declspec(dllimport) size_t __stdcall decode(const char* img_path, WRESULT * result_buffer, size_t buffer_size, UINT compr_quality = 100);

std::string GetLastErrorv2()
{
    int len = get_last_error(nullptr, 0);
    if (len <= 0) return "";

    char* buf = new char[len + 1];
    memset(buf, 0, len + 1);
    get_last_error(buf, len + 1);
    std::string ret(buf);
    delete[] buf;
    return ret;
}

std::string GetCurrentDir() {
    char buf[MAX_PATH] = { 0 };
    GetCurrentDirectoryA(MAX_PATH, buf); //获取当前目录
    return std::string(buf);
}

void SetCurrentDir(std::string dir) {
    SetCurrentDirectoryA(dir.c_str());
}

int main(int argc, char* argv[])
{
    std::cout << "wcd v1.0 Copyright Lovespite\n";

    int ret = init_model();
    auto c_dir = GetCurrentDir();

    // std::cout << "current dir: " << c_dir << std::endl;

    if (ret != 0)
    {
        std::cout << "init model failed: " << GetLastErrorv2() << std::endl;
        return 0;
    }
    else {
        std::cout << "init model success" << std::endl;
    }

    if (argc <= 1) {
        std::cout << "Usage: wcd <image_path> [-o output_file]\n";
        return 0;
    }

    std::string path = argv[1];
    std::string output = argc == 4 ? argv[3] : "";

    if (!PathFileExistsA(path.c_str())) {
        std::cout << "image file not exists: " << path << std::endl;
        return 0;
    }

    const int max_count = 288; // 72 * 4
    WRESULT* result = new WRESULT[max_count];
    size_t buffer_size = sizeof(WRESULT) * max_count;

    std::cout << "[" << path << "]" << std::endl;
    auto ret2 = decode(path.c_str(), result, buffer_size);

    if (ret2 < 0) {
        std::cout << "decode failed: " << GetLastErrorv2() << std::endl;
    }
    else if (ret2 == 0) {
        std::cout << "decode failed: no data" << std::endl;
    }
    else {
        std::cout << "decode success: " << ret2 << std::endl;
        std::cout << "output file: " << output << std::endl;

        FILE* fp = nullptr;

        if (!output.empty()) fopen_s(&fp, output.c_str(), "w");

        for (int i = 0; i < ret2; i++) {
            auto& r = result[i];
            std::cout << "(" << (i + 1) << ") " << r.data << std::endl;

            if (fp != nullptr) fprintf(fp, "(%i) %s\n", i + 1, r.data);

            for (UINT j = 0; j < r.points_len; j++) {
                auto& p = r.points[j];
                std::cout << " - [" << (j + 1) << "] (" << p.x << ", " << p.y << ") " << std::endl;

                if (fp != nullptr) fprintf(fp, " - %d %d\n", p.x, p.y);
            }

            r.release();
        }

        if (fp != nullptr)  fclose(fp);
    }

    delete[] result;

    release_model();
    // std::system("pause");
    return 0;
}
