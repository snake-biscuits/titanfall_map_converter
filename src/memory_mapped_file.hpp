#pragma once

#include <string>
#include <string_view>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif _WIN32

using namespace std::literals::string_literals;

class memory_mapped_file
{
    char* data_{};
    LARGE_INTEGER size_{};
    HANDLE file_{};
    HANDLE mapping_{};
    bool exists_{false};

public:
    bool open_existing(const char* filename);
    bool open_new(const char* filename, size_t size);
    //bool open(const std::string& filename) { return open(filename.data()); };
    //bool open(const std::filesystem::path& filename) { return open(filename.str()); };
    //void prefetch();
    void fill(uint8_t filler);
    void set_size_and_close(size_t new_size);
    void close();
    ~memory_mapped_file() { close(); };
    inline std::string_view data() { return { data_, static_cast<size_t>(size_.QuadPart) }; }
    inline char* rawdata() { return data_; }
    template <typename T> inline T* rawdata() const { return reinterpret_cast<T*>(data_); }
    inline char* rawdata(size_t offset) { return data_ + offset; }
    template <typename T> inline T* rawdata(size_t offset) const { return reinterpret_cast<T*>(data_ + offset); }
    inline size_t size() { return static_cast<size_t>(size_.QuadPart); }
    inline bool exists() { return exists_; }
};

#ifdef _WIN32
bool memory_mapped_file::open_existing(const char* filename)
{
    file_ = CreateFileA(filename, FILE_READ_DATA, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);
    if (!file_ || file_ == INVALID_HANDLE_VALUE) [[unlikely]] {
        auto lastError = GetLastError();
        if (lastError == ERROR_FILE_NOT_FOUND || lastError == ERROR_PATH_NOT_FOUND) {
            exists_ = false;
            return false;
        }
        throw std::runtime_error("Failed opening file: "s + filename + " (" + std::to_string(lastError) + ")");
    }

    if (!GetFileSizeEx(file_, &size_)) [[unlikely]]
        throw std::runtime_error("Failed getting file size for: "s + filename);

    mapping_ = CreateFileMappingW(file_, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!mapping_ || mapping_ == INVALID_HANDLE_VALUE) [[unlikely]]
        throw std::runtime_error("Failed creating file mapping: "s + filename);

    data_ = reinterpret_cast<char*>(MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, 0));
    if (data_ == nullptr) [[unlikely]]
        throw std::runtime_error("Failed mapping view of file: "s + filename);

    exists_ = true;
    return true;
}

bool memory_mapped_file::open_new(const char* filename, size_t size) {
    file_ = CreateFileA(filename, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL,
        CREATE_ALWAYS, FILE_FLAG_NO_BUFFERING, NULL);
    if (!file_ || file_ == INVALID_HANDLE_VALUE) [[unlikely]] {
        auto lastError = GetLastError();
        if (lastError == ERROR_FILE_NOT_FOUND || lastError == ERROR_PATH_NOT_FOUND) {
            exists_ = false;
            return false;
        }
        if (!file_ || lastError != ERROR_ALREADY_EXISTS)
            throw std::runtime_error("Failed opening file: "s + filename + " (" + std::to_string(lastError) + ")");
    }

    size_.QuadPart = static_cast<long long>(size);
    mapping_ = CreateFileMappingW(file_, NULL, PAGE_READWRITE, size_.HighPart, size_.LowPart, NULL);
    if (!mapping_ || mapping_ == INVALID_HANDLE_VALUE) [[unlikely]]
        throw std::runtime_error("Failed creating file mapping: "s + filename);

    data_ = reinterpret_cast<char*>(MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS, 0, 0, 0));
    if (data_ == nullptr) [[unlikely]]
        throw std::runtime_error("Failed mapping view of file: "s + filename);

    exists_ = true;
    return true;
}

void memory_mapped_file::fill(uint8_t filler) {
    memset(data_, filler, size_.QuadPart);
}

void memory_mapped_file::set_size_and_close(size_t new_size) {
    if (!exists_ || !data_)
	    return;

    FlushViewOfFile(data_, new_size);
    FlushFileBuffers(file_);

    if (data_ != nullptr) {
        UnmapViewOfFile(data_);
        data_ = nullptr;
    }
    if (mapping_ && mapping_ != INVALID_HANDLE_VALUE) {
        CloseHandle(mapping_);
        mapping_ = nullptr;
    }

    LARGE_INTEGER new_size_li{ .QuadPart = static_cast<long long>(new_size) };
    auto ret = SetFilePointerEx(file_, new_size_li, nullptr, FILE_BEGIN);
    auto lastError = GetLastError();

    if (ret == INVALID_SET_FILE_POINTER && lastError != NO_ERROR)
        throw std::runtime_error("Failed changing file size (SetFilePointerEx failed), error: " + std::to_string(lastError));

    //if (!SetEndOfFile(file_))
    //    throw std::runtime_error("Failed changing file size (SetEndOfFile failed), error: " + std::to_string(GetLastError()));

    size_.QuadPart = new_size_li.QuadPart;

    close();
}

void memory_mapped_file::close() {
    if (data_ != nullptr) {
        UnmapViewOfFile(data_);
        data_ = nullptr;
    }
    if (mapping_ && mapping_ != INVALID_HANDLE_VALUE) {
        CloseHandle(mapping_);
        mapping_ = nullptr;
    }
    if (file_ && file_ != INVALID_HANDLE_VALUE) {
        CloseHandle(file_);
        file_ = nullptr;
    }
    exists_ = false;
    size_.QuadPart = 0;
}

#endif
