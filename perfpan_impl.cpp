/*

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifdef _WIN32
#include "avs/win.h"
#endif
#include "def.h"
#include <avisynth.h>
#include "math.h"
#include <stdio.h>
#include <stdint.h>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <string>

#include "perfpan_impl.h"

class algo {
    const BYTE* reference;
    const BYTE* current;
    int rowsize;
    int pitch;
    int height;
    int best_x;
    int best_y;
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    float best_match;
    float blank_threshold;
    IScriptEnvironment* env;
    std::unordered_map<int, float> scorecache;
    int frame;
    bool plot_scores;
    FILE* plotfile;
    int max_search;

    float compare_frame(int x, int y);
    void calculate_shifts_exhaustive(void);
    void calculate_shifts_gradient(void);

public:
    algo(const BYTE* reference, const BYTE* current, int pitch, int rowsize, int height, float blank_threshold, int max_search,
        int frame, bool plot_scores, IScriptEnvironment* env);
    ~algo();

    void calculate_shifts(void);
    int get_best_x(void) { return best_x; };
    int get_best_y(void) { return best_y; };
    float get_best_match(void) { return best_match; };
    int get_limit_flags(int x, int y);
};

algo::algo(const BYTE* _reference, const BYTE* _current, int _pitch, int _rowsize, int _height, 
    float _blank_threshold, int _max_search, int _frame, bool _plot_scores, IScriptEnvironment* _env) :
    reference(_reference), current(_current), pitch(_pitch), rowsize(_rowsize), height(_height),
    best_x(0), best_y(0), best_match(-100), blank_threshold(_blank_threshold), max_search(_max_search), 
    frame(_frame), plot_scores(_plot_scores), env(_env)
{
    min_x = -rowsize / 4;
    min_y = -height / 4;
    max_x = rowsize / 4;
    max_y = height / 4;
    if (plot_scores) {
        char plotfilename[100];
        sprintf(plotfilename, "frame%d.%s", frame, (max_search == -1 ? "plt" : "txt"));
        plotfile = fopen(plotfilename, "wt");
        if (plotfile == NULL) {
            env->ThrowError("PerfPan: plotfile can not be created");
        }
    }
    else {
        plotfile = NULL;
    }
}

algo::~algo() {
    if (plotfile != NULL) {
        fclose(plotfile);
    }
}

/*
returns limit flags for log file
*/
int algo::get_limit_flags(int x, int y)
{
    int res = 0;
    if (x == min_x + 1) res |= 0x01;
    if (x == max_x - 1) res |= 0x02;
    if (y == min_y + 1) res |= 0x04;
    if (y == max_y - 1) res |= 0x08;

    return(res);
}

