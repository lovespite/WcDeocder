using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Linq;
using System.Net;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace wcd.net
{
    public class LibWcdNative
    {
        const string NativeDllName = "libwcd0.dll";

        [StructLayout(LayoutKind.Sequential)]
        public struct POINTv2
        {
            public int x;
            public int y;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct WRESULT : IDisposable
        {
            public static readonly int StructSize = Marshal.SizeOf<WRESULT>();
            public uint data_len;
            public uint points_len;
            public IntPtr points;
            public IntPtr data;

            public string Data
            {
                get
                {
                    if (data == IntPtr.Zero)
                        return string.Empty;
                    byte[] bytes = new byte[data_len];
                    Marshal.Copy(data, bytes, 0, (int)data_len);
                    return Encoding.ASCII.GetString(bytes);
                }
            }

            public POINTv2[] Points
            {
                get
                {
                    if (points == IntPtr.Zero)
                        return new POINTv2[0];
                    POINTv2[] pts = new POINTv2[points_len];
                    IntPtr p = points;
                    for (int i = 0; i < points_len; i++)
                    {
                        pts[i] = (POINTv2)Marshal.PtrToStructure(p, typeof(POINTv2));
                        p = new IntPtr(p.ToInt64() + Marshal.SizeOf(typeof(POINTv2)));
                    }
                    return pts;
                }
            }

            public void Dispose()
            {
                if (points != IntPtr.Zero)
                {
                    Marshal.FreeHGlobal(points);
                    points = IntPtr.Zero;
                }
                if (data != IntPtr.Zero)
                {
                    Marshal.FreeHGlobal(data);
                    data = IntPtr.Zero;
                }
            }
        }


        [DllImport(NativeDllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int init_model();

        [DllImport(NativeDllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void release_model();

        [DllImport(NativeDllName, EntryPoint = "get_last_error", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern int _get_last_error(IntPtr pMsg, int len);

        public static string GetLastErrorv2()
        {
            int len = _get_last_error(IntPtr.Zero, 0);
            IntPtr pMsg = Marshal.AllocHGlobal(len);
            _get_last_error(pMsg, len);
            string msg = Marshal.PtrToStringAnsi(pMsg, len);
            Marshal.FreeHGlobal(pMsg);
            return msg;
        }

        [DllImport(NativeDllName, EntryPoint = "decode", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern ulong decode(string img_path, IntPtr pResult, ulong bufferSize, uint cpr_quality = 100);

        public static WRESULT[] Decode(string img_path, uint cpr_quality = 0)
        {
            const int count = 288; // 288 is the max count of results = 72 * 4  
            int bufferSize = count * WRESULT.StructSize;

            IntPtr pResult = Marshal.AllocHGlobal(bufferSize);
            long lpResult = pResult.ToInt64();
            if (pResult == IntPtr.Zero) throw new Exception("Failed to allocate memory for results");

            try
            {
                var ret = (long)decode(img_path, pResult, (ulong)bufferSize, cpr_quality);

                if (ret == 0)
                {
                    var err = GetLastErrorv2();
                    if (!string.IsNullOrEmpty(err)) throw new Exception(GetLastErrorv2());
                    return Array.Empty<WRESULT>();
                }

                WRESULT[] results = new WRESULT[ret];
                for (long i = 0; i < ret; i++)
                {
                    results[i] = Marshal.PtrToStructure<WRESULT>(new IntPtr(lpResult + WRESULT.StructSize * i));
                }
                return results;
            }
            finally
            {

                Marshal.FreeHGlobal(pResult);
            }

        }

        [DllImport(NativeDllName, EntryPoint = "decode_bitmap", CallingConvention = CallingConvention.Cdecl)]
        public static extern ulong decode_bitmap(IntPtr pBitmap, uint width, uint height, uint stride, uint nChannels, IntPtr pResult, ulong bufferSize);

        public static WRESULT[] DecodeBitmap(System.Drawing.Bitmap img)
        {
            const int count = 288; // 288 is the max count of results = 72 * 4
            int bufferSize = count * WRESULT.StructSize;
            IntPtr pResult = Marshal.AllocHGlobal(bufferSize);
            long lpResult = pResult.ToInt64();

            if (pResult == IntPtr.Zero) throw new Exception("Failed to allocate memory for results");

            var width = (uint)img.Width;
            var height = (uint)img.Height;

            var data = img.LockBits(
                new System.Drawing.Rectangle(0, 0, img.Width, img.Height),
                ImageLockMode.ReadWrite,
                img.PixelFormat
            );

            try
            {
                var stride = (uint)data.Stride;

                var channels = GetImageChannelCount(img.PixelFormat);


                var ret = (long)decode_bitmap(data.Scan0, width, height, stride, channels, pResult, (ulong)bufferSize);

                var results = new WRESULT[ret];
                for (long i = 0; i < ret; i++)
                {
                    results[i] = Marshal.PtrToStructure<WRESULT>(new IntPtr(lpResult + WRESULT.StructSize * i));
                }

                return results;
            }
            finally
            {
                Marshal.FreeHGlobal(pResult);
                img.UnlockBits(data);
            }
        }

        public static uint GetImageChannelCount(PixelFormat format)
        {
            switch (format)
            {
                case PixelFormat.Format24bppRgb:
                    return 3;

                case PixelFormat.Format32bppArgb:
                case PixelFormat.Format32bppPArgb:
                case PixelFormat.Format32bppRgb:
                    return 4;

                case PixelFormat.Format1bppIndexed:
                case PixelFormat.Format4bppIndexed:
                case PixelFormat.Format8bppIndexed:
                    return 1;

                default:
                    return 3;
            }
        }

        [DllImport(NativeDllName, EntryPoint = "prune", CallingConvention = CallingConvention.Cdecl)]
        public static extern ulong prune(ref ImageInfo imgInfo, ref ContourParameter parameters);

        [StructLayout(LayoutKind.Sequential)]
        public struct CRESULT
        {
            public uint size;
            public IntPtr data;

            public static readonly int StructSize = Marshal.SizeOf<CRESULT>();
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct ImageInfo : IDisposable
        {
            public IntPtr imgBuffer;     // 图像数据的指针
            public IntPtr imgResult;     // 识别结果的缓冲区
            public ulong resultDataSize;  // 缓冲区的大小
            public uint width;           // 图像的宽度
            public uint height;          // 图像的高度
            public uint stride;          // 图像的行跨度（每行的字节数）
            public uint nChannels;       // 图像的通道数

            public void Dispose()
            {
                Marshal.FreeHGlobal(imgResult);
            }
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct ContourParameter
        {
            public float area_threshold; // 最小边长, default: 1000
            public float binarize;       // 二值化阈值: default: 225
            public float canny_low;      // canny 边缘检测的低阈值, default: 50
            public float canny_high;     // canny 边缘检测的高阈值, default: 100 

            public static ContourParameter Default => new ContourParameter
            {
                area_threshold = 1150,
                binarize = 225,
                canny_low = 50,
                canny_high = 100
            };
        }

        public static Bitmap Prune(Bitmap img, ContourParameter? parameters = null)
        {
            var width = (uint)img.Width;
            var height = (uint)img.Height;

            var data = img.LockBits(
                new Rectangle(0, 0, img.Width, img.Height),
                ImageLockMode.ReadWrite,
                img.PixelFormat
            );

            ImageInfo image = new ImageInfo()
            {
                imgBuffer = data.Scan0,
                width = width,
                height = height,
            };
            try
            {
                image.stride = (uint)data.Stride;
                image.nChannels = GetImageChannelCount(img.PixelFormat);

                ContourParameter p = parameters ?? ContourParameter.Default;

                var ret = (long)prune(ref image, ref p);

                if (ret <= 0) return null;

                var dataSize = image.resultDataSize;
                if (dataSize <= 0) return null;
                if (image.imgResult == IntPtr.Zero) return null;

                var buffer = new byte[dataSize];
                Marshal.Copy(image.imgResult, buffer, 0, (int)dataSize);

                using (var ms = new MemoryStream(buffer))
                {
                    var rstImg = Image.FromStream(ms);
                    return new Bitmap(rstImg);
                }
            }
            finally
            {
                image.Dispose();
                img.UnlockBits(data);
            }
        }
    }
}
