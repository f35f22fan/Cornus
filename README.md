### Cornus - a fast file browser for KDE Linux written in C++17 and Qt5.

##### Requirements: Linux 5.3+, Qt 5.15.2+
---
Building on Ubuntu:
* sudo apt-get install qt5-default libdbus-c++-dev libudisks2-dev libdconf-dev cmake git ark
* mkdir build
* cd build
* cmake ..
* make -j4

##### Application Shortcuts:

* Alt+Up => Move one directory up
* Ctrl+H => Toggle show hidden files
* Ctrl+Q => Quit app
* Shift+Delete => Delete selected files
* F2 => Rename selected file
* Ctrl+I => Focus table
* Ctrl+L => Focus address bar
* Ctrl+A => Select all files
* Ctrl+E => Toggle exec bit of selected file(s)
* D => Display contents of selected file
* Ctrl+F => Search for file by name (then hit Enter to search forward or Ctrl+Enter for backwards)
* Ctrl+M => Search by (movie) file's metadata (see bottom of page)

---
### Screenshot with dark theme:
![](resources/Screenshot_dark.webp)

### Screenshot with light theme:
![](resources/Screenshot_light.webp)


##### Differences from other file browsers like Nautilus or Dolphin:
* Files with the executable bit set are marked green.
* If needed right click a file -> Open With -> Preferences... to set a default app with which to open a given type of file.
* To see the tree of links of a symbolic link click the icon of the link in the browser.
* Press 'D' after selecting a non-folder to display its contents (or click its icon). The built-in text editor is meant for a quick update of its contents or a glean into the file, not as a full blown text editor. Files' contents unrecognized as text files are opened in read-only mode.
* Files are deleted with Shift+Delete. No recycle bin. At least for now.
* A grey dot near the icon of the file means the file has extended attributes.
*  You can easily set your own icons for files of different types by dropping an icon into the "file_icons" folder with the proper extension as its name, browse the folder "file_icons" to see what I mean, probably located at "/usr/share/cornus/file_icons".
* "cornus_io" is the I/O daemon that is started automatically, keep it in the same folder as "cornus".
* File icons are loaded from "$HOME/.config/CornusMas/file_icons", then from "/usr/share/cornus/file_icons", the icons from the former folder are used if both folders contain files with equal names.
* Supports marking movie files rip quality, codec, resolution, actors, etc.
For this first go to the settings menu -> "Media Database" and fill in with the actors etc you might ever care about. Then you can apply any of these to any movie by clicking on the movie file and pressing "m", the end result:
### Movie Dialog Screenshot:

![](resources/movie_file_attributes.webp)

Media search: Ctrl + M
The settings are saved using extended file attributes and take up extemely little space.
At some point will support search for movie files inside a folder by any given category (actor, director, genre, etc).
