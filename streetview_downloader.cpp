#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <chrono>
#include <ctime>
#include <cmath>
#include <random>
#include <algorithm>
#include <atomic>
#include <memory>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <cstring>
#include "fixerrors.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#endif

// Include libraries for HTTP requests, image processing, and threading
#include <curl/curl.h>
#include <opencv2/opencv.hpp>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(dir) _mkdir(dir)
#else
#include <sys/stat.h>
#define MKDIR(dir) mkdir(dir, 0777)
#endif

// Optional: Use TBB for even faster parallel processing
#ifdef USE_TBB
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/parallel_for_each.h>
#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/blocked_range2d.h>
#include <oneapi/tbb/task_group.h>
#endif

namespace fs = std::filesystem;

// Thread-safe logging class
class Logger {
private:
    std::mutex log_mutex;
    std::ofstream log_file;
    bool console_output;

public:
    Logger(const std::string& filename, bool console = true) : console_output(console) {
        log_file.open(filename, std::ios::app);
    }

    ~Logger() {
        if (log_file.is_open()) {
            log_file.close();
        }
    }

    void log(const std::string& message) {
        std::lock_guard<std::mutex> lock(log_mutex);

        // Get current time
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);

        std::stringstream timestamp;
        timestamp << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");

        std::string formatted_message = timestamp.str() + " - INFO - " + message;

        // Write to file
        if (log_file.is_open()) {
            log_file << formatted_message << std::endl;
        }

        // Output to console if enabled
        if (console_output) {
            std::cout << formatted_message << std::endl;
        }
    }
};

// CSV handling class for different CSV formats
class CSVHandler {
private:
    char delimiter;
    std::string file_path;
    std::vector<std::string> headers;
    std::vector<std::vector<std::string>> rows;
    bool has_headers;
    int panoid_column_index;

    // Detect the delimiter used in a CSV file
    char detect_delimiter(const std::string& sample_line) {
        // Count occurrence of common delimiters
        int comma_count = std::count(sample_line.begin(), sample_line.end(), ',');
        int semicolon_count = std::count(sample_line.begin(), sample_line.end(), ';');
        int tab_count = std::count(sample_line.begin(), sample_line.end(), '\t');

        // Return the most common delimiter
        if (semicolon_count > comma_count && semicolon_count > tab_count) {
            return ';';
        }
        else if (tab_count > comma_count && tab_count > semicolon_count) {
            return '\t';
        }
        else {
            return ',';  // Default to comma
        }
    }

    // Split a line by the delimiter
    std::vector<std::string> split_line(const std::string& line) {
        std::vector<std::string> result;
        std::stringstream ss(line);
        std::string item;

        while (std::getline(ss, item, delimiter)) {
            // Trim whitespace
            item.erase(0, item.find_first_not_of(" \t\r\n"));
            item.erase(item.find_last_not_of(" \t\r\n") + 1);
            result.push_back(item);
        }

        return result;
    }

    // Determine the column index for PanoID
    int find_panoid_column() {
        if (!has_headers) {
            return 0;  // Assume the first column if no headers
        }

        // Look for common header names for PanoID
        for (size_t i = 0; i < headers.size(); i++) {
            std::string header = headers[i];
            std::transform(header.begin(), header.end(), header.begin(), ::tolower);

            if (header == "panoid" || header == "pano_id" || header == "panorama_id" ||
                header == "panoramaid" || header == "pano id" || header == "id") {
                return static_cast<int>(i);
            }
        }

        return 0;  // Default to first column if no match found
    }

    // Validate if a string is a valid Google Street View PanoID
    bool is_valid_panoid(const std::string& str) const {
        // Google Street View PanoIDs are typically 22 characters
        if (str.length() != 22) {
            return false;
        }

        // They contain alphanumeric characters and underscores/hyphens
        for (char c : str) {
            if (!std::isalnum(c) && c != '_' && c != '-') {
                return false;
            }
        }

        return true;
    }

    // Extract just the PanoID from a potentially longer string (with semicolons, etc.)
    std::string extract_panoid(const std::string& str) const {
        // If the string already looks like a valid PanoID, return it
        if (is_valid_panoid(str)) {
            return str;
        }

        // Otherwise, try to extract a valid PanoID from the beginning
        // This handles formats where semicolons are part of the content after the PanoID
        if (str.length() >= 22) {
            std::string potential_panoid = str.substr(0, 22);
            if (is_valid_panoid(potential_panoid)) {
                return potential_panoid;
            }
        }

        // If not found, just return the original string
        return str;
    }

public:
    CSVHandler(const std::string& path) : file_path(path), has_headers(true), panoid_column_index(0) {
        load_csv();
    }

    void load_csv() {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            throw std::runtime_error("Could not open file: " + file_path);
        }

        // Read the first line to detect delimiter
        std::string first_line;
        if (std::getline(file, first_line)) {
            delimiter = detect_delimiter(first_line);

            // Parse headers
            headers = split_line(first_line);

            // Read data rows
            std::string line;
            while (std::getline(file, line)) {
                if (!line.empty()) {
                    rows.push_back(split_line(line));
                }
            }
        }

        file.close();

        // Find PanoID column
        panoid_column_index = find_panoid_column();
    }

    // Get all PanoIDs from the CSV
    std::vector<std::string> get_panoids() {
        std::vector<std::string> panoids;

        for (const auto& row : rows) {
            if (row.size() > static_cast<size_t>(panoid_column_index)) {
                // Extract just the PanoID part (not anything after semicolons)
                std::string panoid = extract_panoid(row[panoid_column_index]);
                if (!panoid.empty()) {
                    panoids.push_back(panoid);
                }
            }
        }

        return panoids;
    }

    // Get all rows with their PanoIDs
    std::map<std::string, std::vector<std::string>> get_rows_with_panoids() {
        std::map<std::string, std::vector<std::string>> result;

        for (const auto& row : rows) {
            if (row.size() > static_cast<size_t>(panoid_column_index)) {
                std::string panoid = extract_panoid(row[panoid_column_index]);
                if (!panoid.empty()) {
                    result[panoid] = row;
                }
            }
        }

        return result;
    }

    // Write a new CSV without the specified failed PanoIDs
    bool write_cleaned_csv(const std::set<std::string>& failed_panoids, const std::string& output_path) {
        std::ofstream out_file(output_path);
        if (!out_file.is_open()) {
            return false;
        }

        // Write headers
        if (has_headers) {
            for (size_t i = 0; i < headers.size(); i++) {
                out_file << headers[i];
                if (i < headers.size() - 1) {
                    out_file << delimiter;
                }
            }
            out_file << "\n";
        }

        // Write rows, excluding failed PanoIDs
        for (const auto& row : rows) {
            if (row.size() > static_cast<size_t>(panoid_column_index)) {
                std::string raw_panoid = row[panoid_column_index];
                std::string panoid = extract_panoid(raw_panoid);

                if (failed_panoids.find(panoid) == failed_panoids.end()) {
                    // This PanoID didn't fail, include it
                    for (size_t i = 0; i < row.size(); i++) {
                        out_file << row[i];
                        if (i < row.size() - 1) {
                            out_file << delimiter;
                        }
                    }
                    out_file << "\n";
                }
            }
        }

        out_file.close();
        return true;
    }

    // Getters
    int get_panoid_column_index() const { return panoid_column_index; }
    char get_delimiter() const { return delimiter; }
    bool has_header_row() const { return has_headers; }
    size_t row_count() const { return rows.size(); }
    std::string get_file_path() const { return file_path; }
};

