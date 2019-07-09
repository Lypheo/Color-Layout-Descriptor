## [Color Layout Descriptor](https://en.wikipedia.org/wiki/Color_layout_descriptor) for [VapourSynth](https://github.com/vapoursynth/vapoursynth)

#### Usage

Input needs to be 8-bit YUV.
```py
clp = core.cld.ComputeCLD(src)
```

CLD matrices are saved as row-major ordered lists in the frame properties `CLD_y`, `CLD_` and `CLD_v`.

Example snippet for finding the frame in a clip that best matches some screenshot, computing the difference as per the spec:
```py
cld_clip = core.cld.ComputeCLD(src)
sc = core.cld.ComputeCLD(screenshot_clip)
sc_cld = {i:v for i,v in sc.et_frame(0).props.items() if "CLD" in i}
diffs = []
for k,f in enumerate(cld_clip.frames()):
	cld = {i:v for i,v in f.props.items() if "CLD" in i}
	diff = 0
	for channel, dct in cld.items():
		ch_diff = 0
		for i,v in enumerate(dct):
			ch_diff += (v - sc_cld[channel][i])**2
		diff += math.sqrt(ch_diff) 
	diffs.append(diff)
 
print(diffs.index(min(diffs))
```
#### Compilation
I have literally no idea D:
