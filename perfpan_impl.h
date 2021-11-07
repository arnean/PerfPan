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
#ifndef __PERFPAN_IMPL_H__
#define __PERFPAN_IMPL_H__

#include "avisynth.h"
#include "stdio.h"
#include <mutex>
#include <unordered_map>

//****************************************************************************
class PerfPan_impl : public GenericVideoFilter {
	bool has_at_least_v8;
	double blank_threshold;
	int reference_frame;
	int max_search;
	bool plot_scores;
	const char *logfilename;
	PClip perforation;
	const char* hintfilename;

	FILE *logfile;
	std::unordered_map<int, int> xhint;
	std::unordered_map<int, int> yhint;

public:
	PerfPan_impl(PClip _child, PClip _perforation, float _blank_threshold, int _reference_frame, int _max_search, 
		const char* _logfilename, bool _plot_scores, const char* hintfilename, IScriptEnvironment* env);
	~PerfPan_impl();

	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
};

#endif
