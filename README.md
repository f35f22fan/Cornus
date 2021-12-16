### Cornus - a fast file browser for KDE Linux written in C++17 and Qt5.

##### Requirements: Linux 5.5+, Qt 5.15.2+
---
Building on Ubuntu, install Qt5 (used to be called qt5-default):
* sudo apt-get install qtbase5-dev qtchooser qt5-qmake qtbase5-dev-tools

Then the other dependencies:
* sudo apt-get install mkvtoolnix libdbus-c++-dev libudev-dev libudisks2-dev libdconf-dev cmake git ark libmtp-dev liburing-dev

And build it:
* mkdir build && cd build
* cmake ..
* make -j4

The file browser executable is "cornus", "cornus_io" is the IO daemon that is started automatically when needed.
To have a desktop launcher for this app - update the file at export/cornus_mas.desktop (specifically its "Icon" and "Exec" fields) according to where you have the "cornus" executable and copy this file to your desktop folder.

##### Major TODO items:
* Implement MTP to deal with Android devices
* Implement Icons View mode.

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


#### Things you should know:
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
#### Movie Dialog Screenshot:

![](resources/movie_file_attributes.webp)

Media search: Ctrl + M
The settings are saved using extended file attributes and take up extemely little space.
At some point will support search for movie files inside a folder by any given category (actor, director, genre, etc).


#### Trash Can Support
Since the official XDG trash specification is over engineered, slow and doesn't prevent dangling files, Cornus follows a different approach which allows for a very fast, efficient and very simple trash can implementation.

The official XDG trash can spec:
* mandates creating 2 files per file deleted
* files can be placed into random partitions which doesn't prevent copying of files.
* Undeleting files is thus rather I/O expensive since one needs to read *all* files from *all* trash folders, processing them and only then proceeding undeleting files, potentially involving copying (instead of atomic moving).

The Cornus file browser places deleted files in a hidden folder at same location and puts the meta in its title, thus the pros:

* avoids ever copying files
* no opening/reading/closing of files
* no 2 files (info file + the file itself) per deleted file.
* no dangling files (when the user deletes a folder he automatically also deletes its trash if any)
* listing files to undelete is instant
* is very easy to group deleted files into batches sorted by deletion time and give the user the option to choose which batch to undelete, a feature implemented (only) by Cornus.
* no need to sort out which files belong to a folder and which ones don't
* no filename clashes thus no need to implemenent name clash prevention
* extremely simple implementation

And one big con:
* Some users might not want to have a trash folder in the same folder where the files were deleted even if the trash is hidden.

However the latter is somewhat leveraged in cases of git projects because Cornus automatically adds an entry to the <b>global</b> gitignore file thus:
* a) all git projects will be unaffected by a potentially unwanted hidden folder.
* b) no need to touch the .gitignore file of any projects
* c) the user doesn't have to do anything at all.