// Structure to hold configuration for each generation
struct GenerationConfig {
    int zoom;
    int max_x;
    int max_y;
    bool crop;
};

// Structure for tile information
struct Tile {
    int x;
    int y;
    cv::Mat image;
    bool valid;

    Tile() : x(0), y(0), valid(false) {}
    Tile(int x_, int y_) : x(x_), y(y_), valid(false) {}
};

// Memory write callback for CURL
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* buffer) {
    size_t total_size = size * nmemb;
    buffer->append(static_cast<char*>(contents), total_size);
    return total_size;
}

// Thread pool implementation for optimal parallel processing
class ThreadPool {
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;

public:
    ThreadPool(size_t threads) : stop(false) {
        for (size_t i = 0; i < threads; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex);
                        condition.wait(lock, [this] { return stop || !tasks.empty(); });
                        if (stop && tasks.empty()) {
                            return;
                        }
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    task();
                }
                });
        }
    }

    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type> {
        using return_type = typename std::result_of<F(Args...)>::type;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> result = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (stop) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            tasks.emplace([task]() { (*task)(); });
        }
        condition.notify_one();
        return result;
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread& worker : workers) {
            worker.join();
        }
    }
};

// Progress bar class to display and update download progress
class ProgressBar {
private:
    int total;
    int console_width;
    std::mutex display_mutex;
    bool visible;

    // Get console width for appropriate formatting
    int get_console_width() {
#ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
#else
        struct winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        return w.ws_col;
#endif
        return 80; // Default fallback width
    }

    // Move cursor to the bottom of the console
    void move_cursor_to_bottom() {
#ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
        COORD coord;
        coord.X = 0;
        coord.Y = csbi.srWindow.Bottom;
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
#else
        // Move to bottom of visible screen
        std::cout << "\033[" << get_terminal_height() - 1 << ";0H";
#endif
    }

    // Get terminal height
    int get_terminal_height() {
#ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
        return csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
#else
        struct winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        return w.ws_row;
#endif
        return 24; // Default fallback height
    }

    // Clear the current line
    void clear_line() {
#ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
        DWORD written;
        FillConsoleOutputCharacterA(
            GetStdHandle(STD_OUTPUT_HANDLE), ' ',
            csbi.dwSize.X, { 0, csbi.dwCursorPosition.Y },
            &written
        );
        SetConsoleCursorPosition(
            GetStdHandle(STD_OUTPUT_HANDLE),
            { 0, csbi.dwCursorPosition.Y }
        );
#else
        std::cout << "\033[2K\r"; // Clear the line and move to the beginning
#endif
    }

public:
    ProgressBar(int total_count) : total(total_count), visible(false) {
        console_width = get_console_width();
    }

    ~ProgressBar() {
        // Ensure we leave the terminal in a clean state
        if (visible) {
            hide();
        }
    }

    void update(int completed, int successful, int failed) {
        std::lock_guard<std::mutex> lock(display_mutex);

        // Calculate progress percentage
        double progress = (total > 0) ? (double)completed / total : 0;
        int bar_width = std::min(50, console_width - 35); // Reserve space for text

        // Save cursor position
#ifndef _WIN32
        std::cout << "\033[s"; // Save cursor position
#endif

        // Move to bottom of console
        move_cursor_to_bottom();
        clear_line();

        // Format progress bar
        std::cout << "[";
        int pos = bar_width * progress;
        for (int i = 0; i < bar_width; ++i) {
            if (i < pos) std::cout << "=";
            else if (i == pos) std::cout << ">";
            else std::cout << " ";
        }

        // Show progress stats
        std::cout << "] " << static_cast<int>(progress * 100.0) << "% ";
        std::cout << completed << "/" << total << " (";
        std::cout << successful << " success, " << failed << " failed)";

        std::cout.flush();
        visible = true;

        // Restore cursor position in Unix-like systems
#ifndef _WIN32
        std::cout << "\033[u"; // Restore cursor position
#endif
    }

    void hide() {
        if (!visible) return;

        std::lock_guard<std::mutex> lock(display_mutex);
#ifndef _WIN32
        std::cout << "\033[s"; // Save cursor position
#endif

        move_cursor_to_bottom();
        clear_line();

#ifndef _WIN32
        std::cout << "\033[u"; // Restore cursor position
#endif
        std::cout.flush();
        visible = false;
    }
};

// Main Street View Downloader class
class StreetViewDownloader {
private:
    // Configuration
    int retry_count;
    int timeout_value;
    int tile_thread_count;
    int pano_thread_count;
    int max_total_threads;
    bool include_gen_in_filename;
    bool auto_crop;
    bool skip_existing;
    bool draw_tile_labels;
    bool create_directional_views;
    bool clean_csv_output;
    std::string csv_output_path;

    // Threading resources
    std::shared_ptr<ThreadPool> thread_pool;
    std::mutex progress_lock;
    std::mutex cache_lock;
    std::mutex failed_panoids_mutex;
    std::atomic<int> download_progress;
    std::atomic<int> active_threads;
    std::shared_ptr<ProgressBar> progress_bar;

    // CURL setup for HTTP requests
    CURL* curl_handle;
    struct curl_slist* headers;

    // Generation cache to avoid redundant detection
    std::unordered_map<std::string, std::pair<int, std::string>> generation_cache;

    // Store failed panoramas for CSV cleanup
    std::set<std::string> failed_panoids;

    // CSV data for input/output
    std::shared_ptr<CSVHandler> csv_handler;

    // Logger
    std::shared_ptr<Logger> logger;

    // Random generator for jitter
    std::mt19937 random_engine;

