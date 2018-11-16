
Debian
====================
This directory contains files used to package rebelliousd/rebellious-qt
for Debian-based Linux systems. If you compile rebelliousd/rebellious-qt yourself, there are some useful files here.

## rebellious: URI support ##


rebellious-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install rebellious-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your rebelliousqt binary to `/usr/bin`
and the `../../share/pixmaps/rebellious128.png` to `/usr/share/pixmaps`

rebellious-qt.protocol (KDE)

