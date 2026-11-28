# PEEK – changes

The newest first. To release a new version: raise the number in VERSION and put its notes here under
"## <the new number>". After the merge into main the release is made by itself (auto-release.yml), the
downloads are built into it, and the website and the Discord post follow.

## 1.1.1
Smoother filter: turning the cutoff no longer steps. The cutoff knob's 256 steps (half a semitone each) now glide into one another, and modulation of the cutoff from the mod matrix moves smoothly too - no stair-step sound when you sweep the filter, also with high resonance. (Also in the MPC version.)

## 1.1.0
Sharing sounds made easy: the SAVE window has two new buttons.
- **Folder** opens the folder with your own patches in Explorer, Finder or the file manager.
- **Import...** takes in patches from someone else: pick one or more `.peek` files, or a `.zip` of them. They show up in the list right away; a patch you already have is not replaced.

## 1.0.10
After saving a sound of your own, the plugin now stays on it (before, Cubase, FL Studio and other VST3 hosts switched to another patch, so the new one had to be loaded again).

## 1.0.9
New: section zoom. Every section's name has a small magnifier - click the name and that section fills the window, sharp and fully playable, so knobs and labels are easy to read and hit on a small laptop screen. Click the X, next to the section or the name again to see the whole panel.

## 1.0.8
New: PEEK for Linux - VST3, LV2 and a standalone app (64-bit, built on Ubuntu 22.04). Checked with pluginval like the Windows version.

## 1.0.7
The window opens as large as you left it - also after closing and reopening the DAW (before, it came back at the smallest size). A new instance starts at the size you used last.
Saving your own sounds fixed: the save window now opens inside the plugin (before, it could open hidden behind the DAW's plugin window, so nothing got saved). If Windows keeps apps out of the Documents folder, your sounds are saved in AppData instead. A failed save now says so.

## 1.0.6
(see the release page on GitHub)