    // Method to initialize CURL with common settings
    CURL* init_curl() {
        CURL* handle = curl_easy_init();
        curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(handle, CURLOPT_TIMEOUT, timeout_value);
        curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteCallback);
        return handle;
    }

    // Check if a tile is valid (not completely black)
    bool is_valid_tile(const cv::Mat& img) {
        if (img.empty() || img.cols < 10 || img.rows < 10) {
            return false;
        }

        // Convert to grayscale for faster processing
        cv::Mat gray;
        cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);

        // Check if the image is completely black (all values are 0)
        cv::Scalar mean = cv::mean(gray);
        return mean[0] > 0.1; // Threshold slightly above 0 to account for compression artifacts
    }

    // Get cached generation if available
    std::pair<int, std::string> get_cached_generation(const std::string& panoid) {
        std::lock_guard<std::mutex> lock(cache_lock);
        auto it = generation_cache.find(panoid);
        if (it != generation_cache.end()) {
            return it->second;
        }
        return { 0, "" };
    }

    // Cache generation for future use
    void cache_generation(const std::string& panoid, int generation, const std::string& description) {
        std::lock_guard<std::mutex> lock(cache_lock);
        generation_cache[panoid] = { generation, description };
    }

    // Record a failed panorama
    void record_failed_pano(const std::string& panoid) {
        std::lock_guard<std::mutex> lock(failed_panoids_mutex);
        failed_panoids.insert(panoid);
    }

    // Detect Street View panorama generation
    std::pair<int, std::string> detect_generation(const std::string& panoid) {
        logger->log("Detecting generation for " + panoid);

        // Generation test patterns - specific tile coordinates and zoom levels to test
        struct TestPattern {
            int gen;
            int zoom;
            std::vector<std::pair<int, int>> tests;
        };

        std::vector<TestPattern> tests = {
            {4, 4, {{15, 7}, {14, 6}}},  // Gen 4 (zoom 4, 16x8)
            {3, 4, {{12, 6}, {11, 5}}},  // Gen 3 (zoom 4, 13x7)
            {2, 4, {{12, 5}, {10, 4}}},  // Gen 2 (zoom 4, 13x6)
            {1, 3, {{7, 3}, {6, 2}}}     // Gen 1 (zoom 3, 8x4)
        };

        CURL* curl = init_curl();
        if (!curl) {
            logger->log("Failed to initialize CURL for generation detection");
            return { 0, "Unknown Generation" };
        }

        for (const auto& test : tests) {
            int gen = test.gen;
            int zoom = test.zoom;

            for (const auto& coords : test.tests) {
                int x = coords.first;
                int y = coords.second;

                std::string url = "https://streetviewpixels-pa.googleapis.com/v1/tile?cb_client=apiv3&panoid=" +
                    panoid + "&output=tile&zoom=" + std::to_string(zoom) +
                    "&x=" + std::to_string(x) + "&y=" + std::to_string(y);

                std::string response_data;
                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

                CURLcode res = curl_easy_perform(curl);

                if (res == CURLE_OK) {
                    long response_code;
                    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

                    if (response_code == 200 && !response_data.empty()) {
                        // Try to convert to an image
                        std::vector<uchar> buffer(response_data.begin(), response_data.end());
                        cv::Mat img = cv::imdecode(buffer, cv::IMREAD_COLOR);

                        if (!img.empty() && is_valid_tile(img)) {
                            std::string description;
                            switch (gen) {
                            case 4: description = "Generation 4 (Zoom 4, 16x8)"; break;
                            case 3: description = "Generation 3 (Zoom 4, 13x7)"; break;
                            case 2: description = "Generation 2 (Zoom 4, 13x6)"; break;
                            case 1: description = "Generation 1 (Zoom 3, 8x4)"; break;
                            }

                            curl_easy_cleanup(curl);
                            return { gen, description };
                        }
                    }
                }
            }
        }

        // Fallback tests for central tiles
        try {
            // Try zoom 4 first (most common)
            std::string url = "https://streetviewpixels-pa.googleapis.com/v1/tile?cb_client=apiv3&panoid=" +
                panoid + "&output=tile&zoom=4&x=8&y=4";

            std::string response_data;
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

            CURLcode res = curl_easy_perform(curl);

            if (res == CURLE_OK) {
                long response_code;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

                if (response_code == 200 && !response_data.empty()) {
                    std::vector<uchar> buffer(response_data.begin(), response_data.end());
                    cv::Mat img = cv::imdecode(buffer, cv::IMREAD_COLOR);

                    if (!img.empty() && is_valid_tile(img)) {
                        curl_easy_cleanup(curl);
                        return { 4, "Generation 4 (Zoom 4, 16x8) - Default" };
                    }
                }
            }

            // Try zoom 3 as a last resort
            url = "https://streetviewpixels-pa.googleapis.com/v1/tile?cb_client=apiv3&panoid=" +
                panoid + "&output=tile&zoom=3&x=4&y=2";

            response_data.clear();
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

            res = curl_easy_perform(curl);

            if (res == CURLE_OK) {
                long response_code;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

                if (response_code == 200 && !response_data.empty()) {
                    std::vector<uchar> buffer(response_data.begin(), response_data.end());
                    cv::Mat img = cv::imdecode(buffer, cv::IMREAD_COLOR);

                    if (!img.empty() && is_valid_tile(img)) {
                        curl_easy_cleanup(curl);
                        return { 1, "Generation 1 (Zoom 3, 8x4) - Default" };
                    }
                }
            }
        }
        catch (const std::exception& e) {
            logger->log("Exception in fallback detection: " + std::string(e.what()));
        }

        curl_easy_cleanup(curl);
        return { 0, "Unknown Generation" };
    }

    // Get configuration for a specific generation
    GenerationConfig get_generation_config(int generation) {
        static const std::unordered_map<int, GenerationConfig> configs = {
            {1, {3, 8, 4, true}},
            {2, {4, 13, 6, true}},
            {3, {4, 13, 7, true}},
            {4, {4, 16, 8, false}}
        };

        auto it = configs.find(generation);
        if (it != configs.end()) {
            return it->second;
        }

        // Default to Generation 4 if unknown
        return configs.at(4);
    }

    // Download a single tile with retry logic
    Tile download_tile(int x, int y, const std::string& panoid, int zoom) {
        Tile tile(x, y);
        std::string url = "https://streetviewpixels-pa.googleapis.com/v1/tile?cb_client=apiv3&panoid=" +
            panoid + "&output=tile&zoom=" + std::to_string(zoom) +
            "&x=" + std::to_string(x) + "&y=" + std::to_string(y);

        CURL* curl = init_curl();
        if (!curl) {
            logger->log("Failed to initialize CURL for tile download");
            return tile;
        }

        // Try multiple times with exponential backoff
        for (int attempt = 0; attempt < retry_count; ++attempt) {
            if (attempt > 0) {
                // Exponential backoff with jitter
                std::uniform_real_distribution<double> dist(0.0, 1.0);
                double backoff_time = std::min(std::pow(2.0, attempt) + dist(random_engine), 10.0);
                std::this_thread::sleep_for(std::chrono::duration<double>(backoff_time));
            }

            std::string response_data;
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

            CURLcode res = curl_easy_perform(curl);

            if (res == CURLE_OK) {
                long response_code;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

                if (response_code == 200 && !response_data.empty()) {
                    // Convert to OpenCV image
                    std::vector<uchar> buffer(response_data.begin(), response_data.end());
                    cv::Mat img = cv::imdecode(buffer, cv::IMREAD_COLOR);

                    if (!img.empty() && is_valid_tile(img)) {
                        tile.image = img;
                        tile.valid = true;
                        curl_easy_cleanup(curl);
                        return tile;
                    }
                }
            }

            // Only log error on final attempt
            if (attempt == retry_count - 1) {
                logger->log("Failed to download tile at (" + std::to_string(x) + ", " +
                    std::to_string(y) + ") for " + panoid);
            }
        }

        curl_easy_cleanup(curl);
        return tile;
    }

    // Download all tiles in parallel
    std::map<std::pair<int, int>, cv::Mat> download_tiles_parallel(
        const std::string& panoid, int zoom, int max_x, int max_y) {
        std::map<std::pair<int, int>, cv::Mat> result;
        std::mutex result_mutex;
        std::atomic<int> completed(0);
        int total_tiles = max_x * max_y;

        // Create vector of futures for parallel downloads
        std::vector<std::future<Tile>> futures;
        futures.reserve(total_tiles);

        // Determine optimal number of threads
        int effective_thread_count = std::min(tile_thread_count, total_tiles);
        logger->log("Using " + std::to_string(effective_thread_count) + " threads for tile downloads");

        // Submit download tasks
        for (int x = 0; x < max_x; ++x) {
            for (int y = 0; y < max_y; ++y) {
                futures.push_back(
                    thread_pool->enqueue(
                        [this, x, y, panoid, zoom]() {
                            active_threads++;
                            Tile tile = download_tile(x, y, panoid, zoom);
                            active_threads--;
                            return tile;
                        }
                    )
                );
            }
        }

        // Process results as they complete
        for (auto& future : futures) {
            Tile tile = future.get();

            if (tile.valid) {
                std::lock_guard<std::mutex> lock(result_mutex);
                result[{tile.x, tile.y}] = tile.image;
            }

            // Update progress atomically
            completed++;
            if (completed % 10 == 0 || completed == total_tiles) {
                logger->log("Downloaded " + std::to_string(completed) + "/" +
                    std::to_string(total_tiles) + " tiles for " + panoid);
            }
        }

        // Check if any tiles were successfully downloaded
        int valid_tiles = result.size();
        logger->log("Successfully downloaded " + std::to_string(valid_tiles) + " tiles for " + panoid);

        return result;
    }

    // Stitch tiles together into a panorama
    cv::Mat stitch_panorama(
        const std::map<std::pair<int, int>, cv::Mat>& tiles,
        int max_x, int max_y, int zoom_level) {
        // Check if we have any valid tiles
        if (tiles.empty()) {
            return cv::Mat();
        }

        // Find first valid tile to get dimensions
        const cv::Mat& sample_tile = tiles.begin()->second;
        int tile_width = sample_tile.cols;
        int tile_height = sample_tile.rows;

        // Create empty panorama
        cv::Mat panorama(max_y * tile_height, max_x * tile_width, CV_8UC3, cv::Scalar(255, 0, 255));

#ifdef USE_TBB
        // Use mutex for thread-safe OpenCV operations
        std::mutex stitch_mutex;

        // Paste tiles into panorama in parallel
        tbb::parallel_for_each(tiles.begin(), tiles.end(), [&](const auto& tile_pair) {
            const auto& coords = tile_pair.first;
            const cv::Mat& tile = tile_pair.second;

            int x = coords.first;
            int y = coords.second;
            int pos_x = x * tile_width;
            int pos_y = y * tile_height;

            // Create ROI in the panorama
            cv::Rect roi(pos_x, pos_y, tile_width, tile_height);

            // Check if ROI is within panorama bounds
            if (roi.x >= 0 && roi.y >= 0 &&
                roi.x + roi.width <= panorama.cols &&
                roi.y + roi.height <= panorama.rows) {

                // Create a copy of the tile to work with (avoids concurrent access issues)
                cv::Mat tile_copy = tile.clone();

                // For operations that modify the panorama, use a mutex to ensure thread safety
                std::lock_guard<std::mutex> lock(stitch_mutex);

                // Copy tile to panorama ROI
                cv::Mat destination = panorama(roi);
                tile_copy.copyTo(destination);

                // Draw labels if enabled
                if (draw_tile_labels) {
                    // Draw border
                    cv::rectangle(panorama, roi, cv::Scalar(0, 0, 255), 2);

                    // Create label text
                    std::string label = "x:" + std::to_string(x) + ", y:" + std::to_string(y) +
                        "\nz:" + std::to_string(zoom_level);

                    // Draw semi-transparent background
                    cv::Rect text_bg(pos_x + 5, pos_y + 5, 120, 45);
                    cv::Mat overlay = panorama(text_bg).clone();
                    cv::rectangle(panorama, text_bg, cv::Scalar(0, 0, 0), -1);
                    cv::addWeighted(overlay, 0.5, panorama(text_bg), 0.5, 0, panorama(text_bg));

                    // Draw text with outline for better visibility
                    int font_face = cv::FONT_HERSHEY_SIMPLEX;
                    double font_scale = 0.5;
                    int thickness = 1;
                    int baseline = 0;

                    // Split the text by newline
                    std::istringstream iss(label);
                    std::string line;
                    int line_height = 0;
                    int y_pos = pos_y + 20;

                    while (std::getline(iss, line)) {
                        cv::Size text_size = cv::getTextSize(line, font_face, font_scale, thickness, &baseline);
                        line_height = text_size.height + 5;

                        // Draw text outline
                        for (int dx = -1; dx <= 1; dx += 2) {
                            for (int dy = -1; dy <= 1; dy += 2) {
                                cv::putText(panorama, line, cv::Point(pos_x + 10 + dx, y_pos + dy),
                                    font_face, font_scale, cv::Scalar(0, 0, 0), thickness);
                            }
                        }

                        // Draw main text
                        cv::putText(panorama, line, cv::Point(pos_x + 10, y_pos),
                            font_face, font_scale, cv::Scalar(0, 255, 255), thickness);

                        y_pos += line_height;
                    }
                }
            }
            });
#else
        // Standard sequential processing
        // Paste tiles into panorama
        for (const auto& tile_pair : tiles) {
            const auto& coords = tile_pair.first;
            const cv::Mat& tile = tile_pair.second;

            int x = coords.first;
            int y = coords.second;
            int pos_x = x * tile_width;
            int pos_y = y * tile_height;

            // Create ROI in the panorama
            cv::Rect roi(pos_x, pos_y, tile_width, tile_height);

            // Check if ROI is within panorama bounds
            if (roi.x >= 0 && roi.y >= 0 &&
                roi.x + roi.width <= panorama.cols &&
                roi.y + roi.height <= panorama.rows) {

                // Copy tile to panorama ROI
                cv::Mat destination = panorama(roi);
                tile.copyTo(destination);

                // Draw labels if enabled
                if (draw_tile_labels) {
                    // Draw border
                    cv::rectangle(panorama, roi, cv::Scalar(0, 0, 255), 2);

                    // Create label text
                    std::string label = "x:" + std::to_string(x) + ", y:" + std::to_string(y) +
                        "\nz:" + std::to_string(zoom_level);

                    // Draw semi-transparent background
                    cv::Rect text_bg(pos_x + 5, pos_y + 5, 120, 45);
                    cv::Mat overlay = panorama(text_bg).clone();
                    cv::rectangle(panorama, text_bg, cv::Scalar(0, 0, 0), -1);
                    cv::addWeighted(overlay, 0.5, panorama(text_bg), 0.5, 0, panorama(text_bg));

                    // Draw text with outline for better visibility
                    int font_face = cv::FONT_HERSHEY_SIMPLEX;
                    double font_scale = 0.5;
                    int thickness = 1;
                    int baseline = 0;

                    // Split the text by newline
                    std::istringstream iss(label);
                    std::string line;
                    int line_height = 0;
                    int y_pos = pos_y + 20;

                    while (std::getline(iss, line)) {
                        cv::Size text_size = cv::getTextSize(line, font_face, font_scale, thickness, &baseline);
                        line_height = text_size.height + 5;

                        // Draw text outline
                        for (int dx = -1; dx <= 1; dx += 2) {
                            for (int dy = -1; dy <= 1; dy += 2) {
                                cv::putText(panorama, line, cv::Point(pos_x + 10 + dx, y_pos + dy),
                                    font_face, font_scale, cv::Scalar(0, 0, 0), thickness);
                            }
                        }

                        // Draw main text
                        cv::putText(panorama, line, cv::Point(pos_x + 10, y_pos),
                            font_face, font_scale, cv::Scalar(0, 255, 255), thickness);

                        y_pos += line_height;
                    }
                }
            }
        }
#endif

        return panorama;
    }

    // Crop panorama based on generation
    cv::Mat crop_panorama(const cv::Mat& panorama, int generation) {
        int width = panorama.cols;
        int height = panorama.rows;

        if (generation == 1) {
            // For Generation 1, crop to exactly 3328 x 1664 from top left
            int crop_width = std::min(width, 3328);
            int crop_height = std::min(height, 1664);
            return panorama(cv::Rect(0, 0, crop_width, crop_height));
        }
        else {
            // For other generations, maintain 2:1 aspect ratio (crop from bottom)
            int target_height = width / 2;

            // Only crop if the image is taller than 2:1 ratio
            if (height > target_height) {
                return panorama(cv::Rect(0, 0, width, target_height));
            }

            return panorama;
        }
    }

    // Modified equirectangular to rectilinear projection with 90° horizontal FOV
    cv::Mat equirect_to_rectilinear(
        const cv::Mat& panorama, double direction_rad, double vfov_rad, int output_size,
        double pitch_rad = 0.0, double yaw_rad = 0.0) {

        int pano_width = panorama.cols;
        int pano_height = panorama.rows;

        // The horizontal FOV should be 90 degrees (in radians)
        double hfov_rad = 90.0 * M_PI / 180.0;

        // Create output image
        cv::Mat output(output_size, output_size, CV_8UC3, cv::Scalar(0, 0, 0));

        // Pre-compute values for transformation
        double tan_hfov_half = tan(hfov_rad / 2);
        double tan_vfov_half = tan(vfov_rad / 2);

        // Create mapping matrices for faster remapping
        cv::Mat map_x(output_size, output_size, CV_32F);
        cv::Mat map_y(output_size, output_size, CV_32F);

        // Use parallel processing for better performance
#ifdef USE_TBB
    // Use TBB for parallel processing
        tbb::parallel_for(tbb::blocked_range2d<int>(0, output_size, 0, output_size),
            [&](const tbb::blocked_range2d<int>& range) {
                for (int y = range.rows().begin(); y < range.rows().end(); ++y) {
                    for (int x = range.cols().begin(); x < range.cols().end(); ++x) {
                        // Normalized device coordinates with separate FOVs
                        double nx = (2.0 * x / output_size - 1.0) * tan_hfov_half;
                        double ny = -(2.0 * y / output_size - 1.0) * tan_vfov_half;

                        // Apply pitch and yaw adjustments
                        double nz = 1.0;  // Looking forward

                        // Apply pitch rotation (around x-axis)
                        double py = ny * cos(pitch_rad) - nz * sin(pitch_rad);
                        double pz = ny * sin(pitch_rad) + nz * cos(pitch_rad);

                        // Apply yaw rotation (around y-axis)
                        double yx = nx * cos(yaw_rad) + pz * sin(yaw_rad);
                        double yz = -nx * sin(yaw_rad) + pz * cos(yaw_rad);

                        // Convert to spherical coordinates
                        double r = sqrt(yx * yx + py * py + yz * yz);
                        double phi = asin(py / r);
                        double theta = atan2(yx, yz) + direction_rad;

                        // Convert to panorama coordinates
                        double u = fmod(theta / (2.0 * M_PI) + 1.0, 1.0) * pano_width;
                        double v = (0.5 - phi / M_PI) * pano_height;

                        // Store coordinates for remap
                        map_x.at<float>(y, x) = static_cast<float>(u);
                        map_y.at<float>(y, x) = static_cast<float>(v);
                    }
                }
            });
#else
    // Use OpenMP for parallel processing
        OMP_PARALLEL_FOR
            for (int y = 0; y < output_size; ++y) {
                for (int x = 0; x < output_size; ++x) {
                    // Normalized device coordinates with separate FOVs
                    double nx = (2.0 * x / output_size - 1.0) * tan_hfov_half;
                    double ny = -(2.0 * y / output_size - 1.0) * tan_vfov_half;

                    // Apply pitch and yaw adjustments
                    double nz = 1.0;  // Looking forward

                    // Apply pitch rotation (around x-axis)
                    double py = ny * cos(pitch_rad) - nz * sin(pitch_rad);
                    double pz = ny * sin(pitch_rad) + nz * cos(pitch_rad);

                    // Apply yaw rotation (around y-axis)
                    double yx = nx * cos(yaw_rad) + pz * sin(yaw_rad);
                    double yz = -nx * sin(yaw_rad) + pz * cos(yaw_rad);

                    // Convert to spherical coordinates
                    double r = sqrt(yx * yx + py * py + yz * yz);
                    double phi = asin(py / r);
                    double theta = atan2(yx, yz) + direction_rad;

                    // Convert to panorama coordinates
                    double u = fmod(theta / (2.0 * M_PI) + 1.0, 1.0) * pano_width;
                    double v = (0.5 - phi / M_PI) * pano_height;

                    // Store coordinates for remap
                    map_x.at<float>(y, x) = static_cast<float>(u);
                    map_y.at<float>(y, x) = static_cast<float>(v);
                }
            }
#endif

        // Use OpenCV's efficient remap function with bilinear interpolation
        cv::remap(panorama, output, map_x, map_y, cv::INTER_LINEAR, cv::BORDER_WRAP);

        return output;
    }

    void create_directional_views_with_jitter(
        const cv::Mat& panorama, const std::string& panoid,
        const fs::path& output_dir, int generation, int zoom) {
        logger->log("Creating 8 directional views with 90° FOV for complete coverage");

        int output_size = 512;
        double vfov_deg = 90.0;  // Vertical field of view
        int num_views = 8;
        double fov_deg = 90.0;  // Horizontal field of view for each view (90°)

        // Each view is separated by 45° (360° / 8 = 45°)
        // With 90° FOV, we get 45° of overlap between adjacent views

        // Set up random distributions for jitter
        std::uniform_real_distribution<double> global_rotation_dist(-22.5, 22.5);
        std::uniform_real_distribution<double> fov_jitter_dist(-5.0, 5.0);

        // Add small amount of pitch for more natural looking views
        double pitch_rad = 5.0 * M_PI / 180.0;
        double yaw_rad = 5.0 * M_PI / 180.0;

        // Generate a global rotation to apply to all directions
        double global_rotation = global_rotation_dist(random_engine);
        logger->log("Global rotation for all directions: " + std::to_string(global_rotation) + "°");

        // Define exact view directions for the 8 standard orientations
        const std::vector<std::pair<double, std::string>> directions = {
            {0.0, "N"},
            {45.0, "NE"},
            {90.0, "E"},
            {135.0, "SE"},
            {180.0, "S"},
            {225.0, "SW"},
            {270.0, "W"},
            {315.0, "NW"}
        };

        // Create the 8 directional views
        for (int i = 0; i < num_views; ++i) {
            // Get the base direction and name
            double base_direction_deg = directions[i].first;
            std::string direction_name = directions[i].second;

            // Apply global rotation and ensure it's within [0, 360)
            double final_direction_deg = fmod(base_direction_deg + global_rotation + 360.0, 360.0);

            // Add small random jitter to FOV
            double vfov_jitter = fov_jitter_dist(random_engine);
            double final_vfov_deg = vfov_deg + vfov_jitter;

            // Ensure FOV stays in reasonable range
            final_vfov_deg = std::max(75.0, std::min(110.0, final_vfov_deg));

            // Convert degrees to radians
            double direction_rad = final_direction_deg * M_PI / 180.0;
            double vfov_rad = final_vfov_deg * M_PI / 180.0;

            // Log info
            logger->log("View " + std::to_string(i + 1) + ": " + direction_name +
                " at " + std::to_string(final_direction_deg) + "° with FOV " +
                std::to_string(fov_deg) + "° horizontal, " +
                std::to_string(final_vfov_deg) + "° vertical");

            // Generate the rectilinear view with pitch and yaw adjustments
            cv::Mat output = equirect_to_rectilinear(panorama, direction_rad, vfov_rad, output_size, pitch_rad, yaw_rad);

            // Create output filename
            std::string gen_suffix = include_gen_in_filename ? "_gen" + std::to_string(generation) : "";

            std::ostringstream filename_stream;
            filename_stream << panoid << "_View" << i + 1 << "_" << direction_name << "_FOV" << std::fixed << std::setprecision(1) << fov_deg << ".jpg";

            fs::path output_path = output_dir / filename_stream.str();

            // Save the image
            cv::imwrite(output_path.string(), output);
            logger->log("Saved directional view: " + output_path.string());
        }
    }

    // Process a single panorama
    bool process_panorama(const std::string& panoid, const fs::path& output_dir) {
        try {
            logger->log("Processing panorama " + panoid);
            logger->log("Detecting generation for " + panoid);

            // Check generation cache first
            auto cached_gen = get_cached_generation(panoid);
            int generation = 0;
            std::string description;

            if (cached_gen.first != 0) {
                generation = cached_gen.first;
                description = cached_gen.second;
                logger->log("Using cached generation: " + description);
            }
            else {
                // Detect the generation
                auto gen_result = detect_generation(panoid);
                generation = gen_result.first;
                description = gen_result.second;

                // Cache the result
                cache_generation(panoid, generation, description);
            }

            if (generation == 0) {
                logger->log("Could not detect generation for " + panoid);
                record_failed_pano(panoid);
                return false;
            }

            logger->log("Detected " + description);

            // Get the configuration for this generation
            GenerationConfig config = get_generation_config(generation);

            // Skip checking for existing files since we're only creating directional views

            // Download tiles
            logger->log("Downloading tiles for " + panoid);
            auto tiles = download_tiles_parallel(panoid, config.zoom, config.max_x, config.max_y);

            // Check if we have valid tiles
            if (tiles.empty()) {
                logger->log("Failed to download tiles for " + panoid);
                record_failed_pano(panoid);
                return false;
            }

            // Count valid tiles
            int valid_tiles = tiles.size();
            if (valid_tiles == 0) {
                logger->log("No valid tiles found for " + panoid);
                record_failed_pano(panoid);
                return false;
            }

            // Stitch panorama
            logger->log("Stitching panorama from " + std::to_string(valid_tiles) + " tiles");
            cv::Mat panorama = stitch_panorama(tiles, config.max_x, config.max_y, config.zoom);

            if (panorama.empty()) {
                logger->log("Failed to stitch panorama for " + panoid);
                record_failed_pano(panoid);
                return false;
            }

            // Crop if needed and auto-crop is enabled
            if (config.crop && auto_crop && !draw_tile_labels) {
                logger->log("Cropping panorama");
                panorama = crop_panorama(panorama, generation);
            }

            // Skip saving the full panorama
            // Instead, just proceed to creating directional views

            // Create directional views
            logger->log("Creating directional views with random jitter");
            create_directional_views_with_jitter(panorama, panoid, output_dir, generation, config.zoom);

            return true;
        }
        catch (const std::exception& e) {
            logger->log("Error processing " + panoid + ": " + e.what());
            record_failed_pano(panoid);
            return false;
        }
    }

    // Parse PANOIDs from a file - now enhanced to handle different CSV formats
    std::vector<std::string> parse_panoids_from_file(const std::string& file_path) {
        std::vector<std::string> panoids;

        try {
            // Check file extension
            fs::path path(file_path);
            std::string extension = path.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

            // If it's a CSV file, use the CSV handler
            if (extension == ".csv") {
                try {
                    csv_handler = std::make_shared<CSVHandler>(file_path);
                    panoids = csv_handler->get_panoids();
                    logger->log("Loaded " + std::to_string(panoids.size()) + " PanoIDs from CSV file");

                    if (clean_csv_output) {
                        logger->log("CSV cleanup enabled. Will generate cleaned CSV after processing.");
                    }

                    return panoids;
                }
                catch (const std::exception& e) {
                    logger->log("Error parsing CSV: " + std::string(e.what()) + ". Falling back to simple line parsing.");
                }
            }

            // Fallback to simple text file parsing
            std::ifstream file(file_path);
            if (!file.is_open()) {
                logger->log("Error: Could not open file " + file_path);
                return panoids;
            }

            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();

            // Try to parse as CSV first
            if (content.find(',') != std::string::npos || content.find(';') != std::string::npos) {
                std::istringstream iss(content);
                std::string line;

                while (std::getline(iss, line)) {
                    std::istringstream line_stream(line);
                    std::string panoid;

                    // Determine delimiter
                    char delimiter = ',';
                    if (line.find(';') != std::string::npos) {
                        delimiter = ';';
                    }

                    if (std::getline(line_stream, panoid, delimiter)) {
                        // Trim whitespace
                        panoid.erase(0, panoid.find_first_not_of(" \t\r\n"));
                        panoid.erase(panoid.find_last_not_of(" \t\r\n") + 1);

                        if (!panoid.empty()) {
                            panoids.push_back(panoid);
                        }
                    }
                }

                if (!panoids.empty()) {
                    return panoids;
                }
            }

            // Parse line by line as fallback
            std::istringstream iss(content);
            std::string line;

            while (std::getline(iss, line)) {
                // Trim whitespace
                line.erase(0, line.find_first_not_of(" \t\r\n"));
                line.erase(line.find_last_not_of(" \t\r\n") + 1);

                if (!line.empty()) {
                    panoids.push_back(line);
                }
            }
        }
        catch (const std::exception& e) {
            logger->log("Error reading file: " + std::string(e.what()));
        }

        return panoids;
    }

    // Generate a clean CSV file with failed panoramas removed
    void generate_cleaned_csv() {
        if (!csv_handler || failed_panoids.empty() || csv_output_path.empty()) {
            return;
        }

        try {
            logger->log("Generating cleaned CSV file with " + std::to_string(failed_panoids.size()) +
                " failed panoramas removed...");

            // If no output path is specified, create one based on the input path
            std::string output_file = csv_output_path;
            if (output_file.empty()) {
                fs::path input_path(csv_handler->get_file_path());
                fs::path dir = input_path.parent_path();
                std::string stem = input_path.stem().string();
                output_file = (dir / (stem + "_cleaned.csv")).string();
            }

            // Write the cleaned CSV
            if (csv_handler->write_cleaned_csv(failed_panoids, output_file)) {
                logger->log("Successfully wrote cleaned CSV to: " + output_file);
            }
            else {
                logger->log("Failed to write cleaned CSV file!");
            }
        }
        catch (const std::exception& e) {
            logger->log("Error generating cleaned CSV: " + std::string(e.what()));
        }
    }

