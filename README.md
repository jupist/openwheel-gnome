# OpenWheel

**OpenWheel** is a collection of tools designed to handle ASUS Dial and similar designware hardware under Linux. Whether you're a developer, hobbyist, or hardware enthusiast, OpenWheel provides a robust solution for utilizing and customizing the functionality of these devices on Linux systems.

---

## Table of Contents
- [Features](#features)
- [Getting Started](#getting-started)
- [Installation](#installation)
- [Usage](#usage)
- [Configuration](#configuration)
- [Compatibility](#compatibility)
- [Contributing](#contributing)
- [License](#license)

---

## Features

- **Comprehensive Support**: Works with ASUS Dial and a range of similar designware hardware.
- **Customizable**: Offers flexibility for personalizing hardware functionality to suit your needs.
- **Cross-Platform Development**: Built in C++ with modular components, ensuring efficient functionality.
- **QML Interface**: Includes user-friendly graphical elements using QML.
- **Lightweight and Modular**: Clean, efficient, and designed with modularity in mind.

---

## Getting Started

These instructions will guide you on how to set up and use OpenWheel on your Linux machine.

### Prerequisites
- **Linux Kernel**: Ensure your system is running a modern Linux kernel (v5.4 or later recommended).
- **Development Tools**:
  - A C++17 compatible compiler.
  - CMake version 3.10 or newer.
- **Libraries**:
  - Qt (for systems requiring the QML graphical interface).
  - libevdev: For handling input devices.

To verify your system has these components, you can run:
```bash
gcc --version
cmake --version
```

---

## Installation

1. **Clone the Repository**:
   ```bash
   git clone https://github.com/fredaime/openwheel.git
   cd openwheel
   ```

2. **Build the Project**:
   ```bash
   mkdir build
   cd build
   cmake ..
   make
   ```

3. **Install**:
   Install the compiled software:
   ```bash
   sudo make install
   ```

---

## Usage

Once installed, OpenWheel can be used to interact with ASUS Dial hardware. Below are some basic commands and workflows:

1. **Detect Supported Hardware**:
   ```bash
   openwheel --detect
   ```

2. **Initialize a Device**:
   ```bash
   sudo openwheel --init
   ```

3. **Customize Behavior**:
   Edit configuration files or use the graphical interface:
   ```bash
   openwheel-gui
   ```

4. **View Help**:
   ```bash
   openwheel --help
   ```

---

## Configuration

OpenWheel is designed to be highly configurable. By default, it searches for configuration files under:
`~/.config/openwheel/`

### Example Configuration

Here's an example configuration for setting up actions for the hardware:

```json
{
  "device": "ASUS Dial",
  "actions": [
    {"wheel_action": "scroll", "speed": "fast"},
    {"wheel_action": "zoom", "sensitivity": "medium"}
  ]
}
```

---

## Compatibility

OpenWheel has been explicitly tested with:
- **ASUS Dial**
- Other Designware-compatible hardware

If your device isn't listed but shares similar hardware specifications, you can still attempt to use OpenWheel. For hardware-specific bug reports, create an issue on [GitHub](https://github.com/fredaime/openwheel/issues).

---

## Contributing

We welcome contributions from the community! Here's how you can help:
1. Fork the repository.
2. Create a new branch for your feature:
   ```bash
   git checkout -b feature-name
   ```
3. Make and test your changes.
4. Submit a pull request.

Please adhere to the [contribution guidelines](CONTRIBUTING.md).

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

This README provides a complete overview of OpenWheel. If you encounter any issues or have suggestions, feel free to contribute or open a discussion on our [GitHub Discussions](https://github.com/fredaime/openwheel/discussions).

Happy Hacking!
