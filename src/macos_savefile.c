// Atomic save installation and three previous-version backups.
#include "macos_savefile.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define fsync _commit
#endif

static int replace_save_file(const char *source, const char *destination)
{
#ifdef _WIN32
    if (MoveFileExA(source, destination, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return 0;
    errno = GetLastError() == ERROR_FILE_NOT_FOUND ? ENOENT : EIO;
    return -1;
#else
    return rename(source, destination);
#endif
}

int kfx_save_is_protected(const char *destination)
{
    char flag[PATH_MAX];return snprintf(flag,sizeof(flag),"%s.protected",destination)<(int)sizeof(flag)&&access(flag,F_OK)==0;
}

int kfx_commit_save(const char *temporary, const char *destination)
{
    char backup[PATH_MAX], pending[PATH_MAX], older[PATH_MAX];
    if(kfx_save_is_protected(destination))return 0;
    struct stat info;
    if (stat(temporary, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size <= 0) return 0;
    if (stat(destination, &info) == 0) {
        if (!S_ISREG(info.st_mode)) return 0;
        if (snprintf(pending,sizeof(pending),"%s.backup-pending",destination) >= (int)sizeof(pending)) return 0;
        FILE *source=fopen(destination,"rb"), *output=NULL;
        if (source) output=fopen(pending,"wb");
        if (!source || !output) { if(source)fclose(source); if(output)fclose(output); return 0; }
        char buffer[16384]; size_t count; int ok=1;
        while ((count=fread(buffer,1,sizeof(buffer),source))>0)
            if (fwrite(buffer,1,count,output)!=count) { ok=0; break; }
        if (ferror(source) || fflush(output)!=0 || fsync(fileno(output))!=0) ok=0;
        fclose(source); if(fclose(output)!=0)ok=0;
        if(!ok){unlink(pending);return 0;}
        // The current save remains untouched until the final atomic rename.
        for(int i=3;i>1;--i){
            if(snprintf(backup,sizeof(backup),"%s.backup%d",destination,i)>=(int)sizeof(backup) ||
               snprintf(older,sizeof(older),"%s.backup%d",destination,i-1)>=(int)sizeof(older)) {unlink(pending);return 0;}
            if(replace_save_file(older,backup)!=0 && errno!=ENOENT){unlink(pending);return 0;}
        }
        if(snprintf(backup,sizeof(backup),"%s.backup1",destination)>=(int)sizeof(backup)){unlink(pending);return 0;}
        if(replace_save_file(pending,backup)!=0){unlink(pending);return 0;}
    } else if(errno!=ENOENT) return 0;
    return replace_save_file(temporary,destination)==0;
}
