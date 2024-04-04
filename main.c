#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>
#include <utime.h>

time_t getMaxFileModificationTime(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == -1) {
        perror("stat");
        return -1;
    }
    return st.st_mtime;
}

void updateFileModificationTime(const char *filename, time_t newTime) {
    struct utimbuf newTimes;
    newTimes.actime = newTime;
    newTimes.modtime = newTime;
    if (utime(filename, &newTimes) == -1) {
        perror("utime");
    }
}

void processArchive(const char *archivePath, const char *fileType) {
    char *dirName = strdup(dirname(strdup(archivePath)));
    char *tempDir = "temp";
    char tempArchive[PATH_MAX];

    if (rmdir(tempDir) == -1 && errno != ENOENT) {
        perror("rmdir");
        return;
    }

    // Create temporary directory
    if (mkdir(tempDir, 0777) == -1) {
        perror("mkdir");
        return;
    }

    // Extract the archive
    pid_t extractPid = fork();
    if (extractPid == -1) {
        perror("fork");
        return;
    }

    if (extractPid == 0) {
        // Child process
        execlp("tar", "tar", "-xvf", archivePath, "-C", tempDir, NULL);
        perror("execlp");
        exit(1);
    } else {
        // Parent process
        int extractStatus;
        waitpid(extractPid, &extractStatus, 0);
        if (WIFEXITED(extractStatus) && WEXITSTATUS(extractStatus) == 0) {
            // Extraction successful

            // Get the maximum modification time
            DIR *dir;
            struct dirent *entry;
            dir = opendir(tempDir);
            if (dir == NULL) {
                perror("opendir");
                return;
            }
            time_t maxModificationTime = 0;

            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_type == DT_REG && strstr(entry->d_name, fileType) != NULL) {
                    char filePath[PATH_MAX];
                    snprintf(filePath, sizeof(filePath), "%s/%s", tempDir, entry->d_name);

                    time_t modificationTime = getMaxFileModificationTime(filePath);
                    if (modificationTime > maxModificationTime) {
                        maxModificationTime = modificationTime;
                    }
                }
            }

            closedir(dir);

            // Update the modification time of all files in the temporary directory
            dir = opendir(tempDir);
            if (dir == NULL) {
                perror("opendir");
                return;
            }

            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_type == DT_REG && strstr(entry->d_name, fileType) != NULL) {
                    char filePath[PATH_MAX];
                    snprintf(filePath, sizeof(filePath), "%s/%s", tempDir, entry->d_name);

                    updateFileModificationTime(filePath, maxModificationTime);
                }
            }

            closedir(dir);

            // Create a temporary archive with updated files
            snprintf(tempArchive, sizeof(tempArchive), "%s_temp", archivePath);
            char command[PATH_MAX + 100];
            snprintf(command, sizeof(command), "tar -cvf %s -C %s .", tempArchive, tempDir);
            int status = system(command);
            if (status == -1) {
                perror("system");
                return;
            }

            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                printf("Temporary archive created successfully!\n");

                // Remove old archive
                if (remove(archivePath) != 0) {
                    perror("remove");
                    return;
                }

                // Rename temporary archive to original archive name
                if (rename(tempArchive, archivePath) != 0) {
                    perror("rename");
                    return;
                }

                // Remove temporary directory
                dir = opendir(tempDir);
                if (dir == NULL) {
                    perror("opendir");
                    return;
                }

                while ((entry = readdir(dir)) != NULL) {
                    char filePath[PATH_MAX];
                    snprintf(filePath, sizeof(filePath), "%s/%s", tempDir, entry->d_name);
                    remove(filePath);
                }

                closedir(dir);
                rmdir(tempDir);
            } else {
                printf("Failed to create temporary archive.\n");
            }
        } else {
            printf("Extraction failed.\n");
            return;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <archive_path> <file_type>\n", argv[0]);
        return 1;
    }

    const char *archivePath = argv[1];
    const char *fileType = argv[2];
    processArchive(archivePath, fileType);

    return 0;
}