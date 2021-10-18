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

#include "depanio.h"
#include "info.h"
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
    int debug;

    //bool isblank(const BYTE* frame, int x, int y);
    void compare_frame(int x, int y);
    //void shift_and_process_frame(int x, int y);

public:
    algo(const BYTE* _reference, const BYTE* _current, int pitch, int rowsize, int height, float blank_threshold,
        int debug, IScriptEnvironment* env);

    void calculate_shifts(void);
    int get_best_x(void) { return best_x; };
    int get_best_y(void) { return best_y; };
    float get_best_match(void) { return best_match; };
};

algo::algo(const BYTE* _reference, const BYTE* _current, int _pitch, int _rowsize, int _height, 
    float _blank_threshold, int _debug, IScriptEnvironment* _env) :
    reference(_reference), current(_current), pitch(_pitch), rowsize(_rowsize), height(_height),
    best_x(0), best_y(0), best_match(1), blank_threshold(_blank_threshold), debug(_debug), env(_env)
{
    min_x = -rowsize / 4;
    min_y = -height / 4;
    max_x = rowsize / 4;
    max_y = height / 4;
}

#if 0
/*
blank means frame is completely black or white. blank_threshold allows some error
x & y are shifts of frame - how many pixels are not checked.
when x is positive then pixels at the end of line are not checked
when x is negative then pixels at the beginning of line are not checked
when y is positive then lines at the end of frame are not checked
when y is negative then lines at the beginning of frame are not checked
if there is pixel that is 0 or 255 then processing is aborted
*/
bool algo::isblank(const BYTE* frame, int x, int y)
{
    int blacks = 0;
    int whites = 0;

    for (int cy = y < 0 ? -y : 0; cy < height - (y > 0 ? y : 0); cy++) {
        for (int cx = x < 0 ? -x : 0; cx < rowsize - (x > 0 ? x : 0); cx++) {
            BYTE pixel = *(frame + cy * pitch + cx);

            if (pixel == 0) {
                blacks++;
            }
            else if (pixel == 255) {
                whites++;
            }
            else {
                env->ThrowError("PerfPan: clip must be black and white. Use ConvrtToY8().Levels(160,1,161,0,255)");
            }
        }
    }

    return ((float)blacks / (blacks + whites) < blank_threshold 
        && (float)whites / (blacks + whites) < blank_threshold);
}
#endif

/*
compares current frame to reference frame pixel by pixel by xor-ing them. white means that pixels differ
returns the ratio of white pixels to total pixels
current frame is shifted by x and y before the comparison
*/
void algo::compare_frame(int x, int y)
{
    int mismatches = 0;
    int current_blacks = 0;
    int current_whites = 0;
    int reference_blacks = 0;
    int reference_whites = 0;
    int total = 0;
    int max_height = height - abs(y);
    int max_width = rowsize - abs(x);
    const BYTE* current_ptr;
    const BYTE* reference_ptr;

    if (x > min_x && x < max_x && y > min_y && y < max_y) {
        for (int cy = 0; cy < max_height; cy++) {
            current_ptr = current + (cy + (y > 0 ? 0 : -y)) * pitch + (x > 0 ? 0 : -x);
            reference_ptr = reference + (cy + (y > 0 ? y : 0)) * pitch + (x > 0 ? x : 0);
            for (int cx = 0; cx < max_width; cx++) {
                BYTE current_pixel = *(current_ptr++);
                BYTE reference_pixel = *(reference_ptr++);

                if (current_pixel == 0) {
                    current_blacks++;
                }
                else if (current_pixel == 255) {
                    current_whites++;
                }
                else {
                    env->ThrowError("PerfPan: clip must be black and white. Use ConvrtToY8().Levels(160,1,161,0,255,true)");
                }
                if (reference_pixel == 0) {
                    reference_blacks++;
                }
                else if (reference_pixel == 255) {
                    reference_whites++;
                }
                else {
                    env->ThrowError("PerfPan: clip must be black and white. Use ConvrtToY8().Levels(160,1,161,0,255,true)");
                }
                if ((current_pixel ^ reference_pixel) == 255) {
                    mismatches++;
                }
                total++;
            }
        }

        int threshold = total * blank_threshold;

        if (current_blacks > threshold && current_whites > threshold
            && reference_blacks > threshold && reference_whites > threshold) {
            float match = (float)mismatches / total;
            if (match < best_match) {
                best_x = x;
                best_y = y;
                best_match = match;
            }
        }
    }
}

#if 0
/*
shift current frame and compare it against reference frame
*/
void algo::shift_and_process_frame(int x, int y) {
    if (x > min_x && x < max_x && y > min_y && y < max_y) {
        bool reference_is_blank = isblank(reference, x, y);
        bool current_is_blank = isblank(current, -x, -y);
        bool was_best = false;
        if (!reference_is_blank && !current_is_blank) {
            float match = compare_frame(x, y);
            if (match < best_match) {
                best_x = x;
                best_y = y;
                best_match = match;
                was_best = true;
            }
        }
#ifdef _WIN32
        if (debug != 0) {
            char debugbuf[200]; // buffer for debugview utility

            snprintf(debugbuf, sizeof(debugbuf),
                "PerfPan: shift_and_process_frame(%d, %d), reference %d, current %d, was_best %d, best%f\n",
                x, y, reference_is_blank, current_is_blank, was_best, best_match);
            OutputDebugString(debugbuf);
        }
#endif
    }
}
#endif

