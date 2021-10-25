# Matrix32
A port of jwz's xmatrix module for xscreensaver to Win32. The goal is to provide a Windows version of this excellent screensaver for those of us unfortunate to have to deal with Windows during the day but still want a cool screensaver.

https://sourceforge.net/projects/matrix32/

# Goal of this fork
The aim of this fork is to make the code buildable using tools slightly more modern than VC6.
Ideally we would be building with the windows DDK, screen savers being system components they shouldn't require additional runtimes.
We would also build for Win64, both for x86_64 and arm64 (not a priority this last one).

The second phase of this project would be to update the xmatrix code to be in line with more recent Xscreensaver releases.
I see the trace program is missing, for instance.

The final phase would be to package it for Windows store and attempt publishing.
