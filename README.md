# PerfPan
PerfPan is an Avisynth plugin to stabilize scanned film using perforation as a reference.

It't goal is to remove the bouncing of the frames that is caused by the damaged sprocket holes 
of the film or by the scanning machines that do not have a good film transport. Perfect
input to PerfPan is a sequence of images scanned using Wolverine/Hawkeye scanner. 

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
* **reference_frame** - number of the reference fram in the perforation clip. First frame of the clip will be used if not set.
* **max_search** - this parameters determines how widely the algorithm looks for the best match. If set to -1 PefPan performs exhausitve search, which is very, very slow. This is only needed for debugging the filter. See below for detailed explanation what this parameter does.
* **log** - name of the logfile. If set PerfPan will write a file with frame numbers, x and y shifts and best score of the frame. 
Useful for debugging.
* **plot_scores** - this is something that I used to debug the scoring and searching algorithms. See below for explanation.

```
source_clip.PerfPan(perforation=stabsource2,blank_threshold=0.01,reference_frame=461,\
max_search=10,log="perfpan.log",plot_scores=false)
```

[Here is a small clip](https://home.cyber.ee/arne/perfpan-demo.mp4) that shows the results of the stabilization of the perforation clip itself. 
On the left side is frame on top of reference frame. On the right side is panned frame on top of reference frame.

[Here is a longer clip](https://home.cyber.ee/arne/perfpan-demo.mp4) that shows uncropped result of the stabilization. It is interesting to not that 
while sprocket holes are quite stable the frames themselves tend to move relative to sprocket holes little bit. Notice how big are shifts in some areas
of the clip.

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
found using trial and errrors process. It is likely that for some other clips different value is needed. Right now it is hardcoded.

Recall that the ideal reference frame is completely black, with just the sprocket hole being white. So when both frames have white pixel in same location - it is 20 points. If reference frame has white pixel and current frame has black pixel - it is minus 20 points - there is a black pixel in the sprocket area, frame is shifted too far. If both pixels are black (film area) - it is 1 point. If reference frame has black pixel and current frame has white pixel - it is minus 1 point.

When you shift the frames relative to each other and calculate the scores there is a specific problem, because the bigger the shift, the smaller the comparison area. When you shift the frame to align the sprocket holes, you will reduce the differences in one area of the image - the score improves. But you might at the same time shift out some "good" pixels in other area - this will reduce the score. It is possible that those changes cancel out each other and the total score does not improve - the scoring algorithm does not find an optimal solution.

In order to study how the scoring algorithm works you can plot out the scores. Set the **plot_scores** parameter to true and **max_search** parameter to -1. PerfPan will do an exahaustive search for the best score on each frame - it means that it will shift the current frame relative to the reference frame to all possible positions and calculate score for each of those positions. The results will be written to file frame%d.plt - where %d is the number of the frame. The file is 
**gnuplot** script. If you have gnuplot installed double click on the file and you will see similar image:

![Score heatmap](https://github.com/arnean/PerfPan/blob/master/images/heatmap.png)

Bright yellow is the best score. X and Y coordinates of the brightest yellow pixel determine the amount of panning needed to achieve best match with the 
reference frame. Fortunately the scoring algorithm was quite good (at least for this single clip that I've used so far) and is almost monotonically growing - i.e. there is just one peak in this "map". This means that I can use simple gradiant search that is quite efficient. In practice I found out that there is usually small "plateau" were score values are slightly fluctuating and I had to modify the search algorithm to accomodate this.

### Search algorithm

It is quite simple. It will start with frames (current and referene frame) aligned with each other and calculate the score. Then it will shift the current frame around by one pixel and calculate score for each of those eight positions. Then it will pick the position with highest score as a next center and will repeat this process. If you imagine the scores as a 3D surface where the score at panning position X and Y will determine the height then this algorithm will move "uphill" until it reaches the top. 

This works for most frames but I had a problem with some noisy frames where the "top of the hill" was wide and flat and with some bumps. Now if the algorithm reaches the top - i.e. the scores of the immediately surrounding positions are lower it will look further - it will calculate the scores for the positions 
that are two pixels away. If it still doesn't find better match it will look three pixels away, etc. The **max_search** parameter determines how far it will look.
It is like a local limited exhaustive search when gradient search doesn't give any results. Values like 20 make it already very slow.

If **max_search** is not -1 and **plot_scores** is true then another frame specific files is created instead - frame%d.txt - this will contain the trace of the execution of the search algorithm. How it selected new, better positions and when it increased the search radius. This is useful for debugging the filter.

### Panning algorithm

I took the Avisynth core AddBoders filter and modified it to do panning - i.e. on one side it will add borders but on the other side it will crop to keep the image size. It will work with all image formats and should be quite effective. Just one remark: if the original clip is in YV12 colourspace then it is possible to pan only in steps of two pixels. The plugin will automatically do it, but the result is not as good as with other formats.