/*
compares current frame to reference frame pixel by pixel
updates best match data if best match found
current frame is shifted by x and y before the comparison
*/
float algo::compare_frame(int x, int y)
{
    int score = 0;
    int current_blacks = 0;
    int current_whites = 0;
    int reference_blacks = 0;
    int reference_whites = 0;
    int total = 0;
    int max_height = height - abs(y);
    int max_width = rowsize - abs(x);
    const BYTE* current_ptr;
    const BYTE* reference_ptr;
    float match = -100;

    if (x > min_x && x < max_x && y > min_y && y < max_y) {
        int cachekey = y * rowsize + x;
        if (scorecache.find(cachekey) == scorecache.end()) {
            for (int cy = 0; cy < max_height; cy++) {
                current_ptr = current + (cy + (y > 0 ? 0 : -y)) * pitch + (x > 0 ? 0 : -x);
                reference_ptr = reference + (cy + (y > 0 ? y : 0)) * pitch + (x > 0 ? x : 0);
                for (int cx = 0; cx < max_width; cx++) {
                    BYTE current_pixel = *(current_ptr++);
                    BYTE reference_pixel = *(reference_ptr++);

                    current_blacks += (current_pixel == 0);
                    current_whites += (current_pixel == 255);
                    reference_blacks += (reference_pixel == 0);
                    reference_whites += (reference_pixel == 255);

                    if ((current_pixel != 0 && current_pixel != 255)
                        || (reference_pixel != 0 && reference_pixel != 255)) {
                        env->ThrowError("PerfPan: clip must be black and white. Use ConvrtToY8().Levels(160,1,161,0,255,true)");
                    }
                    // see the documentation for scoring logic and for those magical constants
                    score += (reference_pixel == 255 && current_pixel == 255) * 20
                        - (reference_pixel == 255 && current_pixel == 0) * 20
                        + (reference_pixel == 0 && current_pixel == 0)
                        - (reference_pixel == 0 && current_pixel == 255);
                    total++;
                }
            }

            int threshold = total * blank_threshold;

            // if either reference frame or current frame is blank - without any features that could
            // be used for syncing then we are not going to calculate the match
            if (current_blacks > threshold && current_whites > threshold
                && reference_blacks > threshold && reference_whites > threshold) {
                match = (float)score / total;
                if (match > best_match) {
                    best_x = x;
                    best_y = y;
                    best_match = match;
                }
            }
            scorecache[cachekey] = match;
        }
    }
    return(match);
}

void algo::calculate_shifts() {
    if (max_search == -1) {
        calculate_shifts_exhaustive();
    }
    else {
        calculate_shifts_gradient();
    }
}

/*
shifts current frame in spiral motion and compares with reference frame to find best match
shifts are done half frame up and down and half frame left and right
*/
void algo::calculate_shifts_exhaustive() {
    float min_match = 100;
    float max_match = -100;
    if (plotfile != NULL) {
        fprintf(plotfile, "$map << EOD\n");
    }
    for (int x = min_x; x < max_x; x++) {
        for (int y = min_y; y < max_y; y++) {
            float match = compare_frame(x, y);
            if (match > max_match) {
                max_match = match;
            }
            if (match != -100 && match < min_match) {
                min_match = match;
            }
            if (plotfile != NULL) {
                fprintf(plotfile, "%d\t%d\t%d\t%7.5f\n", frame, x, y, match);
            }
        }
        if (plotfile != NULL) {
            fprintf(plotfile, "\n");
        }
    }
    if (plotfile != NULL) {
        fprintf(plotfile, "EOD\n");
        fprintf(plotfile, "set cbrange[%f:%f]\n", min_match, max_match);
        fprintf(plotfile, "set view map\n");
        fprintf(plotfile, "plot '$map' using 2:3:4 with image\n");
    }
}

/*
shifts current frame to the direction where the match is best 
repeats until there is not better match around
shifts are done half frame up and down and half frame left and right
*/
void algo::calculate_shifts_gradient() {
    int x = 0;
    int y = 0;
    int current_search = 1;
    bool run = true;

    compare_frame(0, 0);
    do {
        // scan the square circle around x & y
        // increase radius every time best_x & best_y do not improve
        // stop after max search radius is achieved
        for (int cx = -current_search; cx <= current_search; cx++) {
            compare_frame(x + cx, y + current_search);
            compare_frame(x + cx, y - current_search);
        }
        for (int cy = -(current_search-1); cy <= current_search - 1; cy++) {
            compare_frame(x + current_search, y + cy);
            compare_frame(x - current_search, y + cy);
        }
        if (x != best_x || y != best_y) {
            // better score found, reset radius
            x = best_x;
            y = best_y;
            current_search = 1;
            if (plotfile != NULL) {
                fprintf(plotfile, "x,y = %d,%d\n", best_x, best_y);
            }
        }
        else if (current_search < max_search) {
            // no better score, look further
            current_search++;
            if (plotfile != NULL) {
                fprintf(plotfile, "r = %d\n", current_search);
            }
        }
        else {
            // we are done
            run = false;
        }
    } while (run);
}