/*
shifts current frame in spiral motion and compares with reference frame to find best match
shifts are done half frame up and down and half frame left and right
*/
void algo::calculate_shifts() {
    int x = 0;
    int y = 0;
    int covered_min_x = 0;
    int covered_min_y = 0;
    int covered_max_x = 0;
    int covered_max_y = 0;

    compare_frame(x, y);
    bool worked = true;
    while (worked) {
        worked = false;
        // move right
        while (x < covered_max_x + 1) {
            if (y < max_y) {
                compare_frame(x, y);
                worked = true;
            }
            x++;
        }
        covered_max_x = x;
        // move down
        while (y > covered_min_y - 1) {
            if (x < max_x) {
                compare_frame(x, y);
                worked = true;
            }
            y--;
        }
        covered_min_y = y;
        // move left
        while (x > covered_min_x - 1) {
            if (y > min_y) {
                compare_frame(x, y);
                worked = true;
            }
            x--;
        }
        covered_min_x = x;
        // move up
        while (y < covered_max_y + 1) {
            if (x > min_x) {
                compare_frame(x, y);
                worked = true;
            }
            y++;
        }
        covered_max_y = y;
    }
}

#if 0
/*
shifts current frame to the direction where the match is best 
repeats until there is not better match around
shifts are done half frame up and down and half frame left and right
*/
void algo::calculate_shifts() {
    int x = 0;
    int y = 0;

    compare_frame(x, y);
    do {
        x = best_x;
        y = best_y;
        compare_frame(x + 1, y);
        compare_frame(x + 1, y + 1);
        compare_frame(x + 1, y - 1);
        compare_frame(x - 1, y);
        compare_frame(x - 1, y + 1);
        compare_frame(x - 1, y - 1);
        compare_frame(x, y + 1);
        compare_frame(x, y - 1);
    } while (x != best_x || y != best_y);
}
#endif

PerfPan_impl::PerfPan_impl(PClip _child, PClip _perforation, float _blank_threshold, int _reference_frame, int _info, 
    const char* _logfilename, int _debug, IScriptEnvironment* env) :
    GenericVideoFilter(_child), perforation(_perforation), blank_threshold(_blank_threshold), 
    reference_frame(_reference_frame), info(_info), logfilename(_logfilename), debug(_debug)
{
    has_at_least_v8 = true;
    try { env->CheckVersion(8); }
    catch (const AvisynthError&) { has_at_least_v8 = false; }

    if (!vi.IsY8()) {
        env->ThrowError("PerfPan: input must be Y8");
    }

    logfile = NULL;
    if (lstrlen(logfilename) > 0) {
        logfile = fopen(logfilename, "wt");
        if (logfile == NULL)    env->ThrowError("DePanEstimate: Log file can not be created!");
    }

    if (info == 0) {  // if image is not used for look, crop it to size of depan data
        // number of bytes for writing of all depan data for frames from ndest-range to ndest+range
        vi.width = depan_data_bytes(1);
        vi.height = 4;  // safe value. depan need in 1 only, but 1 is not possible for YV12
    }
}

PerfPan_impl::~PerfPan_impl() {
    if (lstrlen(logfilename) > 0 && logfile != NULL) {
        fclose(logfile);
    }
}

PVideoFrame __stdcall PerfPan_impl::GetFrame(int ndest, IScriptEnvironment* env) {
    PVideoFrame current = child->GetFrame(ndest, env);
    PVideoFrame reference = child->GetFrame(reference_frame, env);

    algo algo(reference->GetReadPtr(), current->GetReadPtr(), reference->GetPitch(), 
        reference->GetRowSize(), reference->GetHeight(), blank_threshold, debug, env);
    algo.calculate_shifts();
 
    PVideoFrame dst = has_at_least_v8 ? env->NewVideoFrameP(vi, &current) : env->NewVideoFrame(vi); // frame property support

    BYTE* dstp = dst->GetWritePtr();
    const BYTE* srcp = current->GetReadPtr();
    const int src_pitch = current->GetPitch();
    const int dst_pitch = dst->GetPitch();
    const int dst_rowsize = dst->GetRowSize();
    const int dst_height = dst->GetHeight();

    if (info) { // show text info on frame
        char messagebuf[64];
        int xmsg = dst_rowsize / 4 - 8; 
        int ymsg = dst_height / 40 - 4;

        env->BitBlt(dstp, dst_pitch, srcp, src_pitch, dst_rowsize, dst_height);
        /*
        snprintf(messagebuf, sizeof(messagebuf), " PerfPan");
        DrawString(dst, vi, xmsg, ymsg, messagebuf);
        snprintf(messagebuf, sizeof(messagebuf), " frame=%7d", ndest);
        DrawString(dst, vi, xmsg, ymsg + 1, messagebuf);
        snprintf(messagebuf, sizeof(messagebuf), " dx   =%7.2f", (float)algo.get_best_x());
        DrawString(dst, vi, xmsg, ymsg + 2, messagebuf);
        snprintf(messagebuf, sizeof(messagebuf), " dy   =%7.2f", (float)algo.get_best_y());
        DrawString(dst, vi, xmsg, ymsg + 4, messagebuf);
        snprintf(messagebuf, sizeof(messagebuf), " match=%7.2f", algo.get_best_match());
        DrawString(dst, vi, xmsg, ymsg + 5, messagebuf);
        */
    }
    else {
        write_depan_data_one(dstp, ndest, (float)algo.get_best_x(), (float)algo.get_best_y(), 1.0);
    }

    if (logfile != NULL) {
        fprintf(logfile, " %6d %7.2f %7.2f %7.5f\n", ndest, (float)algo.get_best_x(), 
            (float)algo.get_best_y(), algo.get_best_match());
    }

    return dst;
}

