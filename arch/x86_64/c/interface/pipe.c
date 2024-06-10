#include <stdint.h>
#include <stddef.h>

#include <filesystem.h>
#include <device.h>
#include <pipe.h>
#include <string.h>
#include <memory.h>
#include <unused.h>
#include <sys/errno.h>
#include <system.h>

device_t pipe_device_in = {0};
device_t pipe_device_out = {0};

typedef struct pipe
{
    char *buffer;
    size_t read_pos;
    size_t write_pos;
    size_t size;
    uint64_t flags;
    uint64_t read_dependents;
    uint64_t write_dependents;
    file_descriptor_t *write_fd;
    file_descriptor_t *read_fd;
} pipe_t;

size_t pipe_read(void *ptr, size_t size, size_t nmemb, void *filedes_data, void *device_passed, uint64_t flags) {
    UNUSED(flags);
    UNUSED(device_passed);

    // just read the bytes
    pipe_t *pipe = (pipe_t *)filedes_data;

    size_t read = 0;
    size_t to_read = size * nmemb;

    while (read < to_read) {
        if (pipe->size == 0) {
            // pipe is empty
            return read;
        }

        // read byte
        ((char *)ptr)[read] = pipe->buffer[pipe->read_pos];
        pipe->read_pos++;
        if (pipe->read_pos == PIPE_SIZE) {
            pipe->read_pos = 0;
        }
        pipe->size--;
        read++;
    }

    return read;
}

size_t pipe_write(const void *ptr, size_t size, size_t nmemb, void *filedes_data, void *device_passed, uint64_t flags)
{
    UNUSED(flags);
    UNUSED(device_passed);

    // just write the bytes
    pipe_t *pipe = (pipe_t *)filedes_data;

    size_t written = 0;
    size_t to_write = size * nmemb;

    while (written < to_write)
    {
        if (pipe->size == PIPE_SIZE)
        {
            // pipe is full
            return written;
        }

        // write byte
        pipe->buffer[pipe->write_pos] = ((char *)ptr)[written];
        pipe->write_pos++;
        if (pipe->write_pos == PIPE_SIZE)
        {
            pipe->write_pos = 0;
        }
        pipe->size++;
        written++;
    }

    return written;
}

int pipe_close(void *filedes_data, void *device_passed)
{
    pipe_t *pipe = (pipe_t *)filedes_data;

    if (device_passed == &pipe_device_in)
    {
        pipe->write_dependents--;
    }
    else if (device_passed == &pipe_device_out)
    {
        pipe->read_dependents--;
    }

    if (pipe->read_dependents == 0 && pipe->write_dependents == 0)
    {
        kfree(pipe->buffer);
        kfree(pipe);
    }

    return 0;
}

void *pipe_dup(void *filedes_data, void *device_passed)
{
    pipe_t *pipe = (pipe_t *)filedes_data;

    if (device_passed == &pipe_device_in)
    {
        pipe->write_dependents++;
    }
    else if (device_passed == &pipe_device_out)
    {
        pipe->read_dependents++;
    }

    return pipe;
}

void *pipe_clone(void *filedes_data, void *device_passed)
{
    // copy the file descriptor data
    // since multiple processes can have the same pipe, we can just increase the dependents
    // and return the same pipe
    pipe_t *pipe = (pipe_t *)filedes_data;

    if (device_passed == &pipe_device_in)
    {
        pipe->write_dependents++;
    }
    else if (device_passed == &pipe_device_out)
    {
        pipe->read_dependents++;
    }

    return pipe;
}

int pipe_select(void *filedes_data, void *device_passed, int type)
{
    UNUSED(device_passed);

    pipe_t *pipe = (pipe_t *)filedes_data;

    if (type == SELECT_READ)
    {
        return pipe->size > 0;
    }
    else if (type == SELECT_WRITE)
    {
        return pipe->size < PIPE_SIZE;
    }

    return 0;
}

void pipe_init()
{
    pipe_device_in.write = pipe_write;
    pipe_device_in.close = pipe_close;
    pipe_device_in.dup = pipe_dup;
    pipe_device_in.clone = pipe_clone;
    pipe_device_in.select = pipe_select;
    pipe_device_in.flags = DEVICE_TYPE_PIPE;
    strcpy(pipe_device_in.name, "pipein");

    pipe_device_out.read = pipe_read;
    pipe_device_out.close = pipe_close;
    pipe_device_out.dup = pipe_dup;
    pipe_device_out.clone = pipe_clone;
    pipe_device_out.select = pipe_select;
    pipe_device_out.flags = DEVICE_TYPE_PIPE;
    strcpy(pipe_device_out.name, "pipeout");

    // We don't register this one since it's an internal filesystem device
}

int kpipe(int pipefd[2])
{
    pipe_t *pipe = (pipe_t *)kmalloc(sizeof(pipe_t));
    pipe->read_pos = 0;
    pipe->write_pos = 0;
    pipe->size = 0;
    pipe->buffer = (char *)kmalloc(PIPE_SIZE);
    pipe->flags = 0;
    pipe->read_dependents = 1;
    pipe->write_dependents = 1;

    // freed in
    file_descriptor_t *fd1 = (file_descriptor_t *)kmalloc(sizeof(file_descriptor_t));
    fd1->flags = 0;
    fd1->data = pipe;
    fd1->device = &pipe_device_out;

    file_descriptor_t *fd2 = (file_descriptor_t *)kmalloc(sizeof(file_descriptor_t));
    fd2->flags = 0;
    fd2->data = pipe;
    fd2->device = &pipe_device_in;

    pipe->write_fd = fd1;
    pipe->read_fd = fd2;

    pipefd[0] = add_descriptor(fd1);
    pipefd[1] = add_descriptor(fd2);

    // TODO: debug, remove this
    memset(pipe->buffer, 0, PIPE_SIZE);

    return 0;
}