public:
    // Constructor with default settings
    StreetViewDownloader() :
        retry_count(3),
        timeout_value(10),
        tile_thread_count(128),
        pano_thread_count(4),
        max_total_threads(512),
        include_gen_in_filename(true),
        auto_crop(true),
        skip_existing(true),
        draw_tile_labels(false),
        create_directional_views(true),
        clean_csv_output(false),
        download_progress(0),
        active_threads(0),
        random_engine(std::random_device{}())
    {
        // Initialize logger
        logger = std::make_shared<Logger>("streetview_downloader.log", true);

        // Initialize thread pool
        thread_pool = std::make_shared<ThreadPool>(std::min(max_total_threads,
            static_cast<int>(std::thread::hardware_concurrency())));

        // Initialize CURL globally
        curl_global_init(CURL_GLOBAL_ALL);

        // Set up HTTP headers
        headers = curl_slist_append(nullptr, "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36");
        headers = curl_slist_append(headers, "Referer: https://www.google.com/maps/");
        headers = curl_slist_append(headers, "Accept: image/webp,image/apng,image/*,*/*;q=0.8");
    }

    // Destructor to clean up resources
    ~StreetViewDownloader() {
        // Clean up CURL resources
        if (headers) {
            curl_slist_free_all(headers);
        }

        curl_global_cleanup();
    }

    // Setters for configuration
    void set_retry_count(int count) { retry_count = count; }
    void set_timeout_value(int timeout) { timeout_value = timeout; }
    void set_tile_thread_count(int count) { tile_thread_count = count; }
    void set_pano_thread_count(int count) {
        pano_thread_count = count;
        // Re-initialize thread pool with new configuration
        thread_pool = std::make_shared<ThreadPool>(std::min(max_total_threads,
            static_cast<int>(std::thread::hardware_concurrency())));
    }
    void set_max_total_threads(int count) { max_total_threads = count; }
    void set_include_gen_in_filename(bool value) { include_gen_in_filename = value; }
    void set_auto_crop(bool value) { auto_crop = value; }
    void set_skip_existing(bool value) { skip_existing = value; }
    void set_draw_tile_labels(bool value) { draw_tile_labels = value; }
    void set_create_directional_views(bool value) { create_directional_views = value; }
    void set_clean_csv_output(bool value, const std::string& output_path = "") {
        clean_csv_output = value;
        csv_output_path = output_path;
    }

    // Process multiple panoramas with multi-level parallelism
    std::pair<int, int> process_panoids(const std::vector<std::string>& panoids, const fs::path& output_dir) {
        int total = panoids.size();
        std::atomic<int> successful(0);
        std::atomic<int> failed(0);
        std::atomic<int> completed(0);

        logger->log("Processing " + std::to_string(total) + " panoramas with " +
            std::to_string(pano_thread_count) + " concurrent panoramas");

        // Initialize progress bar
        progress_bar = std::make_shared<ProgressBar>(total);

        // Create a vector to store futures for each panorama processing task
        std::vector<std::future<bool>> futures;
        futures.reserve(total);

        // Process panoramas in batches to control memory usage
        const int batch_size = pano_thread_count * 2;

        for (int start = 0; start < total; start += batch_size) {
            int end = std::min(start + batch_size, total);
            futures.clear();

            // Start processing a batch of panoramas
            for (int i = start; i < end; ++i) {
                const std::string& panoid = panoids[i];
                futures.push_back(
                    thread_pool->enqueue(
                        [this, panoid, &output_dir]() {
                            return process_panorama(panoid, output_dir);
                        }
                    )
                );
            }

            // Wait for the batch to complete
            for (auto& future : futures) {
                bool success = future.get();
                if (success) {
                    successful++;
                }
                else {
                    failed++;
                }

                // Update completed count and progress bar
                completed++;
                progress_bar->update(completed, successful, failed);

                // Update progress periodically
                if (completed % 5 == 0 || completed == total) {
                    logger->log("Progress: " + std::to_string(completed) + "/" + std::to_string(total) +
                        " complete (" + std::to_string(successful) + " successful, " +
                        std::to_string(failed) + " failed)");
                }
            }
        }

        // Hide progress bar before final message
        progress_bar->hide();

        // Generate cleaned CSV if requested
        if (clean_csv_output && csv_handler) {
            generate_cleaned_csv();
        }

        // Print failed panorama IDs if any
        if (failed > 0) {
            print_failed_panoids();
        }

        // Processing complete
        std::string completion_message = "Completed: " + std::to_string(successful) +
            " successful, " + std::to_string(failed) + " failed";
        logger->log(completion_message);

        return { successful, failed };
    }

    // Main processing function
    int run(int argc, char* argv[]) {
        // Parse command-line arguments
        std::string panoid;
        std::string file_path;
        fs::path output_dir = fs::path(getenv("HOME") ? getenv("HOME") : ".") / "streetview_output";
        bool has_input = false;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-f" || arg == "--file") {
                if (i + 1 < argc) {
                    file_path = argv[++i];
                    has_input = true;
                }
            }
            else if (arg == "-o" || arg == "--output") {
                if (i + 1 < argc) {
                    output_dir = argv[++i];
                }
            }
            else if (arg == "-t" || arg == "--tile-threads") {
                if (i + 1 < argc) {
                    tile_thread_count = std::stoi(argv[++i]);
                }
            }
            else if (arg == "-p" || arg == "--pano-threads") {
                if (i + 1 < argc) {
                    pano_thread_count = std::stoi(argv[++i]);
                }
            }
            else if (arg == "--max-threads") {
                if (i + 1 < argc) {
                    max_total_threads = std::stoi(argv[++i]);
                }
            }
            else if (arg == "--timeout") {
                if (i + 1 < argc) {
                    timeout_value = std::stoi(argv[++i]);
                }
            }
            else if (arg == "--retries") {
                if (i + 1 < argc) {
                    retry_count = std::stoi(argv[++i]);
                }
            }
            else if (arg == "--no-gen-suffix") {
                include_gen_in_filename = false;
            }
            else if (arg == "--no-crop") {
                auto_crop = false;
            }
            else if (arg == "--no-skip") {
                skip_existing = false;
            }
            else if (arg == "--labels") {
                draw_tile_labels = true;
            }
            else if (arg == "--no-directional") {
                create_directional_views = false;
            }
            else if (arg == "--clean-csv") {
                clean_csv_output = true;
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    csv_output_path = argv[++i];
                }
            }
            else if (arg == "-h" || arg == "--help") {
                print_usage(argv[0]);
                return 0;
            }
            else if (!has_input && arg[0] != '-') {
                // Assume it's a PANOID if it doesn't start with - and no input method specified yet
                panoid = arg;
                has_input = true;
            }
        }

        // Re-initialize thread pool with configured values
        thread_pool = std::make_shared<ThreadPool>(std::min(max_total_threads,
            std::max(tile_thread_count, pano_thread_count)));

        // Create output directory
        try {
            fs::create_directories(output_dir);
            logger->log("Output directory: " + output_dir.string());
        }
        catch (const std::exception& e) {
            logger->log("Error creating output directory: " + std::string(e.what()));
            return 1;
        }

        // Process PANOIDs
        std::vector<std::string> panoids;

        if (!panoid.empty()) {
            // Single PANOID mode
            panoids.push_back(panoid);
            logger->log("Processing single PANOID: " + panoid);
        }
        else if (!file_path.empty()) {
            // File mode
            logger->log("Reading PANOIDs from file: " + file_path);
            panoids = parse_panoids_from_file(file_path);

            if (panoids.empty()) {
                logger->log("Error: No valid PANOIDs found in file.");
                return 1;
            }

            logger->log("Found " + std::to_string(panoids.size()) + " PANOIDs to process");
        }
        else {
            logger->log("Error: No PANOID or file specified.");
            print_usage(argv[0]);
            return 1;
        }

        // Process all PANOIDs
        auto start_time = std::chrono::high_resolution_clock::now();
        auto [successful, failed] = process_panoids(panoids, output_dir);
        auto end_time = std::chrono::high_resolution_clock::now();

        // Print summary
        double duration = std::chrono::duration<double>(end_time - start_time).count();
        logger->log("Processing complete in " + std::to_string(duration) + " seconds");
        logger->log("Successful: " + std::to_string(successful) + "/" + std::to_string(panoids.size()));
        logger->log("Failed: " + std::to_string(failed) + "/" + std::to_string(panoids.size()));

        // Add this line to print failed panorama IDs
        if (failed > 0) {
            print_failed_panoids();
        }

        // Return exit code based on success
        return (failed == 0) ? 0 : 1;
    }

    void print_usage(const char* program_name) {
        // ASCII art banner
        std::cout << "\n"
            << "░█▀▀░▀█▀░█▀▄░█▀▀░█▀▀░▀█▀░█░█░▀█▀░█▀▀░█░█\n"
            << "░▀▀█░░█░░█▀▄░█▀▀░█▀▀░░█░░▀▄▀░░█░░█▀▀░█▄█\n"
            << "░▀▀▀░░▀░░▀░▀░▀▀▀░▀▀▀░░▀░░░▀░░▀▀▀░▀▀▀░▀░▀\n"
            << "░█▀▄░█▀█░█░█░█▀█░█░░░█▀█░█▀█░█▀▄░█▀▀░█▀▄\n"
            << "░█░█░█░█░█▄█░█░█░█░░░█░█░█▀█░█░█░█▀▀░█▀▄\n"
            << "░▀▀░░▀▀▀░▀░▀░▀░▀░▀▀▀░▀▀▀░▀░▀░▀▀░░▀▀▀░▀░▀\n\n";

        std::cout << "Street View Panorama Downloader - C++ Version with Multi-level Parallelism" << std::endl;
        std::cout << "Usage: " << program_name << " [PANOID] [options]" << std::endl;
        std::cout << "   or: " << program_name << " -f FILE [options]" << std::endl;
        std::cout << std::endl;
        std::cout << "Input options:" << std::endl;
        std::cout << "  PANOID                Single PANOID to download" << std::endl;
        std::cout << "  -f, --file FILE       File containing PANOIDs (one per line or CSV)" << std::endl;
        std::cout << std::endl;
        std::cout << "Output options:" << std::endl;
        std::cout << "  -o, --output DIR      Output directory for saved panoramas" << std::endl;
        std::cout << "  --clean-csv [FILE]    Create cleaned CSV file with failed panoramas removed" << std::endl;
        std::cout << "                        Optional: specify output file path" << std::endl;
        std::cout << std::endl;
        std::cout << "Performance options:" << std::endl;
        std::cout << "  -t, --tile-threads N  Number of download threads per panorama (default: 128)" << std::endl;
        std::cout << "  -p, --pano-threads N  Number of panoramas to process concurrently (default: 4)" << std::endl;
        std::cout << "  --max-threads N       Maximum total number of threads (default: 512)" << std::endl;
        std::cout << "  --timeout N           Download timeout in seconds (default: 10)" << std::endl;
        std::cout << "  --retries N           Number of download retries (default: 3)" << std::endl;
        std::cout << std::endl;
        std::cout << "Other options:" << std::endl;
        std::cout << "  --no-gen-suffix       Do not include generation in filename" << std::endl;
        std::cout << "  --no-crop             Do not auto-crop panoramas" << std::endl;
        std::cout << "  --no-skip             Do not skip existing files" << std::endl;
        std::cout << "  --labels              Draw tile labels (x,y,zoom)" << std::endl;
        std::cout << "  --no-directional      Do not create directional views" << std::endl;
        std::cout << "  -h, --help            Show this help message" << std::endl;
    }

    void print_failed_panoids() {
        std::lock_guard<std::mutex> lock(failed_panoids_mutex);

        if (failed_panoids.empty()) {
            logger->log("No failed panoramas to report.");
            return;
        }

        logger->log("===== FAILED PANORAMAS =====");
        logger->log("The following " + std::to_string(failed_panoids.size()) + " panoramas failed to download:");

        // Output each failed panoid with index
        int i = 1;
        for (const auto& panoid : failed_panoids) {
            logger->log(std::to_string(i++) + ". " + panoid);
        }

        logger->log("============================");

        // If clean CSV option is enabled, mention it
        if (clean_csv_output) {
            logger->log("These failed panoramas will be excluded from the cleaned CSV output.");
        }
    }
};

