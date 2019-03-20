# wlmenu

A simple and fast application launcher for wayland.

## Motivation

## Installation

### Dependencies

* wayland 1.16
* cairo 1.16
* libxkbcommon-x11 0.8.3
* freetype2 2.9.1

### Compilation

    $ git clone https://github.com/stnuessl/wlmenu
    $ cd wlmenu
    $ make CC=gcc

## Usage

### Command-Line
    $ wlmenu --help

### Configuration File

The configuration file template below can be adjusted to your needs.
If the file "~/.config/wlmenu/config" exists, wlmenu will automatically
load its configuration from it.

```
#
# wlmenu configuration file
#

[ default ]

# Default window foreground color
foreground = 0xFF8700FF

# Default window background color
background = 0x282828FF

# Default window border color
border     = 0xFF8700FF

[ focus ]

# Foreground color of selected menu entry
foreground = 0x282828FF

# Background color of selected menu entry
background = 0xFF8700FF

[ gui ]

# Amount of rows the selection menu will display
rows = 20

[ font ]

# Font to use. Absolut path to a specific font file required.
font = /usr/share/fonts/TTF/Hack-Regular.ttf

# Font size
size = 16.0
```

