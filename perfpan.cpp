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

//#include "windows.h"
#include <avisynth.h>
#include "perfpan_impl.h"

AVSValue __cdecl Create_PerfPan(AVSValue args, void* user_data, IScriptEnvironment* env) {

  return new PerfPan_impl(args[0].AsClip(), // the 0th parameter is the original clip
    args[1].AsClip(), // perforation clip
    (float)args[2].AsFloat(0.01),		//  parameter - blank_threshold.
    args[3].AsInt(0),	//  parameter - reference_frame.
    args[4].AsInt(3),//  parameter - max_search.
    args[5].AsString(""),	//  parameter - log.
    args[6].AsBool(false),	//  parameter - plot_scores.
    args[7].AsString(""),  // parameter - hintfile
    args[8].AsBool(false),	//  parameter - copy_on_limit.
    env);
}


//*****************************************************************************
// The following function is the function that actually registers the filter in AviSynth
// It is called automatically, when the plugin is loaded to see which functions this filter contains.
/* New 2.6 requirement!!! */
// Declare and initialise server pointers static storage.
const AVS_Linkage *AVS_linkage = 0;

/* New 2.6 requirement!!! */
// DLL entry point called from LoadPlugin() to setup a user plugin.
extern "C" __declspec(dllexport) const char* __stdcall
AvisynthPluginInit3(IScriptEnvironment* env, const AVS_Linkage* const vectors) {

  /* New 2.6 requirment!!! */
  // Save the server pointers.
  AVS_linkage = vectors;

  env->AddFunction("PerfPan", "c[perforation]c[blank_threshold]f[reference_frame]i[max_search]i[log]s[plot_scores]b[hintfile]s[copy_on_limit]b", Create_PerfPan, 0);

  return "`PerfPan' PerfPan plugin";
}

