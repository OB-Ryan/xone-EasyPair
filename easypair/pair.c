#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <linux/limits.h>

static const char begin_pairing = '1';
static const char stop_pairing = '0';
static const char pairing_path[] = "/sys/bus/usb/drivers/xone-dongle/";
static const char pairing[] = "/pairing";

/*
 * Find the first xone-dongle sysfs instance with a pairing attribute
 */
static int find_path(char *out, size_t outlen) {
    struct dirent *de;
    DIR *dr = opendir(pairing_path);
    if (dr == NULL) {
        fprintf(stderr, "xone-easypair: could not open directory %s: %s\n", pairing_path, strerror(errno));
        return -1;
    }

    while ((de = readdir(dr)) != NULL) {
        char* name = de->d_name;

        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }

        char candidate[PATH_MAX];
        int ret = snprintf(candidate, PATH_MAX, "%s%s%s", pairing_path, name, pairing);
        if (ret < 0) {
            fprintf(stderr, "xone-easypair: snprintf error while building candidate path\n");
            closedir(dr);
            return -1;
        }
        if ((size_t)ret >= PATH_MAX) {
            fprintf(stderr, "xone-easypair: candidate path too long (needed %d bytes, limit %d)\n", ret, PATH_MAX);
            closedir(dr);
            return -1;
        }

        if (access(candidate, F_OK) == 0) {
            if (out == NULL || outlen == 0) {
                closedir(dr);
                return -1;
            }

            if ((size_t)ret + 1 > outlen) {
                fprintf(stderr, "xone-easypair: output buffer too small: need %d, have %zu\n", ret + 1, outlen);
                closedir(dr);
                return -1;
            }

            ret = snprintf(out, outlen, "%s", candidate);
            if (ret < 0 || (size_t)ret >= outlen) {
                fprintf(stderr, "xone-easypair: snprintf failed copying to output buffer\n");
                closedir(dr);
                return -1;
            }

            closedir(dr);
            return 0;
        }
    }

    closedir(dr);
    fprintf(stderr, "xone-easypair: no pairing path found under %s\n", pairing_path);
    return -1;
}

/*
 * Write the requested pairing mode to the selected sysfs attribute
 */
static int write_sysfs(char mode, const char* path) {
    if (mode != begin_pairing && mode != stop_pairing) {
        fprintf(stderr, "xone-easypair: invalid mode for pairing sysfs write\n");
        return -1;
    }
    
    int fd = open(path, O_WRONLY);
    if (fd != -1) {
        ssize_t bytes = write(fd, &mode, sizeof(char));
        if (bytes < 0) {
            fprintf(stderr, "xone-easypair: write failed: %s\n", strerror(errno));
            close(fd);
            return -1;
        } else if (bytes != 1) {
            fprintf(stderr, "xone-easypair: partial write: %zd bytes\n", bytes);
            close(fd);
            return -1;
        }
        close(fd);
    } else {
        fprintf(stderr, "xone-easypair: open failed for %s: %s\n", path, strerror(errno));
        return -1;
    }
    return 0;
}

int main() {    
    char path[PATH_MAX];
    if (find_path(path, sizeof(path)) != 0) {
        return 1;
    }

    if (write_sysfs(begin_pairing, path) != 0) {
        return 1;
    }
    
    return 0;
}