PerfPan_impl::PerfPan_impl(PClip _child, PClip _perforation, float _blank_threshold, int _reference_frame, int _max_search,
    const char* _logfilename, bool _plot_scores, const char* _hintfilename, bool _copy_on_limit, IScriptEnvironment* env) :
    GenericVideoFilter(_child), perforation(_perforation), blank_threshold(_blank_threshold),
    reference_frame(_reference_frame), logfilename(_logfilename), max_search(_max_search),
    plot_scores(_plot_scores), hintfilename(_hintfilename), copy_on_limit(_copy_on_limit)
{
    has_at_least_v8 = true;
    try { env->CheckVersion(8); }
    catch (const AvisynthError&) { has_at_least_v8 = false; }

    if (!perforation->GetVideoInfo().IsY8()) {
        env->ThrowError("PerfPan: input must be Y8");
    }

    logfile = NULL;
    if (lstrlen(logfilename) > 0) {
        logfile = fopen(logfilename, "wt");
        if (logfile == NULL)    env->ThrowError("PerfPan: log file can not be created");
    }

    if (lstrlen(hintfilename) > 0) {
        std::ifstream infile(hintfilename);

        if (!infile.is_open()) {
            env->ThrowError("PerfPan: hint file can not be opened");
        }

        int frame;
        int x;
        int y;
        float match;
        int limit;

        while (infile >> frame >> x >> y >> match >> limit) {
            xhint[frame] = x;
            yhint[frame] = y;
        }
    }
}

PerfPan_impl::~PerfPan_impl() {
    if (lstrlen(logfilename) > 0 && logfile != NULL) {
        fclose(logfile);
    }
}

template<typename T>
T clamp(T n, T min, T max)
{
    n = n > max ? max : n;
    return n < min ? min : n;
}

static uint8_t ScaledPixelClip(int i) {
    // return PixelClip((i+32768) >> 16);
    // PF: clamp is faster than lut
    return (uint8_t)clamp((i + 32768) >> 16, 0, 255);
}

inline int RGB2YUV(int rgb) // limited range
{
    const int cyb = int(0.114 * 219 / 255 * 65536 + 0.5);
    const int cyg = int(0.587 * 219 / 255 * 65536 + 0.5);
    const int cyr = int(0.299 * 219 / 255 * 65536 + 0.5);

    // y can't overflow
    int y = (cyb * (rgb & 255) + cyg * ((rgb >> 8) & 255) + cyr * ((rgb >> 16) & 255) + 0x108000) >> 16;
    int scaled_y = (y - 16) * int(255.0 / 219.0 * 65536 + 0.5);
    int b_y = ((rgb & 255) << 16) - scaled_y;
    int u = ScaledPixelClip((b_y >> 10) * int(1 / 2.018 * 1024 + 0.5) + 0x800000);
    int r_y = (rgb & 0xFF0000) - scaled_y;
    int v = ScaledPixelClip((r_y >> 10) * int(1 / 1.596 * 1024 + 0.5) + 0x800000);
    return ((y * 256 + u) * 256 + v) | (rgb & 0xff000000);
}

static float uv8tof(int color) {
#ifdef FLOAT_CHROMA_IS_HALF_CENTERED
    const float shift = 0.5f;
#else
    const float shift = 0.0f;
#endif
    return (color - 128) / 255.0f + shift;
}

// 8 bit fullscale to float
static AVS_FORCEINLINE float c8tof(int color) {
    return color / 255.0f;
}

template<typename pixel_t>
static inline pixel_t GetHbdColorFromByte(uint8_t color, bool fullscale, int bits_per_pixel, bool chroma)
{
    if constexpr (sizeof(pixel_t) == 1) return color;
    else if constexpr (sizeof(pixel_t) == 2) return (pixel_t)(fullscale ? (color * ((1 << bits_per_pixel) - 1)) / 255 : (int)color << (bits_per_pixel - 8));
    else {
        if (chroma)
            return (pixel_t)uv8tof(color);  // float, scale, 128=0.0f
        else
            return (pixel_t)c8tof(color); // float, scale to [0..1]
    }
}

