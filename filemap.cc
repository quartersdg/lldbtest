#if !defined(__FILEMAP_CC__)
#define __FILEMAP_CC__
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/mman.h>
typedef struct FileMap {
    char    *FileName;
    int     Handle;
    off_t   Size;
    void    *Pointer;
} FileMap;

void UnmapFile(FileMap *Map) {
    if (Map) {
        if (Map->FileName) {
            free(Map->FileName);
        }
        if (Map->Pointer) {
            munmap(Map->Pointer,Map->Size);
            Map->Pointer = 0;
        }
        if (Map->Handle != -1) {
            close(Map->Handle);
            Map->Handle = -1;
        }
    }
}

void MapFile(FileMap* Map, const char *FileName) {
    struct stat sb;

    /* TODO: Keep a global? list of all Maps and their st_mtime, scan
       them every once in a while and if the st_mtime changes
       then refresh */
    Map->FileName = strdup(FileName);
    Map->Handle = open(FileName,O_RDONLY);
    if (-1 != Map->Handle) {
        if (-1 != fstat(Map->Handle,&sb)) {

            Map->Size = sb.st_size;
            if (Map->Size > 0) {
                Map->Pointer = mmap(NULL,Map->Size,PROT_READ,MAP_PRIVATE,Map->Handle,0);
            }

            if (NULL != Map->Pointer) {
                return;
            }
        }
    }
    UnmapFile(Map);
}

#endif /* __FILEMAP_CC__ */
