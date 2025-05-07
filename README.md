# Street View Downloader

<div align="center">

```
░█▀▀░▀█▀░█▀▄░█▀▀░█▀▀░▀█▀░█░█░▀█▀░█▀▀░█░█
░▀▀█░░█░░█▀▄░█▀▀░█▀▀░░█░░▀▄▀░░█░░█▀▀░█▄█
░▀▀▀░░▀░░▀░▀░▀▀▀░▀▀▀░░▀░░░▀░░▀▀▀░▀▀▀░▀░▀
░█▀▄░█▀█░█░█░█▀█░█░░░█▀█░█▀█░█▀▄░█▀▀░█▀▄
░█░█░█░█░█▄█░█░█░█░░░█░█░█▀█░█░█░█▀▀░█▀▄
░▀▀░░▀▀▀░▀░▀░▀░▀░▀▀▀░▀▀▀░▀░▀░▀▀░░▀▀▀░▀░▀
```

**License:** MIT  
**C++ Standard:** C++17  
**Platform:** Windows | Linux | macOS

**A high-performance, multi-threaded Google Street View panorama downloader**

</div>

## 🌟 Features

- **Blazing Fast**: Multi-level parallelism with tile-level and panorama-level threading
- **Automatic Detection**: Identifies Street View generation (1-4) automatically
- **Directional Views**: Creates 8 rectilinear directional views (N, NE, E, SE, S, SW, W, NW)
- **Format Support**: Processes single PanoIDs or batch files (TXT, CSV with various delimiters)
- **Robust Error Handling**: Exponential backoff retry logic for network errors
- **Real-time Progress**: Live console progress tracking
- **CSV Cleaning**: Generates filtered CSV files with only successful PanoIDs
- **Cross-Platform**: Works on Windows, Linux, and macOS

## 📋 Requirements

- C++17 compatible compiler
- OpenCV 4.x
- libcurl
- CMake 3.10+
- Optional: Intel TBB for enhanced parallelism

## 🚀 Installation

### From Source

```bash
# Clone the repository
git clone https://github.com/yourusername/street-view-downloader.git
cd street-view-downloader

# Create build directory
mkdir build && cd build

# Configure
cmake ..

# Build
cmake --build . --config Release

# Install (optional)
cmake --install .
```

### Dependencies Installation

#### Ubuntu/Debian

```bash
sudo apt update
sudo apt install libopencv-dev libcurl4-openssl-dev cmake build-essential libtbb-dev
```

#### macOS

```bash
brew install opencv curl cmake libomp tbb
```

#### Windows

Install using [vcpkg](https://github.com/microsoft/vcpkg):

```powershell
vcpkg install opencv:x64-windows curl:x64-windows tbb:x64-windows
```

## 🖥️ Usage

### Basic Usage

Download a single panorama:

```bash
./streetview_downloader PanoID123456789
```

Process multiple panoramas from a file:

```bash
./streetview_downloader -f panoramas.txt
```

Specify output directory:

```bash
./streetview_downloader -f panoramas.csv -o ~/street_view_images
```

### Advanced Examples

Maximum performance (adjust thread counts based on your hardware):

```bash
./streetview_downloader -f panoramas.csv -t 256 -p 8 --max-threads 1024
```

Create directional views with tile labels for debugging:

```bash
./streetview_downloader PanoID123456789 --labels
```

Process a CSV file and generate a cleaned version without failed panoramas:

```bash
./streetview_downloader -f input.csv --clean-csv cleaned_output.csv
```

Skip automatic cropping and generation labeling:

```bash
./streetview_downloader -f panoramas.txt --no-crop --no-gen-suffix
```

## ⚙️ Command Line Options

| Option | Description |
|--------|-------------|
| `[PANOID]` | Single PanoID to download |
| `-f, --file FILE` | File containing PanoIDs (one per line or CSV) |
| `-o, --output DIR` | Output directory for saved panoramas (default: ~/streetview_output) |
| `--clean-csv [FILE]` | Create cleaned CSV file with failed panoramas removed |
| `-t, --tile-threads N` | Number of download threads per panorama (default: 128) |
| `-p, --pano-threads N` | Number of panoramas to process concurrently (default: 4) |
| `--max-threads N` | Maximum total number of threads (default: 512) |
| `--timeout N` | Download timeout in seconds (default: 10) |
| `--retries N` | Number of download retries (default: 3) |
| `--no-gen-suffix` | Do not include generation in filename |
| `--no-crop` | Do not auto-crop panoramas |
| `--no-skip` | Do not skip existing files |
| `--labels` | Draw tile labels (x,y,zoom) |
| `--no-directional` | Do not create directional views |
| `-h, --help` | Show help message |

## 📁 Output Format

By default, the program creates:

1. **Directional Views**: 8 rectilinear views (90° FOV) for each panorama:
   - Filename format: `[PanoID]_View[1-8]_[Direction]_FOV90.0.jpg`
   - Directions: N, NE, E, SE, S, SW, W, NW
   - Resolution: 512×512 pixels

### Example Output Files

```
PanoID123456789_View1_N_FOV90.0.jpg
PanoID123456789_View2_NE_FOV90.0.jpg
PanoID123456789_View3_E_FOV90.0.jpg
PanoID123456789_View4_SE_FOV90.0.jpg
PanoID123456789_View5_S_FOV90.0.jpg
PanoID123456789_View6_SW_FOV90.0.jpg
PanoID123456789_View7_W_FOV90.0.jpg
PanoID123456789_View8_NW_FOV90.0.jpg
```

## 📊 Generation Types

The program automatically detects Street View panorama generations:

| Generation | Zoom Level | Grid Size | Description |
|------------|------------|-----------|-------------|
| 1 | 3 | 8×4 | Older panoramas (~2007-2014) |
| 2 | 4 | 13×6 | Intermediate panoramas (~2014-2017) |
| 3 | 4 | 13×7 | Higher resolution panoramas (~2017-2020) |
| 4 | 4 | 16×8 | Current generation panoramas (2020+) |

## 📄 CSV File Support

The program supports various CSV formats:

- Simple text files with one PanoID per line
- CSV files with comma, semicolon, or tab delimiters
- Automatic detection of PanoID column based on header names
- Headers like "panoid", "pano_id", "panorama_id", "id" are recognized automatically

## 🛠️ Build Options

For even higher performance, you can enable Intel TBB support:

```bash
cmake -DUSE_TBB=ON ..
```

## 📝 Logging

The program creates a detailed log file (`streetview_downloader.log`) in the working directory with timestamps for all operations.

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## 📜 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgements

- OpenCV for image processing capabilities
- libcurl for HTTP requests
- Intel TBB for parallel algorithms (optional)

---

<div align="center">
  <sub>Built with ❤️ by Your Name</sub>
</div>