template<typename pixel_t>
static void addborders_planar(PVideoFrame& dst, PVideoFrame& src, VideoInfo& vi, int xpan, int ypan, int color, bool isYUV, 
    bool force_color_as_yuv, int bits_per_pixel, IScriptEnvironment* env)
{
    const unsigned int colr = isYUV && !force_color_as_yuv ? RGB2YUV(color) : color;
    // const unsigned int colr = color;
    const unsigned char YBlack = (unsigned char)((colr >> 16) & 0xff);
    const unsigned char UBlack = (unsigned char)((colr >> 8) & 0xff);
    const unsigned char VBlack = (unsigned char)((colr) & 0xff);
    const unsigned char ABlack = (unsigned char)((colr >> 24) & 0xff);

    int planesYUV[4] = { PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A };
    int planesRGB[4] = { PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A };
    int* planes = isYUV ? planesYUV : planesRGB;
    uint8_t colorsYUV[4] = { YBlack, UBlack, VBlack, ABlack };
    uint8_t colorsRGB[4] = { UBlack, VBlack, YBlack, ABlack }; // mapping for planar RGB
    uint8_t* colors = isYUV ? colorsYUV : colorsRGB;
    for (int p = 0; p < vi.NumComponents(); p++)
    {
        int plane = planes[p];
        int xsub = vi.GetPlaneWidthSubsampling(plane);
        int ysub = vi.GetPlaneHeightSubsampling(plane);

        int src_pitch = src->GetPitch(plane);
        int src_rowsize = src->GetRowSize(plane) - vi.BytesFromPixels(abs(xpan) >> xsub);
        int src_height = src->GetHeight(plane) - (abs(ypan) >> ysub);
        const BYTE* src_readptr = src->GetReadPtr(plane) + ((ypan < 0 ? -ypan : 0) >> ysub) * src_pitch
            + vi.BytesFromPixels((xpan < 0 ? -xpan : 0) >> xsub);

        int dst_pitch = dst->GetPitch(plane);

        const int initial_black = ((ypan > 0 ? ypan : 0) >> ysub) * dst_pitch 
            + vi.BytesFromPixels((xpan > 0 ? xpan : 0) >> xsub);
        const int middle_black = dst_pitch - src_rowsize;
        const int final_black = ((ypan < 0 ? -ypan : 0) >> ysub) * dst_pitch 
            + vi.BytesFromPixels((xpan < 0 ? -xpan : 0) >> xsub) 
            + (dst_pitch - dst->GetRowSize(plane));

        const bool chroma = plane == PLANAR_U || plane == PLANAR_V;

        pixel_t current_color = GetHbdColorFromByte<pixel_t>(colors[p], !isYUV, bits_per_pixel, chroma);

        BYTE* dstp = dst->GetWritePtr(plane);
        // copy original
        env->BitBlt(dstp + initial_black, dst_pitch, src_readptr, src_pitch, src_rowsize, src_height);
        // add top
        for (size_t a = 0; a < initial_black / sizeof(pixel_t); a++) {
            reinterpret_cast<pixel_t*>(dstp)[a] = current_color;
        }
        // middle right + left (fill overflows from right to left)
        dstp += initial_black + src_rowsize;
        for (int y = src_height - 1; y > 0; --y) {
            for (size_t b = 0; b < middle_black / sizeof(pixel_t); b++) {
                reinterpret_cast<pixel_t*>(dstp)[b] = current_color;
            }
            dstp += dst_pitch;
        }
        // bottom
        for (size_t c = 0; c < final_black / sizeof(pixel_t); c++)
            reinterpret_cast<pixel_t*>(dstp)[c] = current_color;
    }
}

