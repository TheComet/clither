#include "clither/fs.h"
#include "clither/log.h"

#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

/* ------------------------------------------------------------------------- */
void*
fs_map_file(const char* utf8_filename, int* length)
{
    int fd;
    struct stat stbuf;
    void* address;

    if ((fd = open(utf8_filename, O_RDONLY)) < 0)
        goto open_failed;

    /*
     * Stat the file and ensure it is a regular file. st_size is only valid
     * for regular files, which we require in order to know the total size
     * of the file
     */
    if (fstat(fd, &stbuf) != 0)
        goto fstat_failed;
    if (!S_ISREG(stbuf.st_mode))
    {
        log_err("fstat failed: File \"%s\" is not a regular file!\n", utf8_filename);
        goto fstat_failed;
    }

    /*
     * Map file into memory. We use MAP_PRIVATE since the file will always
     * be opened in read-only mode and thus we don't need to worry about
     * propagating changes to the file to other processes mapping the same
     * region.
     */
    address = mmap(NULL, stbuf.st_size, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, fd, 0);
    if (address == MAP_FAILED)
        goto mmap_failed;

    /* The file descriptor is no longer required and can be closed. */
    close(fd);

    /* Success */
    *length = (int)stbuf.st_size;
    return address;

mmap_failed:
fstat_failed:
    close(fd);
open_failed:
    return NULL;
}

/* ------------------------------------------------------------------------- */
void
fs_unmap_file(void* address, int length)
{
    munmap(address, length);
}
