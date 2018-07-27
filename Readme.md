[![Codacy Badge](https://api.codacy.com/project/badge/Grade/685785c9c6ff46acaa0750cdc466565b)](https://app.codacy.com/app/cgzones/ctguard?utm_source=github.com&utm_medium=referral&utm_content=cgzones/ctguard&utm_campaign=badger)
[![GitHub License](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/cgzones/ctguard/master/LICENSE)
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/5141c7ed9f7249db8d5d4f217ed2b788)](https://www.codacy.com/app/cgzones/ctguard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=cgzones/ctguard&amp;utm_campaign=Badge_Grade)

# ctguard

- [Overview](#overview)
- [Dependencies](#dependencies)
- [Installation](#installation)
- [License](#license)
- [Third-Party Libraries](#third-party-libraries)

## Overview

ctguard is a small host-based intrusion detection system ([hids](https://en.wikipedia.org/wiki/Host_based_intrusion_detection_system)) inspired by [ossec](https://www.ossec.net/).

Its current features are:

- rule based log analysis
- detection of filesystem object changes/additions/deletions
- intervening with custom actions

## Dependencies

To build ctguard, a c++17 compliant compiler and standard library is needed, e.g:

- GCC 7 and up
- Clang 5.0 and up

Also sqlite3 develepment headers are needed, on Debian supplied by the package `libsqlite3-dev`.

To build the manpages, [asciidoctor](https://asciidoctor.org/) is required.

## Installation

### Debian

The simplest way to install ctguard on Debian (and derivates) is to build a package, by using the command `debuild -us -uc` from the package `devscripts`.
This generates a Debian package in the parent directory.
Simply install it with `dpkg -i ../ctguard_*.deb`.

ctguard can then be later deinstalled by `apt purge ctguard`.

### Generic

The first step is to build ctguard.
Run `make USERMODE=1 -j binaries` in the ctguard directory as normal user.
Then install run as root or via sudo `make full-install`.
Finally enable and start ctguard: `systemctl enable ctguard-*` and `systemctl start ctguard-*`.

To deinstall ctguard first stop its services: `systemctl stop ctguard-*`.
Now delete the remaining files: `rm -Rf /var/lib/ctguard /etc/ctguard /usr/sbin/ctguard-* /lib/systemd/system/ctguard-*`.

## License

ctguard is licensed under the [MIT License](opensource.org/license/MIT):

MIT License

Copyright &copy; 2018 Christian Göttsche

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## Third-Party Libraries

ctguard uses three third-party libraries:

- [dtl](https://github.com/cubicdaiya/dtl): diff template library licensed under the "BSD License"
- [cereal](https://uscilab.github.io/cereal/): serialization library licensed under the "BSD License"
- [sha2 by Aaron D. Gifford](https://www.aarongifford.com/computers/sha.html): a sha2 implementation licensed under the "BSD License"
