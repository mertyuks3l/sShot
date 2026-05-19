<div align="center">
<img src="data/sshot_icon.svg" alt="Sshot Icon" width="100" height="100">

# sShot 
Minimalist screenshot tool
</div>

sShot is a minimalist screenshot tool written in C using the SDL3 library. It works on both Wayland and X11. It allows users to take screenshots of their entire screen or a selected area, save them in PNG format, and copy them to the clipboard. The application is designed to be simple, minimal and easy to use. 

## Preview
![sShot Preview](data/preview.gif)

## Index
- [Features](#features)
- [Installation](#installation)
- [Building from source](#building-from-source)
- [Usage](#usage)
- [Contributing](#contributing)
- [License](#license)

## Features
- Take screenshots of the entire screen or a selected area
- Save screenshots in PNG format
- Save screenshots to a specified directory
- Copy screenshots to the clipboard
- Simple and easy to use interface

## Installation
Pre-built binaries(including AppImage) for sShot are available on the [Releases](https://github.com/MertYksl03/sShot/releases) page. You can download the latest release and follow the installation instructions provided in the release notes.


## Building from source
1. Install the required dependencies:
   #### On Debian/Ubuntu:

   ```bash
   sudo apt-get install libsdl3-dev libsdl3-image-dev libnotify-dev
   ```

   > [!NOTE]   
   > On Debian/Ubuntu, you may need to add the `experimental` repository to get the latest SDL3 packages. Or you can build SDL3 from source if you    > prefer. See the [SDL3 installation guide](https://github.com/libsdl-org/SDL/blob/main/INSTALL.md) for more details. 
   >

   #### On Arch Linux:

   ```bash
   sudo pacman -S sdl3 sdl3_image libnotify
   ```

2. Clone the repository:
   ```bash
   git clone https://github.com/MertYksl03/sShot.git
   ```
3. Navigate to the project directory:
   ```bash
   cd sShot
   ```
4. Build the project:
   ```bash
   make build 
   ```
5. Install the binary to your system:
   ```bash
   sudo make install
   ```

## Usage
To take a screenshot, simply run the `sshot` command in your terminal or from the application menu. You can choose to capture the entire screen or select a specific area. The captured screenshot will be saved in the specified directory and copied to the clipboard.

### On Multi-Monitor Setups

>[!NOTE]   
> On multi-monitor setups, sShot captures the entire virtual screen on X11, while on Wayland it captures the monitor where the application is launched. This is due to the limitations of the Wayland protocol, which does not allow applications to capture content from other monitors for security reasons.

### Keybindings
|Key|Action|
|-----|--------|
|<kbd>Left Click</kbd>|Start selecting an area to crop|
|<kbd>Right Click</kbd>|Crop the selected area|
|<kbd>Ctrl</kbd> + <kbd>S</kbd>|Save screenshot|
|<kbd>Ctrl</kbd> + <kbd>C</kbd>|Copy screenshot to clipboard|
|<kbd>Ctrl</kbd> + <kbd>Z</kbd>|Undo last action|
|<kbd>Mouse Wheel</kbd> |Zoom in and out|
|<kbd>Esc</kbd> or <kbd>Q</kbd>|Exit the application|

## Uninstallation
To uninstall sShot, run the following command in your terminal:
1. First navigate to the project directory:
   ```bash
   cd sShot
   ```
2. Then run the uninstall command:
    ```bash
    sudo make uninstall
    ```

## Contributing
Contributions are welcome! If you have any ideas for new features or improvements, please feel free to submit a pull request or open an issue.

## License
This project is licensed under the GNU General Public License v3. See the [LICENSE](LICENSE) file for details