PVideoFrame __stdcall PerfPan_impl::GetFrame(int ndest, IScriptEnvironment* env) 
{
    PVideoFrame current = perforation->GetFrame(ndest, env);
    PVideoFrame reference = perforation->GetFrame(reference_frame, env);

    int xpan;
    int ypan;

    if (xhint.find(ndest) == xhint.end() || yhint.find(ndest) == yhint.end()) {
        algo algo(reference->GetReadPtr(), current->GetReadPtr(), reference->GetPitch(),
            reference->GetRowSize(), reference->GetHeight(), blank_threshold, max_search, ndest, plot_scores, env);
        algo.calculate_shifts();

        xpan = algo.get_best_x();
        ypan = algo.get_best_y();

        int limit_flags = algo.get_limit_flags(xpan, ypan);

        /*
        frame was shifted to its limits. in practice there are two cases when this happens:
        
        a) compare window was too small - more panning was really needed. 
        one should increase the compare window to get good shift automatically

        b) the frame was low quality and then calculated shift is not correct. in practice it's 
        good strategy to use shift values from last frame. you can enable this with copy_on_limit option
        */
        if (limit_flags != 0 && copy_on_limit && xhint.find(ndest-1) != xhint.end() 
                && yhint.find(ndest-1) != yhint.end()) {
            xpan = xhint[ndest-1];
            ypan = yhint[ndest-1];
        }
        /* store values so they can used for next frame if needed */
        xhint[ndest] = xpan;
        yhint[ndest] = ypan;
        if (logfile != NULL) {
            fprintf(logfile, " %6d %4d %4d %7.5f %d\n", ndest, xpan, ypan, algo.get_best_match(), limit_flags);
        }
    }
    else {
        xpan = xhint[ndest];
        ypan = yhint[ndest];
    }

    bool force_color_as_yuv = false;
    int clr = 0x00FF00;

    if (vi.IsYUV() || vi.IsYUVA()) {
        int xsub = 0;
        int ysub = 0;
        if (vi.NumComponents() > 1) {
            xsub = vi.GetPlaneWidthSubsampling(PLANAR_U);
            ysub = vi.GetPlaneHeightSubsampling(PLANAR_U);
        }

        const int xmask = (1 << xsub) - 1;
        const int ymask = (1 << ysub) - 1;

        // YUY2, etc, ... can only add even amounts
        if (xpan > 0 && (xpan & xmask)) {
            xpan &= ~xmask;
        }
        if (xpan < 0 && (-xpan & xmask)) {
            xpan = -(-xpan & ~xmask);
        }
        if (ypan > 0 && (ypan & ymask)) {
            ypan &= ~ymask;
        }
        if (ypan < 0 && (-ypan & ymask)) {
            ypan = - (-ypan & ~ymask);
        }
    }
    else if (!vi.IsPlanarRGB() && !vi.IsPlanarRGBA()) {
        // RGB is upside-down
        ypan = -ypan;
    }

    PVideoFrame src = child->GetFrame(ndest, env);
    PVideoFrame dst = env->NewVideoFrameP(vi, &src);

    if (vi.IsPlanar()) {
        int bits_per_pixel = vi.BitsPerComponent();
        bool isYUV = vi.IsYUV() || vi.IsYUVA();
        switch (vi.ComponentSize()) {
        case 1: addborders_planar<uint8_t>(dst, src, vi, xpan, ypan, clr, isYUV, force_color_as_yuv /*like MODE_COLOR_YUV in BlankClip */, bits_per_pixel, env); break;
        case 2: addborders_planar<uint16_t>(dst, src, vi, xpan, ypan, clr, isYUV, force_color_as_yuv, bits_per_pixel, env); break;
        default: //case 4:
            addborders_planar<float>(dst, src, vi, xpan, ypan, clr, isYUV, force_color_as_yuv, bits_per_pixel, env); break;
        }
        return dst;
    }

    const int src_pitch = src->GetPitch();
    const int dst_pitch = dst->GetPitch();
    const int src_row_size = src->GetRowSize() - vi.BytesFromPixels(abs(xpan));
    const int dst_row_size = dst->GetRowSize();
    const int src_height = src->GetHeight() - abs(ypan);
    const BYTE* srcp = src->GetReadPtr() + (ypan < 0 ? -ypan : 0) * src_pitch + vi.BytesFromPixels(xpan < 0 ? -xpan : 0);
    BYTE* dstp = dst->GetWritePtr();

    const int initial_black = (ypan > 0 ? ypan : 0) * dst_pitch + vi.BytesFromPixels(xpan > 0 ? xpan : 0);
    const int middle_black = dst_pitch - src_row_size;
    const int final_black = (ypan < 0 ? -ypan : 0) * dst_pitch + vi.BytesFromPixels(xpan < 0 ? -xpan : 0)
        + (dst_pitch - dst_row_size);

    if (vi.IsYUY2()) {
        const unsigned int colr = force_color_as_yuv ? clr : RGB2YUV(clr);
        // const unsigned int colr = clr;
        const uint32_t black = (colr >> 16) * 0x010001 + ((colr >> 8) & 255) * 0x0100 + (colr & 255) * 0x01000000;

        env->BitBlt(dstp + initial_black, dst_pitch, srcp, src_pitch, src_row_size, src_height);
        for (int a = 0; a < initial_black; a += 4) {
            *(uint32_t*)(dstp + a) = black;
        }
        dstp += initial_black + src_row_size;
        for (int y = src_height - 1; y > 0; --y) {
            for (int b = 0; b < middle_black; b += 4) {
                *(uint32_t*)(dstp + b) = black;
            }
            dstp += dst_pitch;
        }
        for (int c = 0; c < final_black; c += 4) {
            *(uint32_t*)(dstp + c) = black;
        }
    }
    else if (vi.IsRGB24()) {
        const unsigned char  clr0 = (unsigned char)(clr & 0xFF);
        const unsigned short clr1 = (unsigned short)(clr >> 8);
        const int leftbytes = vi.BytesFromPixels(xpan > 0 ? xpan : 0);
        const int leftrow = src_row_size + leftbytes;
        const int rightbytes = vi.BytesFromPixels(xpan < 0 ? -xpan : 0);
        const int rightrow = dst_pitch - dst_row_size + rightbytes;

        env->BitBlt(dstp + initial_black, dst_pitch, srcp, src_pitch, src_row_size, src_height);
        /* Cannot use *_black optimisation as pitch may not be mod 3 */
        for (int y = (ypan > 0 ? ypan : 0); y > 0; --y) {
            for (int i = 0; i < dst_row_size; i += 3) {
                dstp[i] = clr0;
                *(uint16_t*)(dstp + i + 1) = clr1;
            }
            dstp += dst_pitch;
        }
        for (int y = src_height; y > 0; --y) {
            for (int i = 0; i < leftbytes; i += 3) {
                dstp[i] = clr0;
                *(uint16_t*)(dstp + i + 1) = clr1;
            }
            dstp += leftrow;
            for (int i = 0; i < rightbytes; i += 3) {
                dstp[i] = clr0;
                *(uint16_t*)(dstp + i + 1) = clr1;
            }
            dstp += rightrow;
        }
        for (int y = (ypan < 0 ? -ypan : 0); y > 0; --y) {
            for (int i = 0; i < dst_row_size; i += 3) {
                dstp[i] = clr0;
                *(uint16_t*)(dstp + i + 1) = clr1;
            }
            dstp += dst_pitch;
        }
    }
    else if (vi.IsRGB32()) {
        env->BitBlt(dstp + initial_black, dst_pitch, srcp, src_pitch, src_row_size, src_height);
        for (int i = 0; i < initial_black; i += 4) {
            *(uint32_t*)(dstp + i) = clr;
        }
        dstp += initial_black + src_row_size;
        for (int y = src_height - 1; y > 0; --y) {
            for (int i = 0; i < middle_black; i += 4) {
                *(uint32_t*)(dstp + i) = clr;
            }
            dstp += dst_pitch;
        } // for y
        for (int i = 0; i < final_black; i += 4) {
            *(uint32_t*)(dstp + i) = clr;
        }
    }
    else if (vi.IsRGB48()) {
        const uint16_t  clr0 = GetHbdColorFromByte<uint16_t>(clr & 0xFF, true, 16, false);
        uint32_t clr1 =
            ((uint32_t)GetHbdColorFromByte<uint16_t>((clr >> 16) & 0xFF, true, 16, false) << (8 * 2)) +
            ((uint32_t)GetHbdColorFromByte<uint16_t>((clr >> 8) & 0xFF, true, 16, false));
        const int leftbytes = vi.BytesFromPixels(xpan > 0 ? xpan : 0);
        const int leftrow = src_row_size + leftbytes;
        const int rightbytes = vi.BytesFromPixels(xpan < 0 ? -xpan : 0);
        const int rightrow = dst_pitch - dst_row_size + rightbytes;

        env->BitBlt(dstp + initial_black, dst_pitch, srcp, src_pitch, src_row_size, src_height);
        /* Cannot use *_black optimisation as pitch may not be mod 3 */
        for (int y = (ypan > 0 ? ypan : 0); y > 0; --y) {
            for (int i = 0; i < dst_row_size; i += 6) {
                *(uint16_t*)(dstp + i) = clr0;
                *(uint32_t*)(dstp + i + 2) = clr1;
            }
            dstp += dst_pitch;
        }
        for (int y = src_height; y > 0; --y) {
            for (int i = 0; i < leftbytes; i += 6) {
                *(uint16_t*)(dstp + i) = clr0;
                *(uint32_t*)(dstp + i + 2) = clr1;
            }
            dstp += leftrow;
            for (int i = 0; i < rightbytes; i += 6) {
                *(uint16_t*)(dstp + i) = clr0;
                *(uint32_t*)(dstp + i + 2) = clr1;
            }
            dstp += rightrow;
        }
        for (int y = (ypan < 0 ? -ypan : 0); y > 0; --y) {
            for (int i = 0; i < dst_row_size; i += 6) {
                *(uint16_t*)(dstp + i) = clr0;
                *(uint32_t*)(dstp + i + 2) = clr1;
            }
            dstp += dst_pitch;
        }
    }
    else if (vi.IsRGB64()) {
        env->BitBlt(dstp + initial_black, dst_pitch, srcp, src_pitch, src_row_size, src_height);

        uint64_t clr64 =
            ((uint64_t)GetHbdColorFromByte<uint16_t>((clr >> 24) & 0xFF, true, 16, false) << (24 * 2)) +
            ((uint64_t)GetHbdColorFromByte<uint16_t>((clr >> 16) & 0xFF, true, 16, false) << (16 * 2)) +
            ((uint64_t)GetHbdColorFromByte<uint16_t>((clr >> 8) & 0xFF, true, 16, false) << (8 * 2)) +
            ((uint64_t)GetHbdColorFromByte<uint16_t>((clr) & 0xFF, true, 16, false));

        for (int i = 0; i < initial_black; i += 8) {
            *(uint64_t*)(dstp + i) = clr64;
        }
        dstp += initial_black + src_row_size;
        for (int y = src_height - 1; y > 0; --y) {
            for (int i = 0; i < middle_black; i += 8) {
                *(uint64_t*)(dstp + i) = clr64;
            }
            dstp += dst_pitch;
        } // for y
        for (int i = 0; i < final_black; i += 8) {
            *(uint64_t*)(dstp + i) = clr64;
        }
    }

    return dst;
}
