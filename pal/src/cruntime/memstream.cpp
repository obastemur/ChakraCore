#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __APPLE__
#include <sys/cdefs.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <wchar.h>

struct memstream
{
    char**  bufp;
    size_t  *sizep;
    ssize_t len;
    fpos_t  offset;
};

static int
memstream_grow(struct memstream *ms, fpos_t newoff)
{
    char *buf;
    ssize_t newsize;

    if (newoff < 0 || newoff >= SSIZE_MAX)
        newsize = SSIZE_MAX - 1;
    else
        newsize = newoff;
    if (newsize > ms->len)
    {
        buf = (char*)realloc(*ms->bufp, newsize + 1);
        if (buf != NULL)
        {
            memset(buf + ms->len + 1, 0, newsize - ms->len);
            *ms->bufp = buf;
            ms->len = newsize;
            return (1);
        }
        return (0);
    }
    return (1);
}

static void
memstream_update(struct memstream *ms)
{
    assert(ms->len >= 0 && ms->offset >= 0);
    *ms->sizep = ms->len < ms->offset ? ms->len : ms->offset;
}

static int
memstream_write(void *cookie, const char *buf, int len)
{
    struct memstream *ms;
    ssize_t tocopy;

    ms = (struct memstream *)cookie;
    if (!memstream_grow(ms, ms->offset + len))
    {
        return (-1);
    }
    tocopy = ms->len - ms->offset;
    if (len < tocopy)
    {
        tocopy = len;
    }
    memcpy(*ms->bufp + ms->offset, buf, tocopy);
    ms->offset += tocopy;
    memstream_update(ms);
    return (tocopy);
}

static fpos_t
memstream_seek(void *_, fpos_t __, int ___)
{
    abort(); // not supported
    return 0;
}

static int
memstream_close(void *cookie)
{
    free(cookie);
    return 0;
}

FILE *
open_memstream(char **bufp, size_t *sizep)
{
    struct memstream *ms;
    int save_errno;
    FILE *fp;

    if (bufp == NULL || sizep == NULL)
    {
        errno = EINVAL;
        return (NULL);
    }
    *bufp = (char*)calloc(1, 1);
    if (*bufp == NULL)
    {
        return (NULL);
    }
    ms = (struct memstream *)malloc(sizeof(*ms));
    if (ms == NULL)
    {
        save_errno = errno;
        free(*bufp);
        *bufp = NULL;
        errno = save_errno;
        return (NULL);
    }
    ms->bufp = bufp;
    ms->sizep = sizep;
    ms->len = 0;
    ms->offset = 0;
    memstream_update(ms);
    fp = funopen(ms, NULL, memstream_write, memstream_seek,
        memstream_close);
    if (fp == NULL)
    {
        save_errno = errno;
        free(ms);
        free(*bufp);
        *bufp = NULL;
        errno = save_errno;
        return (NULL);
    }
    fwide(fp, -1);
    return (fp);
}

#endif // __APPLE__

#include "memstream.h"

void CCMemoryStream::CreateStream()
{
    wasClosed = false;
    fd = (void*) open_memstream(&buffer, &length);
}

size_t CCMemoryStream::Write(const char* data, size_t dataLength)
{
    if (bufferPosition + dataLength < 4096)
    {
        char *startBuffer = tempBuffer + bufferPosition;
        bufferPosition   += dataLength;
        memcpy(startBuffer, data, dataLength);

        return dataLength;
    }
    else if (bufferPosition)
    {
        if (wasClosed) CreateStream();
        fwrite(tempBuffer, bufferPosition, 1, (FILE*)fd);
        bufferPosition = 0;
    }

    if (wasClosed) CreateStream();
    return fwrite(data, dataLength, 1, (FILE*)fd);
}

void CCMemoryStream::Flush()
{
    if (!length) return;
    if (bufferPosition)
    {
        fwrite(tempBuffer, bufferPosition, 1, (FILE*)fd);
        bufferPosition = 0;
    }

    fflush((FILE*)fd);
}

void CCMemoryStream::Close()
{
    if (wasClosed) return;
    wasClosed = true;

    Flush();
    fclose((FILE*)fd);
}

CCMemoryStream::~CCMemoryStream()
{
    Close();
    if (length)
    {
        free(buffer);
    }
}

char*  CCMemoryStream::GetString()
{
    if (length)
    {
        if (bufferPosition)
        {
            Flush();
        }
        return buffer;
    }
    else
    {
        return tempBuffer;
    }
}

size_t CCMemoryStream::GetLength()
{
    return length + bufferPosition;
}