// Main entry point
int main(int argc, char* argv[]) {
    try {
        // Display ASCII art banner if no arguments provided
        if (argc == 1) {
            // ASCII art banner
            std::cout << "\n"
                << "░█▀▀░▀█▀░█▀▄░█▀▀░█▀▀░▀█▀░█░█░▀█▀░█▀▀░█░█\n"
                << "░▀▀█░░█░░█▀▄░█▀▀░█▀▀░░█░░▀▄▀░░█░░█▀▀░█▄█\n"
                << "░▀▀▀░░▀░░▀░▀░▀▀▀░▀▀▀░░▀░░░▀░░▀▀▀░▀▀▀░▀░▀\n"
                << "░█▀▄░█▀█░█░█░█▀█░█░░░█▀█░█▀█░█▀▄░█▀▀░█▀▄\n"
                << "░█░█░█░█░█▄█░█░█░█░░░█░█░█▀█░█░█░█▀▀░█▀▄\n"
                << "░▀▀░░▀▀▀░▀░▀░▀░▀░▀▀▀░▀▀▀░▀░▀░▀▀░░▀▀▀░▀░▀\n\n";

            // Create a downloader instance just to display usage info
            StreetViewDownloader downloader;
            downloader.print_usage(argv[0]);
            return 0;
        }

        // Normal execution with arguments
        StreetViewDownloader downloader;
        return downloader.run(argc, argv);
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}