# gbsplay - A Game Boy sound player

[![Code coverage status](https://codecov.io/github/mmitch/gbsplay/coverage.svg)](https://codecov.io/github/mmitch/gbsplay?branch=master)
[![Linux Build](https://github.com/mmitch/gbsplay/actions/workflows/build_linux.yml/badge.svg)](https://github.com/mmitch/gbsplay/actions/workflows/build_linux.yml)
[![FreeBSD Build](https://github.com/mmitch/gbsplay/actions/workflows/build_freebsd.yml/badge.svg)](https://github.com/mmitch/gbsplay/actions/workflows/build_freebsd.yml)
[![macOS Build](https://github.com/mmitch/gbsplay/actions/workflows/build_macos.yml/badge.svg)](https://github.com/mmitch/gbsplay/actions/workflows/build_macos.yml)
[![Windows Build](https://github.com/mmitch/gbsplay/actions/workflows/build_windows.yml/badge.svg)](https://github.com/mmitch/gbsplay/actions/workflows/build_windows.yml)

This program emulates the sound hardware of the Nintendo Game Boy.  It
is able to play the sounds from a Game Boy module dump (.GBS format, as
well as the older .GBR format), a subset of plain Game Boy rom dumps (.GB
format; button input and graphics are not supported) and Game Boy Video
Game Music dumps (.VGM/.VGZ format).

Homepage/Repo:   https://github.com/mmitch/gbsplay/
Bug reports:     please use the issue tracker at
                 https://github.com/mmitch/gbsplay/issues

The gbsplay package contains the following parts:

 * `gbsplay`:    a console player
 * `gbsinfo`:    displays information about a gbs file
 * `gbs2gb`:     convert a gbs file to gb to play it on real hardware
 * `xgbsplay`:   an X11 player (need to be enabled during configure)

## Packaging status

[![Packaging status](https://repology.org/badge/vertical-allrepos/gbsplay.svg)](https://repology.org/project/gbsplay/versions)

## License

```
(C) 2003-2026 by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
                 Christian Garbs <mitch@cgarbs.de>
                 Maximilian Rehkopf <otakon@gmx.net>
                 Vegard Nossum <vegardno@ifi.uio.no>
                 Lisa Riedler <dev@riedler.wien>

Source Code licensed under GNU GPL v1 or, at your option, any later version.

Individual copyright notices can be found in the file headers.
The copyright notice for each file should be in the following form:

"(C) 20xx-20yy by List of File Major Contributors"

Where 20xx is the first release year for the file, 20yy is the most
recent release year for the file.

For detailed author attribution, see the revision history of each file.

Additionally, the following files are included under different licenses:

File:       examples/nightmode.gbs
Copyright:  Laxity of Dual Crew Shining (Nightmode Demo)
License:    Public Domain
Homepage:   http://www.dc-s.com/

File:       crc32.c
Copyright:  Provided to GNUnet by peter@horizon.com
License:    Public Domain
Homepage:   n/a
```
