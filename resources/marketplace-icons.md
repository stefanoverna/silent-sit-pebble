# Marketplace icon prompts

The appstore (marketplace) listing icons are **separate** from the on-watch menu
icon (`icon.png`, a 25×25 monochrome silhouette that Pebble tints). They are
full-colour tiles on an indigo background.

The publish-ready PNGs are committed:

- `marketplace_small.png` — 48×48, the **flat** droplet (stays crisp in app lists) → `--icon-small`
- `marketplace_large.png` — 144×144, the **detailed** droplet with ripples → `--icon-large`

There are no committed masters. To regenerate: feed one of the prompts below to an
image generator (indigo = Pebble's `GColorIndigo`, `#5500AA`), get a 1024×1024
square, then downscale:

```sh
magick <generated-large>.jpg -filter Lanczos -resize 144x144 -strip resources/marketplace_large.png
magick <generated-small>.jpg -filter Lanczos -resize 48x48   -strip resources/marketplace_small.png
```

---

## Small / flat (→ `marketplace_small.png`, 48×48)

```
App store icon for "Silent Sit", a silent vibration-only meditation
timer for the Pebble smartwatch.

SUBJECT: a single clean white water droplet (classic teardrop shape),
solid pure white #FFFFFF, perfectly centered, flat design with no
gradient, no shading, no outline. It occupies about 55% of the frame
height with generous, even padding on all sides.

BACKGROUND: a solid flat indigo fill, exact color hex #5500AA, covering
the entire canvas edge to edge, no border, no frame, no rounded corners.

STYLE: minimalist, modern, geometric, zen and calm. Vector/flat icon
aesthetic, crisp sharp edges, high contrast. Absolutely no text, no
letters, no numbers, no logo, no shadows, no 3D, no bevel, no gloss, no
reflections, no realistic water, no splashes or ripples, no clutter,
single object only.

FORMAT: a perfectly square image, 1024 x 1024 pixels, full-bleed
background (no transparency).
```

Negative prompt:

```
text, letters, words, numbers, watermark, logo, photorealistic, realistic
water, splash, ripples, 3D, bevel, drop shadow, glossy, reflection,
gradient background, rounded corners, frame, border, multiple objects,
clutter, noise, texture
```

---

## Large / detailed (→ `marketplace_large.png`, 144×144)

```
App store icon for "Silent Sit", a silent vibration-only meditation
timer for the Pebble smartwatch. A refined, detailed yet elegant icon.

SUBJECT: a glossy white water droplet (classic teardrop shape) centered
in the frame, with a soft subtle highlight and a faint inner glow giving
it gentle depth. From the base of the droplet, concentric ripple rings
radiate outward in lighter translucent indigo and soft white, suggesting
a vibration spreading calmly across the wrist. 3 to 4 evenly spaced,
thin, fading rings.

BACKGROUND: a smooth radial gradient of indigo, centered on the droplet,
going from a slightly brighter indigo at the center to a deeper darker
indigo at the corners. Base color hex #5500AA. Full-bleed, edge to edge,
no border, no rounded corners. A soft luminous halo glows behind the
droplet.

STYLE: modern, premium, serene and meditative. Clean vector-like shapes
with subtle gradients and soft glows, smooth and polished, high contrast,
balanced composition with comfortable padding. No text, no letters, no
numbers, no logo, no harsh shadows, no photorealistic water, no splashes.

FORMAT: a perfectly square image, 1024 x 1024 pixels, full-bleed
background (no transparency).
```

Negative prompt:

```
text, letters, words, numbers, watermark, logo, photorealistic, realistic
water, splash droplets, busy, cluttered, noisy texture, harsh shadow,
rounded corners, frame, border, multiple droplets, distorted teardrop,
low contrast, muddy colors, oversaturated
```

---

## Listing banner (→ `marketplace_banner.png`, 720×320)

The store header banner is **not** set by `pebble publish` — upload it manually
from the appstore dashboard ("Upload New Banner"). It must be **exactly
720×320 px**. The publish-ready PNG is committed; no master is kept.

The text ("Silent Sit" / "A beat on your wrist") is baked into the prompt —
generators usually spell short strings right, but **double-check the output**.
If the lettering comes out garbled, generate text-free and typeset it afterwards.

Generate at 1440×640 (2.25:1, 2×), then downscale and centre-crop to exact size:

```sh
magick <generated-banner>.jpg -filter Lanczos -resize x320 -gravity center -extent 720x320 -strip resources/marketplace_banner.png
```

Prompt:

```
Wide horizontal app store banner for a meditation app, 2.25:1 aspect ratio.

LEFT SIDE: a single glossy white water droplet (classic teardrop shape)
with a soft highlight and faint inner glow, positioned in the left third.
From its base, concentric ripple rings radiate outward in lighter
translucent indigo and soft white, suggesting a calm vibration.

RIGHT SIDE: clean text, crisp and perfectly legible, in a modern
geometric sans-serif, pure white #FFFFFF, left-aligned, vertically
centered. A large bold title reading exactly "Silent Sit" on one line,
and directly below it a smaller lighter-weight tagline reading exactly
"A beat on your wrist". Correct spelling, evenly spaced letters, no extra
words.

BACKGROUND: a smooth indigo gradient, brighter around the droplet fading
to deeper darker indigo toward the edges, base color hex #5500AA.
Full-bleed, edge to edge, no border, no rounded corners. Soft luminous
halo behind the droplet.

STYLE: modern, premium, serene, meditative. Clean vector-like shapes,
subtle gradients, soft glows, high contrast, balanced and uncluttered.
No watch render, no device frame, no harsh shadows, no photorealistic
water, no splashes.

FORMAT: a wide rectangular image, exactly 1440 x 640 pixels (2.25:1),
full-bleed background, no transparency.
```

Negative prompt:

```
misspelled text, garbled text, gibberish letters, extra words, random
characters, duplicated text, distorted typography, watermark, logo, ui,
smartwatch render, device frame, photorealistic, realistic water, splash
droplets, busy, cluttered, noisy texture, harsh shadow, rounded corners,
frame, border, multiple droplets, distorted teardrop, low contrast, muddy
colors, oversaturated, square crop
```
