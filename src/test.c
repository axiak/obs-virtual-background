#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define SEGMENTATION_PORT_FILENAME     ".segmentation.port"
int main(char ** argv)
{
    char* tmpdir = getenv("TMPDIR");
    if (tmpdir == NULL) {
        tmpdir = "/tmp";
    }
    char* segmentation_path = (char *)malloc(strlen(tmpdir) + 1 + strlen(SEGMENTATION_PORT_FILENAME) + 1);
    if (segmentation_path == NULL) {
        return -1;
    }
    strcpy(segmentation_path, tmpdir);
    strcat(segmentation_path, "/");
    strcat(segmentation_path, SEGMENTATION_PORT_FILENAME);

    FILE *file = fopen(segmentation_path, "r");
    free(segmentation_path);

    if (file == NULL) {
        return -1;
    }
    int port;
    size_t rc = fread(&port, 1, sizeof(int), file);
    if (rc != sizeof(int)) {
        return -1;
    }
    printf("Port: %d\n", port);
    return port;

}