# PerfPan
PerfPan is an Avisynth plugin to stabilize scanned film using perforation as a reference.

It't goal is to remove the bouncing of the frames that is caused by the damaged sprocket holes 
(perforation) of the film or by scanning machines that do not have good film transport. Perfect
input to PerfPan is a sequence of images scanned using Wolverine/Hawkeye scanner. 

```
source_clip=ImageReader("scanned_images%06d.jpg", start=1, end=7046, fps=16)
```

PerfPan will only pan frames in order to align the perforation. It will not rotate or zoom or alter the image in any other way - just 
panning to correct the scanning defects. You can later use some other deshaker if you like to get rid of camera movements or something. 

You need to overscan the film to capture the sprocket holes. Just a partial image of the sprocket hole is sufficient.



In addition to the original clip that will be stabilized, PerfPan needs a specially prepared clip that is used 
for aligning the perforation. 

1. Crop out the perforation area. Make sure that the cropped area is large enough for 
correctly panning all the images. PerfPan will pan the image only pluss-minyus one fourth of it's width and height. 
If the cropped area is too small PefPan is not able to fully fix the scanning errors. 

2. Make it black-and-white using Levels plugin 
