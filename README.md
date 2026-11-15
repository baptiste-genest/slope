# What is Slope?
## [Documentation Website](https://slopedoc.github.io/), [Code Examples](https://github.com/baptiste-genest/SlopeExamples), [Project Template](https://github.com/baptiste-genest/SlopeTemplate) & [Tutorial video](https://youtu.be/YEEsuHSZFN0)
Exporting scientific results or complicated concepts into pretty presentations is a very hard process that very often leads to a sad succession of screenshots and equations.


But talking about graphics and geometry without 3D is both a waste of joy and clarity.

Hence we propose **Slope**, a 3D slide engine built on top of [Polyscope](https://polyscope.run/), which can be loaded on top of your scientific code!

The goal is to allow researchers, teachers and geometry lovers to easily illustrate concepts with *pretty & interactive* 3D slides and animations.
<p align="center">
  <img src="https://slopedoc.github.io/static/slope.gif" alt="Slope">
</p>

### Main features:

- Easy integration of all polyscope primitives and quantities
- LaTeX compilation, with hot-reloading
- Slides can be composed in a hot-reloadable YAML deck, editable while running
- :fire: **In-App text editor & documentation!** :fire:
- :fire: **Live shaders, synchronizable with polyscope!** :fire:
- :fire: **Animation logic in hot-reloaded Lua snippets** :fire:, and values tunable live while the show runs
- Full control over objects and animations
- Automatic transitions
- Step by step code display
- Plots drawn live: curves, scatters and log axes, from a file, a formula or your own data
- Interactivity with ImGui widgets
- Possible forced breaks to control the pace of the presentation and talk rehearsal mode
- 3D world to screen tracking

### Presentations with *Slope*:

Here is a link to a youtube playlist containing presentations with Slope : [link](https://youtube.com/playlist?list=PLmC_RrzoO5-dHpV4h-ZX7077p0G-bjQWw&si=YfHUrDVlMtzPaDGn). Please let me know if you want to add your presentation to the list!

### Install dependencies

#### Linux
```
sudo apt-get update
sudo apt-get install -y build-essential cmake git xorg-dev libglu1-mesa-dev libgl1-mesa-dev libegl-dev libgl1-mesa-dri imagemagick ghostscript texlive-latex-base texlive-latex-extra texlive-latex-recommended texlive-fonts-recommended ffmpeg
```

#### Windows
Install Visual Studio 2022 (C++ workload), [MiKTeX](https://miktex.org/), then with [scoop](https://scoop.sh/):
```
scoop install git imagemagick ghostscript ffmpeg
```
Build from a *Developer PowerShell for VS 2022* in a new terminal:
```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
The webcam is not available on Windows yet.
