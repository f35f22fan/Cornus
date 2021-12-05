### Cornus - a fast file browser for KDE Linux written in C++17 and Qt5.

##### Requirements: Linux 5.5+, Qt 5.15.2+
---
Building on Ubuntu:
* sudo apt-get install qt5-default mkvtoolnix libdbus-c++-dev libudev-dev libudisks2-dev libdconf-dev cmake git ark libmtp-dev liburing-dev
* mkdir build && cd build
* cmake ..
* make -j4

The file browser executable is "cornus", "cornus_io" is the IO daemon that is started automatically when needed.
To have a desktop launcher for this app - update the file at export/cornus_mas.desktop (specifically its "Icon" and "Exec" fields) according to where you have the "cornus" executable and copy this file to your desktop folder.

##### Big TODO items:
* Implement MTP to deal with Android devices
* Improve the IO daemon GUI pane.

##### Application Shortcuts:
* Ctrl+T => Open a new tab
* Alt+Up => Go one directory up
* Ctrl+H => Toggle showing of hidden files
* Ctrl+Q => Quit app
* Shift+Delete => Delete selected files permanently, no confirmation asked.
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


##### Things you should know:
* Files with the executable bit set are marked green.
* To set a default app to open a given type of file right click a file -> Open With -> Preferences...
* To see the tree of links of a symbolic link click the icon of the link.
* Press 'D' after selecting a non-folder to display its contents (or click its icon). The built-in text editor is meant for a quick update of its contents or a glean into the file, not as a full blown text editor. Files' contents unrecognized as text files are opened in read-only mode.
* Shift+Delete = delete permanently, Delete = move to trash.
* A grey dot near the icon of the file means the file has extended attributes,
a blue one means it has media related extended attributes.
* Launching Cornus from the command line and getting it to select a folder at startup, examples:<br/>
To select the folder "Documents" from ${HOME} execute:<br/>
`cornus ${HOME} --select Documents`<br/>
or<br/>
`cornus ${HOME} --select "My Folder With Whitespaces"`<br/>
To select "File.txt" from ${HOME} execute:</br>
`cornus ${HOME}/File.txt`<br/>
or:<br/>
`cornus ${HOME} --select File.txt`<br/>
or:<br/>
`cornus ${HOME} --select "My File With Whitespaces.txt"`<br/>
* You can easily set your own icons for files of different types by dropping an icon into the "file_icons" folder with the proper extension as its name, browse the folder "file_icons" to see what I mean, probably located at "/usr/share/cornus/file_icons".
* "cornus_io" is the I/O daemon that is started automatically, keep it in the same folder as "cornus".
* File icons are loaded from "$HOME/.config/CornusMas/file_icons", then from "/usr/share/cornus/file_icons", the icons from the former folder are used if both folders contain files with equal names.
* Supports marking movie files rip quality, codec, resolution, actors, etc.
For this first go to the settings menu -> "Media Database" and fill in with the actors, directors etc you might ever care about. Then you can apply any of these to any movie by selecting the movie file and pressing "m", the end result:
### Movie Dialog Screenshot:

![](resources/movie_file_attributes.webp)

Media search: Ctrl + M
The settings are saved using extended file attributes and take up extemely little space.
At some point will support search for movie files inside a folder by any given category (actor, director, genre, etc).
