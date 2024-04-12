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

void remove_directory_recursive(const char *path) {
    DIR *dir = opendir(path);
    if (dir == NULL) {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue; 
        }

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        if (entry->d_type == DT_DIR) {
            remove_directory_recursive(full_path); 
        } else {
            remove(full_path); 
        }
    }

    closedir(dir);
    rmdir(path); 
}

void process_directory(const char *path, const char *fileType, time_t *maxModificationTime) {
    DIR *dir = opendir(path);
    if (dir == NULL) {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue; 
        }

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        if (entry->d_type == DT_DIR) {
            process_directory(full_path, fileType, maxModificationTime); 
        } else if (entry->d_type == DT_REG && strstr(entry->d_name, fileType) != NULL) {
            time_t modificationTime = getMaxFileModificationTime(full_path);
            if (modificationTime > *maxModificationTime) {
                *maxModificationTime = modificationTime;
            }
        }
    }

    closedir(dir);
}

void processFilesRecursively(const char *dirPath, const char *fileType, time_t maxModificationTime) {
    DIR *dir = opendir(dirPath);
    if (dir == NULL) {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue; 
        }

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", dirPath, entry->d_name);

        if (entry->d_type == DT_DIR) { 
            processFilesRecursively(full_path, fileType, maxModificationTime);
        } else if (entry->d_type == DT_REG && strstr(entry->d_name, fileType) != NULL) {
            updateFileModificationTime(full_path, maxModificationTime);
        }
    }

    closedir(dir);
}

void processArchive(const char *archivePath, const char *fileType) {
    char *dirName = strdup(dirname(strdup(archivePath)));
    printf("dirNAME= %s\n",dirName);
    char *tempDir = "temp";
    char tempArchive[PATH_MAX];

    remove_directory_recursive(tempDir); 

    if (mkdir(tempDir, 0777) == -1) {
        perror("mkdir");
        return;
    }

    pid_t extractPid = fork();
    if (extractPid == -1) {
        perror("fork");
        return;
    }

    if (extractPid == 0) {
        // Выбор команды извлечения в зависимости от расширения имени файла
        char *extension =  strrchr(archivePath, '.');
        if (extension == NULL) {
            printf("Cannot determine archive type.\n");
            exit(1);
        }
        if (strcmp(extension, ".tar") == 0) {
            execlp("tar", "tar", "-xvf", archivePath, "-C", tempDir, NULL);
        } else if (strcmp(extension, ".zip") == 0) {
            execlp("unzip", "unzip", archivePath, "-d", tempDir, NULL);
        } else if (strcmp(extension, ".gz") == 0) {
            execlp("gunzip", "gunzip", "-c", archivePath, ">", tempDir, NULL);
        } else {
            printf("Unsupported archive type.\n");
            exit(1);
        }
        perror("execlp");
        exit(1);
    } else {
        int extractStatus;
        waitpid(extractPid, &extractStatus, 0);
        if (WIFEXITED(extractStatus) && WEXITSTATUS(extractStatus) == 0) {

            time_t maxModificationTime = 0;
            process_directory(tempDir, fileType, &maxModificationTime);

            processFilesRecursively(tempDir, fileType, maxModificationTime);
   
            snprintf(tempArchive, sizeof(tempArchive), "%s_temp", archivePath);
            char command[PATH_MAX + 100];
            // Создание команды архивирования в зависимости от расширения имени файла
            char *extension = strrchr(archivePath, '.');
            if (extension == NULL) {
                printf("Cannot determine archive type.\n");
                return;
            }
            if (strcmp(extension, ".tar") == 0) {
                snprintf(command, sizeof(command), "tar -cvf %s -C %s --exclude=%s .", tempArchive, tempDir, tempDir);
            } else if (strcmp(extension, ".zip") == 0) {
                char fullpathArchive[PATH_MAX];
                realpath(tempArchive, fullpathArchive);
                snprintf(command, sizeof(command), "cd temp/ && zip -r %s *", fullpathArchive);
                printf("COMMAND = %s",command);
                // printf("Path to tempDir = %s",fullTempDirPath);
                // snprintf(command, sizeof(command), "cd %s && zip -r %s *", fullTempDirPath, tempArchive);


            } else if (strcmp(extension, ".gz") == 0) {
                snprintf(command, sizeof(command), "gzip -c %s > %s", tempDir, tempArchive);
            } else {
                printf("Unsupported archive type.\n");
                return;
            }

            int status = system(command);
            if (status == -1) {
                perror("system");
                return;
            }

            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                printf("Temporary archive created successfully!\n");

                if (remove(archivePath) != 0) {
                    perror("remove");
                    return;
                }

                if (rename(tempArchive, archivePath) != 0) {
                    perror("rename");
                    return;
                }

                remove_directory_recursive(tempDir);
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
