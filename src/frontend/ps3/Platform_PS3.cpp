// Implementation of the core's Platform:: interface for the PS3 frontend.
//
// Everything here is plain C/C++ (stdio, no OS-specific calls) so it builds
// unchanged for the PS3 and for the host headless runner. Threads, semaphores,
// mutexes and clocks live in Sync_std.cpp / Sync_single.cpp.

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

#include "FrontendContext.h"
#include "Platform.h"

namespace melonDS::PS3Frontend
{

static std::string DataDir;

void SetDataDir(const std::string& dir)
{
    DataDir = dir;
}

}

namespace melonDS::Platform
{

struct FileHandle
{
    FILE* File;
};

static PS3Frontend::Context* ContextFrom(void* userdata)
{
    return static_cast<PS3Frontend::Context*>(userdata);
}

void SignalStop(StopReason reason, void* userdata)
{
    if (PS3Frontend::Context* ctx = ContextFrom(userdata))
    {
        ctx->StopReason = (int)reason;
        ctx->StopRequested = true;
    }
}

// ---- files ----------------------------------------------------------------

std::string GetLocalFilePath(const std::string& filename)
{
    return PS3Frontend::DataDir + filename;
}

FileHandle* OpenFile(const std::string& path, FileMode mode)
{
    const unsigned m = (unsigned)mode;
    const bool read = (m & Read) != 0;
    const bool write = (m & Write) != 0;
    if (!read && !write)
        return nullptr;

    std::string fmode;
    if (!write)
    {
        fmode = "r";
    }
    else
    {
        const bool exists = FileExists(path);
        if (!exists && (m & NoCreate))
            return nullptr;

        if (m & Append)
            fmode = read ? "a+" : "a";
        else if ((m & Preserve) && exists)
            fmode = "r+"; // open without truncating
        else
            fmode = read ? "w+" : "w";
    }
    if (!(m & Text))
        fmode += "b";

    FILE* f = fopen(path.c_str(), fmode.c_str());
    if (!f)
        return nullptr;

    return new FileHandle {f};
}

FileHandle* OpenLocalFile(const std::string& path, FileMode mode)
{
    return OpenFile(GetLocalFilePath(path), mode);
}

bool FileExists(const std::string& name)
{
    FILE* f = fopen(name.c_str(), "rb");
    if (!f)
        return false;
    fclose(f);
    return true;
}

bool LocalFileExists(const std::string& name)
{
    return FileExists(GetLocalFilePath(name));
}

bool CheckFileWritable(const std::string& filepath)
{
    if (FileExists(filepath))
    {
        FILE* f = fopen(filepath.c_str(), "r+b");
        if (!f)
            return false;
        fclose(f);
        return true;
    }

    FILE* f = fopen(filepath.c_str(), "wb");
    if (!f)
        return false;
    fclose(f);
    remove(filepath.c_str());
    return true;
}

bool CheckLocalFileWritable(const std::string& filepath)
{
    return CheckFileWritable(GetLocalFilePath(filepath));
}

bool CloseFile(FileHandle* file)
{
    if (!file)
        return false;
    int r = fclose(file->File);
    delete file;
    return r == 0;
}

bool IsEndOfFile(FileHandle* file)
{
    return feof(file->File) != 0;
}

bool FileReadLine(char* str, int count, FileHandle* file)
{
    return fgets(str, count, file->File) != nullptr;
}

u64 FilePosition(FileHandle* file)
{
    long pos = ftell(file->File);
    return pos < 0 ? 0 : (u64)pos;
}

bool FileSeek(FileHandle* file, s64 offset, FileSeekOrigin origin)
{
    int whence = SEEK_SET;
    switch (origin)
    {
    case FileSeekOrigin::Start:           whence = SEEK_SET; break;
    case FileSeekOrigin::Current:         whence = SEEK_CUR; break;
    case FileSeekOrigin::End:             whence = SEEK_END; break;
    }
    return fseek(file->File, (long)offset, whence) == 0;
}

void FileRewind(FileHandle* file)
{
    rewind(file->File);
}

u64 FileRead(void* data, u64 size, u64 count, FileHandle* file)
{
    return fread(data, (size_t)size, (size_t)count, file->File);
}

bool FileFlush(FileHandle* file)
{
    return fflush(file->File) == 0;
}

u64 FileWrite(const void* data, u64 size, u64 count, FileHandle* file)
{
    return fwrite(data, (size_t)size, (size_t)count, file->File);
}

u64 FileWriteFormatted(FileHandle* file, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int r = vfprintf(file->File, fmt, args);
    va_end(args);
    return r < 0 ? 0 : (u64)r;
}

u64 FileLength(FileHandle* file)
{
    long pos = ftell(file->File);
    fseek(file->File, 0, SEEK_END);
    long len = ftell(file->File);
    fseek(file->File, pos, SEEK_SET);
    return len < 0 ? 0 : (u64)len;
}

// ---- logging --------------------------------------------------------------

void Log(LogLevel level, const char* fmt, ...)
{
    if (level == LogLevel::Debug)
        return;

    static const char* names[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    fprintf(stderr, "[%s] ", names[(int)level]);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

// ---- save data ------------------------------------------------------------

static void WriteSaveFile(const std::string& path, const u8* data, u32 len, u32 offset, u32 writelen)
{
    if (path.empty() || !data || offset > len || writelen > len - offset)
        return;

    bool fresh = false;
    FILE* f = fopen(path.c_str(), "r+b");
    if (!f)
    {
        f = fopen(path.c_str(), "w+b");
        fresh = true;
    }
    if (!f)
    {
        Log(LogLevel::Error, "Could not open save file %s for writing\n", path.c_str());
        return;
    }

    long curlen = 0;
    if (!fresh)
    {
        fseek(f, 0, SEEK_END);
        curlen = ftell(f);
    }

    if (fresh || curlen != (long)len)
    {
        // new or resized file: write the whole buffer
        fseek(f, 0, SEEK_SET);
        fwrite(data, 1, len, f);
    }
    else
    {
        fseek(f, (long)offset, SEEK_SET);
        fwrite(data + offset, 1, writelen, f);
    }

    fclose(f);
}

void WriteNDSSave(const u8* savedata, u32 savelen, u32 writeoffset, u32 writelen, void* userdata)
{
    if (PS3Frontend::Context* ctx = ContextFrom(userdata))
        WriteSaveFile(ctx->NDSSavePath, savedata, savelen, writeoffset, writelen);
}

void WriteGBASave(const u8* savedata, u32 savelen, u32 writeoffset, u32 writelen, void* userdata)
{
    if (PS3Frontend::Context* ctx = ContextFrom(userdata))
        WriteSaveFile(ctx->GBASavePath, savedata, savelen, writeoffset, writelen);
}

void WriteFirmware(const Firmware&, u32, u32, void*)
{
    // The generated firmware isn't persisted.
}

void WriteDateTime(int, int, int, int, int, int, void*)
{
}

// ---- local multiplayer and networking: not supported ------------------------

void MP_Begin(void*) {}
void MP_End(void*) {}
int MP_SendPacket(u8*, int, u64, void*) { return 0; }
int MP_RecvPacket(u8*, u64*, void*) { return 0; }
int MP_SendCmd(u8*, int, u64, void*) { return 0; }
int MP_SendReply(u8*, int, u64, u16, void*) { return 0; }
int MP_SendAck(u8*, int, u64, void*) { return 0; }
int MP_RecvHostPacket(u8*, u64*, void*) { return 0; }
u16 MP_RecvReplies(u8*, u64, u16, void*) { return 0; }

int Net_SendPacket(u8*, int, void*) { return 0; }
int Net_RecvPacket(u8*, void*) { return 0; }

// ---- camera, microphone, AAC: not supported ---------------------------------

void Camera_Start(int, void*) {}
void Camera_Stop(int, void*) {}
void Camera_CaptureFrame(int, u32*, int, int, bool, void*) {}

void Mic_Start(void*) {}
void Mic_Stop(void*) {}
int Mic_ReadInput(s16*, int, void*) { return 0; }

AACDecoder* AAC_Init() { return nullptr; }
void AAC_DeInit(AACDecoder*) {}
bool AAC_Configure(AACDecoder*, int, int) { return false; }
bool AAC_DecodeFrame(AACDecoder*, const void*, int, void*, int) { return false; }

// ---- add-on controllers: not supported ---------------------------------------

bool Addon_KeyDown(KeyType, void*) { return false; }
void Addon_RumbleStart(u32, void*) {}
void Addon_RumbleStop(void*) {}
float Addon_MotionQuery(MotionQueryType, void*) { return 0.0f; }

DynamicLibrary* DynamicLibrary_Load(const char*) { return nullptr; }
void DynamicLibrary_Unload(DynamicLibrary*) {}
void* DynamicLibrary_LoadFunction(DynamicLibrary*, const char*) { return nullptr; }

}
