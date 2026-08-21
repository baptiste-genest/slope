# Missing features

A running list of things that turned out not to exist, or to be broken, while
actually building something with slope. Each entry says what was wanted, what
exists today, and what the gap is. Append as you hit them; delete when fixed.

---

## From building the ARAP example (SlopeExamples/arap), 2026-08-20

### 1. `--export` ignores the background colour
`Slideshow::play()` sets `polyscope::view::bgColor` from `BackgroundColor::Default`
every frame, but the `--export` path installs its own `userCallback` which never
does. So every exported slide is white whatever the deck says — including when a
`BackgroundColor` primitive is placed in the frame, since the screenshot clears
before the callback runs. Setting `polyscope::view::bgColor` by hand before
`run()` does not survive either.

*Impact:* a deck designed on a tinted background looks different exported. The
ARAP example was redesigned around white rather than ship that mismatch.

### 2. No live numeric readout
There is no way to put a changing number on a slide. Latex is compiled to png and
cached by hash, so a value that moves every frame cannot go through it.

Wanted: the current energy and sweep count as text beside the plot. Ended up
drawing the *curve* in a shader instead, which works but cannot show a figure.

Would want: a `readout:` item, or a latex item interpolating a name from the
snippet/params namespace (`E = {energy}`), re-rendered only when the digits change.

### 3. Params are read-only from C++
`Params::Handle` has `operator T()` and `operator*`, and there is
`Params::read(name, scalar*)`, but nothing writes. A C++ updater cannot set a
parameter's value.

Wanted: on entering the live slide, hand the scripted tour's final handle position
to the `vec3` gizmo so the mesh does not jump. Worked around by matching the
parameter's default to the tour's hold position by hand — which silently rots if
either is edited.

### 4. Shader uniforms take no array from C++
`Shader::set` covers `float/int/vec2/vec/RGBA`; `bindDynamic` tops out at 4
components. The manifest can declare `"vec3[8]"`, but those are *tunable
parameters*, not values C++ feeds.

Wanted: push 11 energies to a plot shader. Did it as a 1xN float data texture via
`setTexture(name, std::vector<float>, w, h, comps)`, which works well and is
documented — but `bind("energies", const std::vector<float>&)` is the obvious
missing overload, and a texture is a strange thing to reach for to plot 11 numbers.

### 5. `plot2d.glsl` has no text
The stdlib draws grids, axes, ticks, curves and markers, and they look good. But
there is no way to draw a number, so every axis label is a separate latex item
positioned against the panel by hand. Tick *values* on an axis (especially a log
one) are effectively impossible.

### 6. Relative placement has no corners
**Not missing:** `below:`/`above:`/`left_of:`/`right_of:` + `padding:` do anchor to
another item, including a shader panel placed with `object:`. `below: energy_plot`
puts a label centred under the panel and follows it if it moves — verified.

**Missing:** those anchors centre on an edge, and `offset:` (which `follow:`
honours) is ignored by them. So a label cannot be hung on a *corner* — a y-axis
label at a panel's top-left has to go back to absolute coordinates. Either
honouring `offset:` on the four anchors, or a corner form, would close this.

### 7. A bad colormap name aborts the process
`setColorMap("mako")` threw `std::runtime_error` from polyscope and killed the run
halfway through an export. A warning plus a fallback would be kinder, and the list
polyscope actually ships (`gray viridis magma inferno plasma coolwarm blues piyg
spectral rainbow jet turbo reds phase hsv`) is not written down anywhere on the
slope side.

### 8. Docs drift
- `docs/options.md` documents `Eigen::Vector3d DefaultBackgroundColor`. That member
  does not exist; the real API is `BackgroundColor::Default` (a `Color`) and
  `BackgroundColor::Add(r,g,b)`.
- `Primitives/Latex/latex.md` still calls `load:` + `latex.json` "the one to
  prefer", though it is being retired in favour of writing the maths inline.

### 9. A camera cannot be aimed at a value computed in C++
`CameraView::Add(from, to, up, flyTo)` exists, but `CameraView` is not a
`Primitive`, so it cannot be registered with `registerObject` and placed by the
manifest. A `camera:` item can only name a saved `views/*.json`.

Wanted: fly to a close-up of a vertex the program picks at runtime (the one-ring
that rotates most). Had to run once, print the vertex's position, and bake a
hand-built `views/closeup.json` from it — which silently stops pointing at the
right place if the mesh, the pull or the sweep count changes.

Would want either a `CameraView` that can be registered like any other object, or
a manifest camera that can read a point from the snippet/params namespace the way
`follow:` does.

### 10. `fly` defaults the other way round from the docs
`DeckLoader.cpp` reads `item.value("fly", true)`, so a bare `- camera: view`
flies. `Primitives/camera.md` documents `fly: true # (default: false)`.

### 11. No help for edge quantities on a `Mesh`
Highlighting one vertex's one-ring on the surface wants an edge quantity, but
polyscope refuses one until `setEdgePermutation()` has been called, and it
numbers edges in its own canonical order: first-seen `(min,max)` pair while
walking every face's three corners in turn. Nothing in slope exposes that, so
the example reimplements the loop in `main.cpp` to know which slot each edge
lands in, and passes an identity permutation to make the two agree.

Would want `Mesh` to build the edge indexing once (it already owns the faces)
and offer something like `mesh->edgeIndex(i,j)` plus a quantity helper, so
marking a ring is a lookup rather than a copy of polyscope's internals. The
first attempt at this drew three `CurveNetwork`s over the surface instead, and
they were unreadable against the wireframe.

## Text does not follow the background

Backgrounds became slide state (a colour param, or a snippet of that name),
so a deck can now go dark. Nothing that draws glyphs knows about it.

A `- title:` is compiled by pdflatex, so its colour lives in the LaTeX source :
a dark frame needs `\color{white}` written into every title, formula and latex
item on it, and again on every frame that inherits the background. `parseColor`
already exists for boxes and arrows, but there is no deck-level text colour.

The slide number was worse and is fixed : `Text` now inverts
`polyscope::view::bgColor` like `drawGizmoMode` and `drawPauseIndicator` always
did. Inversion still vanishes on a mid-grey background, as it does for those two.

Would want one place that answers "what colour should ink be on this slide",
read by the HUD and used as the default for latex items, so a background change
carries its text with it.
