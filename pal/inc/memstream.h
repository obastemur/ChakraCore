
#ifndef CHAKRACORE_PAL_INC_MEMSTREAM_H
#define CHAKRACORE_PAL_INC_MEMSTREAM_H

#ifdef __cplusplus
class CCMemoryStream
{
private:
    void   *fd;
    char   tempBuffer[4096];
    size_t bufferPosition;
    bool   wasClosed;
    char   *buffer;
    size_t length;

    void CreateStream();
public:
    CCMemoryStream(): buffer(nullptr), length(0), wasClosed(true), fd(nullptr),
    bufferPosition(0)
    { }

    size_t Write(const char* buffer, size_t length);

    void   Flush();

    void   Close();

    ~CCMemoryStream();

    char   *GetString();

    size_t GetLength();
};
#endif // __cplusplus

#endif // CHAKRACORE_PAL_INC_MEMSTREAM_H
