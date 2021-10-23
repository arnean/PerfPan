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

![Prepared perforation clip](https://github.com/arnean/PerfPan/blob/master/images/perforation.jpg)

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
source_clip.PerfPan(perforation=stabsource2,blank_threshold=0.01,reference_frame=461,max_search=10,log="perfpan.log",plot_scores=false)
```

