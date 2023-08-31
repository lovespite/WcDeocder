using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

using static wcd.net.LibWcdNative;

namespace wcd.net
{
    internal class Program
    {
        static void Main(string[] args)
        {
            if (args.Length == 0)
            {
                PrintUsage();
                return;
            }

            var ret = init_model();
            if (ret != 0)
            {
                Console.WriteLine("init_model failed: {0}", GetLastErrorv2());
                return;
            }

            var mode = args[0];

            try
            {

                switch (mode)
                {
                    case "-f":
                        if (args.Length < 2)
                        {
                            Console.WriteLine("Usage: wcd.net -f <img_path>");
                            break;
                        }
                        ScanFile(args[1]);
                        break;

                    case "-u":
                        if (args.Length < 2)
                        {
                            Console.WriteLine("Usage: wcd.net -u <url>");
                            break;
                        }
                        var url = args[1];
                        using (var bmp = DownloadUrlImg(url))
                            ScanBitmap(bmp);
                        break;

                    case "-c":
                        using (var bmp2 = CaptureScreen())
                            ScanBitmap(bmp2);
                        break;

                    default:
                        PrintUsage();
                        break;
                }
            }
            catch (Exception ex)
            {
                Console.ForegroundColor = ConsoleColor.Red;
                Console.WriteLine(ex);
                Console.ResetColor();
            }
            finally
            {
                release_model();
            }

#if DEBUG
            Console.ReadKey();
#endif

            void PrintUsage()
            {
                Console.WriteLine("Usage: wcd.net -f <img_path>");
                Console.WriteLine("       wcd.net -u <url>");
                Console.WriteLine("       wcd.net -c");
            }
        }

        public static void ScanFile(string f)
        {
            if (!System.IO.File.Exists(f)) throw new System.IO.FileNotFoundException(f);
            var ret = Decode(f);
            if (ret.Length == 0)
            {
                Console.WriteLine("No result");
                return;
            }
            for (var i = 0; i < ret.Length; i++)
            {
                var r = ret[i];
                Console.WriteLine("({0}) {1}", i, r.Data);
            }
        }

        public static void ScanBitmap(System.Drawing.Bitmap img)
        {
            using (var copy = img.Clone(new System.Drawing.Rectangle(0, 0, img.Width, img.Height), System.Drawing.Imaging.PixelFormat.Format24bppRgb))
            {
                var ret = DecodeBitmap(copy);
                if (ret.Length == 0)
                {
                    Console.WriteLine("No result");
                    return;
                }
                for (var i = 0; i < ret.Length; i++)
                {
                    var r = ret[i];
                    Console.WriteLine("({0}) {1}", i, r.Data);
                }
            }
        }

        public static Bitmap DownloadUrlImg(string url)
        {
            var req = System.Net.WebRequest.Create(url);
            using (var resp = req.GetResponse())
            {
                using (var stream = resp.GetResponseStream())
                {
                    return new Bitmap(stream);
                }
            }
        }

        public static Bitmap CaptureScreen()
        {
            var bmp = new Bitmap(System.Windows.Forms.Screen.PrimaryScreen.Bounds.Width, System.Windows.Forms.Screen.PrimaryScreen.Bounds.Height, System.Drawing.Imaging.PixelFormat.Format24bppRgb);
            using (var g = Graphics.FromImage(bmp))
            {
                g.CopyFromScreen(0, 0, 0, 0, bmp.Size);
            }
            return bmp;
        }
    }
}
