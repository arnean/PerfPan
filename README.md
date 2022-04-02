# PerfPan
PerfPan is an Avisynth plugin to stabilize scanned film using perforation as a reference.

It't goal is to remove the bouncing of the frames that is caused by the damaged sprocket holes 
of the film or by the scanning machines that do not have a good film transport. PerfPan will stabilize the film **globally** - all frames will be aligned with the single reference frame. This is very different from other stabilizers that will stabilize locally, relative to surrounding frames. Globally stabilized clip can be cropped with minimal waste - you will retain maximum image area without exposing borders in any frames.

Here is an example clip: https://home.cyber.ee/~arne/stable-flowers.mp4 Upper part is stabilized and lower part is an original.

Perfect input to PerfPan is a sequence of images scanned using Wolverine/Hawkeye scanner. 

```
source_clip=ImageReader("scanned_images%06d.jpg", start=1, end=7046, fps=16)
```

PerfPan will only pan frames in order to align the perforation. It will not rotate or zoom or alter the image in any other way - just 
panning to correct the scanning defects. You can later use some other deshaker, if you like to get rid off the camera movements or something. 

You need to overscan the film to capture the sprocket holes. Just a partial image of the sprocket hole is sufficient.

![Sufficient overscan](https://github.com/arnean/PerfPan/blob/master/images/overscan.jpg)

In addition to the original clip that will be stabilized, PerfPan needs a specially prepared clip that will be used 
for aligning the perforation.

Crop out the perforation area. Make sure that the cropped area is large enough for 
correctly panning all the images. PerfPan will pan the image only plus-minus one fourth of it's width and height. 
If the cropped area is too small PefPan is not able to fully fix the scanning errors. 

Make the clip black-and-white using Levels plugin. Select threshold that will leave the sprocket hole in all frames white and most of the 
film area black - the less white you have in the frame, the better and faster the plugin works. In the example below I've selected 150/151 as 
a threshold for black and white.

This is good reference frame:

![Prepared perforation clip](https://github.com/arnean/PerfPan/blob/master/images/goodreference.png)

This one is bad - there is white noise in the film area:

![Prepared perforation clip](https://github.com/arnean/PerfPan/blob/master/images/badreference.png)

Finally turn the clip into Y8 format - this is the required format for the perforation clip.

```
stabsource=source_clip.ConvertToYV12().Greyscale().Levels(150,1,151,0,255,false).Crop(0,0,300,600)
stabsource2=stabsource.ConvertToY8()
```

Now about the plugin itself. It works by comparing each frame in the perforation clip against the reference frame in the perforation clip. 
For reference frame select a frame that has sprocket hole corner near the middle of the image - this leaves most room for PerfPan to do it's work.
Ideally there should be no noise in the film area of the reference frame - it should be completely black.

For each frame in the clip the plugin will start by aligning the frame with reference frame, comparing them and calculating the score of the match. 
It will then shift the frame plus-minus one pixel relative to the reference frame in all directions (horizontally, vertically and diagonally) 
and calculate the scores for all those eight positions. If there is a position with better score plugin will pick this as a new starting point
and will repeat the process by shifting the image even further until it is unable to improve the score anymore. 
Finally it will pan the frame of the original clip by same amount to align it with the reference frame in original clip. 
Borders will be filled with green color to show the amount of correction.

Plugin has some parameters that need to be set:

* **perforation** - this is a mandatory parameter for perforation clip.
* **blank_threshold** - no need to change the default value 0.01. This parameter affects the comparison of the frames. When the frames are shifted relative to 
each other their intersection becomes smaller. It is possible that one of the frames will be completely white or black - in this case it is not 
possible to compare the frames with each other because one of the frames does not have any features left. This parameter determines the minimum 
number of differently colored pixels that are needed for comparison. It must be between 0 and 1. Default is 1%.
* **reference_frame** - number of the reference frame in the perforation clip. First frame of the clip will be used if not set.
* **max_search** - this parameters determines how widely the algorithm looks for the best match. If set to -1 PefPan performs exhausitve search, which is very, very slow. This is only needed for debugging the filter. See below for detailed explanation what this parameter does.
* **log** - name of the logfile. If set PerfPan will write a file with frame numbers, x and y shift, best score of the frame and clipping information. Useful for debugging. Logfile has same format as hintfile. Run the script and copy the logfile to hintfile for very fast action and possibility to correct errors.
* **plot_scores** - this is something that I used to debug the scoring and searching algorithms. See below for explanation.
* **hintfile** - file with X and Y offsets for frames. PerfPan will read this file on initialization. If there is hint for the frame the algorithm is not run instead the values from hintfile are used. In principle you can specify offsets for all frames and use PerfPan just for shifting the frames. You do not need to add offsets for all frames. If there is just one frame you want to shift manually add one line to hintfile. Hintfile has same format as logfile. You can run the script once for all frames, close the script, copy the logfile to hintfile, reopen the script and then tweak the individual frames where PerfPan did not find correct offsets.
* **copy_on_limit** - if PerfPan shifts the frame to the limit (which is quarter of the frame height and width) then it is possible that the perforation was not readable (i.e. it was all white) and PerfPan shifted the frame way too far. If this option is set to true, PerfPan will use the offsets from previous frame instead. This will avoid jumping of the frames. See the description of scanning workflow for options.

```
source_clip.PerfPan(perforation=stabsource2,blank_threshold=0.01,reference_frame=461,\
max_search=10,log="perfpan.log",plot_scores=false,hintfile="",copy_on_limit=false)
```

[Here is a small clip](https://home.cyber.ee/arne/perfpan-demo.mp4) that shows the results of the stabilization of the perforation clip itself. 
On the left side is frame on top of reference frame. On the right side is panned frame on top of reference frame.

[Here is a longer clip](https://home.cyber.ee/arne/perfpan-scan.mp4) that shows uncropped result of the stabilization. It is interesting to note that 
while sprocket holes are quite stable the frames themselves tend to move relative to sprocket holes little bit. Notice how big are shifts in some areas
of the clip.

## Scanning workflow

I will go through my scanning workflow step by step and show all the tricks.

### Scan

Use the frame by frame scanning machine like Wolverine with Hawkeye kit to scan your film frame by frame.

TODO: add scanning video.

The result of this step is a directory full of sequentially named images (e.g. `scan00001.jpg`, `scan00002.jpg` etc.). My scripts assume master directory (e.g. `scans`), project directory under it (e.g. `trip1970`) and image directory `img` under it. 

Don't worry if there are bad images e.g. beacuse the film got jammed in the scanner or damaged frames or something like this. Those will be deleted on the next step.

### Select images for converting

Select images that you want to convert. Delete bad frames (e.g. sometimes machine gets stuck and there are repeated frames, sometimes there are blank frames, frames where film was spliced, scenes that you just want to remove etc.). I usually scan full roll that contains different clips from different times and split it into individual parts at this step.

You can use Windows Explorer with image preview or some special software. I like [FastStone Image Viewer](https://www.faststone.org/).

### Renumber images

When you delete some frames there will be holes in the image sequence. Some Avisynth input filters can skip those missing frames, but the standard `ImageReader` doesn't. I found it easier to renumber the remaining images. This is the shell script that I'm using:

```
c=1
for n in `ls | sed -e 's/scan//;s/\.jpg//; s/^[0]*//' | sort -n`; do
        old=`printf "%04d" $n`
        new=`printf "%06d" $c`
        mv scan$old.jpg n_scan$new.jpg
        c=$(($c+1))
done
```

I run it under Ubuntu that I installed from Microsoft Store. So you have fully functioning Linux inside your Windows. Of course you can use some other input filter that can skip the missing frames or renumber the files some other way -- this is just the script that I'm using.

### Create perforation script

You will need to select parameters for the perforation script that will be basis for the image stabilization. Use the following Avisynth script to do it:

```
SetMemoryMax(1000) 
LoadPlugin("../PerfPan.dll")

first_frame=1
last_frame=1234
fps=16
level=220

source0=ImageReader("img\n_scan%06d.jpg", start=first_frame, end=last_frame, fps=fps)
perfcorner=source0.ConvertToYV12().Greyscale().Crop(0,40,180,300).ConvertToY8().coloryuv(autogain=true).Levels(level,1,level+1,0,255,false)
perfcorner
```

You must select parameters for cropping so that the rounded corner of the perforation hole is more or less in the middle of the image. Select the value for the black and white threshold `level` so that frames are mostly black, perforation hole is white and the border of the edges of the perforation hole are nice and sharp. Finally select the reference frame -- all images will be aligned with this frame.

### Check the stabilization parameters

Open this script in Virtualdub:

```
SetMemoryMax(1000) 
LoadPlugin("../PerfPan.dll")

first_frame=1
last_frame=1234
fps=16
reference_frame=234
level=220
hintfile=""
#hintfile="hints.txt"

source0=ImageReader("img\n_scan%06d.jpg", start=first_frame, end=last_frame, fps=fps)

perfcorner=source0.ConvertToYV12().Greyscale().Crop(0,40,180,300).ConvertToY8().coloryuv(autogain=true).Levels(level,1,level+1,0,255,false)

stabsource=perfcorner.ConvertToRGB()

stabbed=stabsource.PerfPan(perforation=perfcorner,blank_threshold=0.01,reference_frame=reference_frame,max_search=10,log="s.log",plot_scores=false,hintfile=hintfile,copy_on_limit=true)

ref=stabsource.FreezeFrame(0,last_frame,reference_frame)

stackhorizontal(Subtract(stabsource,ref), Subtract(stabbed, ref))
```

Browse through the resulting video (I use alt-cursor keys in Virtualdub to jump 50 frames forward and backwards). Make sure that the PerfPan seems to find the perforation holes and stabilize the video. It's okey if some frames are not perfectly stabilized -- we will deal with those later. It's just important that there are not too many frames where PerfPan did a bad job. You can try to play with PerfPan parameters and/or perforation clip parameters to try to fix it.

### Create the hintfile for stabilization

Now you must create a hintfile for stabilization. In principle you can skip this (and following) step and just stabilize the video. But hintfile allows you to fix any errors where PerfPan didn't manages to stabilize the frame. I always use it to verify the quality of the stabilization.

Reopen the script and play it from the beginning (press spacebar in Virtualdub). PerfPan will create a logfile in the directory where the script is located. After Virtualdub finishes playing close the video -- this will close the logfile and then rename it to `hints.txt`.

Now edit the Avisynth script that you just used and set the name of the hintfile variable:

```
hintfile="hints.txt"
```

Reopen the script in Virtualdub.

Next create a Gnuplot file `hints.plt` in the script directory with following contents:

```
set grid

plot 'hints.txt' using 1:2 with points lt rgb "blue" lw 1 pt 6, 'hints.txt' using 1:3 with points lt rgb "red" lw 1 pt 6, 'hints.txt' using 1:5 with points lt rgb "green" lw 1 pt 6
```

Install Gnuplot and then double click on the plt file. Gnuplot will open window that shows all the calculated X and Y offsets and also marks the frames that were shifted too far. You can zoom in using mouse - right-click the corners of the part you want to zoom. Use cursor keys to scroll the graph. You are looking for frames where red and blue dots are far away from neighbouring dots. It is possible that PerfPan made a mistake there. Go to Virtualdub and jump to the area where the problematic frame was located. Scroll back and forward and make sure that all frames are nicely stabilized. If you find a frame that is not correctly panned you can fix it by editing the `hints.txt` file. Find the line that corresponds to the problematic frame. Second and third columns represent the X and Y shifts -- edit them, save the hintfile and reload the script in Virtaldub. Repeat it until frame is properly aligned. 

![Plot of hintfile](https://github.com/arnean/PerfPan/blob/master/images/hintfileplot.png)

It can be quite time-consuming, but improves the quality of the final clip. 

Also check the frames where green dots are not on horizontal axis. Those are the frames where PerfPan shifted the frame as far as it could, but perhaps it was not enough. Also check out the `copy_on_limit` option for the PerfPan filter -- it will copy the shift values from previous frame when the shift limit is reached.

Here is the zoomed in part that shows frames where copy_on_limit was in action:

![Plot of hintfile](https://github.com/arnean/PerfPan/blob/master/images/copyonlimit.png)

After reviewing all the problematic frames save the hintfile.

### Crop the stabilized video

Open this script in Virtualdub:

```
SetMemoryMax(1000) 
LoadPlugin("../PerfPan.dll")

first_frame=1
last_frame=1234
fps=16
reference_frame=234
level=220
#hintfile=""
hintfile="hints.txt"

source0=ImageReader("img\n_scan%06d.jpg", start=first_frame, end=last_frame, fps=fps)

perfcorner=source0.ConvertToYV12().Greyscale().Crop(0,40,180,300).ConvertToY8().coloryuv(autogain=true).Levels(level,1,level+1,0,255,false)

stabsource=source0.ConvertToRGB()

stabbed=stabsource.PerfPan(perforation=perfcorner,blank_threshold=0.01,reference_frame=reference_frame,max_search=10,log="s.log",plot_scores=false,hintfile=hintfile,copy_on_limit=true)

stabbed.Crop(170,124,1024,786)
```

Change the parameters of the last crop command to remove all the borders. 

You can add additional processing at the end of this script.

### Save the video

Save the codec and container format and save the script into video file.

All done.

## How it works

There are three parts:

* scoring algorithm for comparing the frames
* searching alorithm for finding the highest score by shifting the current frame relative to reference frame
* code for doing the actual panning of the original frame

### Scoring algorithm

My initial idea was to just XOR the frames and count the number of white pixels (white pixels on the XOR-ed image are the ones that
are different on frames) and use this number as a negative score (lower is better). 

Later I changed the scoring algorithm to value the match in the sprocket hole area more than the match in the 
rest of the image. Match in the perforation area is 20 times more important than the match in the rest of the frame. This coefficient was 
found using trial and error process. It is likely that for some other clips a different value is needed. Right now it is hardcoded.

Recall that the ideal reference frame is completely black, with just the sprocket hole being white. So when both frames have white pixel in the same location - it is 20 points. If reference frame has white pixel and current frame has black pixel - it is minus 20 points - there is a black pixel in the sprocket area, frame is shifted too far. If both pixels are black (film area) - it is 1 point. If reference frame has black pixel and current frame has white pixel - it is minus 1 point - it can either be frame shifted too far so the sprocket hole in the current frame covers the film area on the reference frame or it is white noise in the film area on current frame.

The further the frames are shifted the smaller gets the comparison area - frame overlap decreases. This causes a problem, because in some cases you will shift out "good" pixels that help to increase the score. When you shift the frame to align the sprocket holes, you will reduce the differences in one area of the image - the score improves. But you might at the same time shift out some "good" pixels in other area - this will reduce the score. It is possible that those changes cancel out each other and the total score does not improve - the scoring algorithm does not find an optimal solution.

In order to study how the scoring algorithm works you can plot out the scores. Set the **plot_scores** parameter to true and **max_search** parameter to -1. PerfPan will do an exahaustive search for the best score on each frame - it means that it will shift the current frame relative to the reference frame to all possible positions and calculate the score for each of those positions. The results will be written to file frame%d.plt - where %d is the number of the frame. The file is **gnuplot** script. If you have gnuplot installed, double click on the file and you will see similar image:

![Score heatmap](https://github.com/arnean/PerfPan/blob/master/images/heatmap.png)

Bright yellow is the best score. X and Y coordinates of the brightest yellow pixel determine the amount of panning needed to achieve best match with the 
reference frame. Fortunately, the scoring algorithm was quite good (at least for this single clip that I've used so far) and is almost monotonically growing - i.e. there is just one peak in this "map". This means that I can use simple gradiant search that is quite efficient. In practice I found out that there is usually a small "plateau" near the top were score values are slightly fluctuating and I had to modify the search algorithm to accomodate this.

### Search algorithm

It is quite simple. Plugin will start with frames (current and referene frame) aligned with each other. It will calculate the score for this position. Then it will shift the current frame around by one pixel and calculate the score for each of those eight positions. Then it will pick the position with highest score as a new center and will repeat this process. If you visualize the scores as a 3D surface, where the score at the panning position X and Y determines the height of the surface, then this algorithm will move "uphill", until it reaches the top. 

This works for most of the frames but I had a problem with some noisy frames where the "top of the hill" was wide and flat and with some bumps. 

The modified algorithm makes an extra step when it reaches the top - i.e. when the scores of the immediately surrounding positions are lower, it will look further by calculating the scores for the positions that are two pixels away. 
If it still doesn't find better match it will look three pixels away, etc. The **max_search** parameter determines how far it will look.
Modified algorithm is doing a local limited exhaustive search when gradient search doesn't give any results. If the exhaustive search finds a better score the  gradiant search resumes until next top is found. **max_search** values bigger than 20 make the algorithm quite slow.

If **max_search** is not -1 and **plot_scores** is true then another frame specific file is created instead - frame%d.txt - this file contains the trace of the execution of the search algorithm: how the algorithm moved through the search space, when it increased the search radius. This is useful for debugging the filter.

### Panning algorithm

I took the Avisynth core AddBorders filter and modified it to do panning - i.e. on one side it will add borders but on the other side it will crop to keep the image size. It will work with all image formats and should be quite effective. Just one remark: if the original clip is in YV12 colourspace then it is possible to pan only in steps of two pixels. The plugin will automatically do it, but the result is not as good as with other formats.
