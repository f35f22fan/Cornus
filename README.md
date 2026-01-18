### Cornus - a fast file browser for KDE Linux written in C++17 and Qt6.

##### Requirements: Linux 5.5+, Qt 6.10+
---

* (the qt6 development package)

Then the other dependencies:
* sudo apt-get install libwebm-dev libdbus-c++-dev libudev-dev libudisks2-dev libzstd-dev libmtp-dev libpolkit-gobject-1-dev libpolkit-agent-1-dev mkvtoolnix cmake ark git

And build it:
* mkdir build && cd build
* cmake ..
* make -j4

The file browser executable is "cornus", "cornus_io" is the IO daemon that is started automatically when needed.
To have a .desktop launcher on your desktop - update the file at export/cornus_mas.desktop (the "Icon" and "Exec" fields) accordingly and copy this file to your desktop folder.

### Screenshot with dark theme:
![](resources/Screenshot_dark.png)

### Screenshot with light theme:
![](resources/Screenshot_light.png)

#### Things you should know:
* Files with the executable bit set are marked green.
* To set a default app to open a given type of file right click a file -> Open With -> Preferences...
* To see the tree of links of a symbolic link - when in details view click the icon of the link.
* Shift+Delete = delete permanently, Delete = move to trash.
* To launch Cornus from the command line and get it to select a folder at startup:<br/>
To select the folder "Documents" from ${HOME}:<br/>
`cornus ${HOME} --select Documents`<br/>
or<br/>
`cornus ${HOME} --select "My Folder With Whitespaces"`<br/>
To select "File.txt" from ${HOME}:</br>
`cornus ${HOME}/File.txt`<br/>
or:<br/>
`cornus ${HOME} --select File.txt`<br/>
or:<br/>
`cornus ${HOME} --select "My File With Whitespaces.txt"`<br/>

* File icons are loaded from "$HOME/.config/CornusMas/file_icons", then if not found from "/usr/share/cornus/file_icons".

