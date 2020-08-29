#ifndef OBS_VIRTUAL_BACKGROUND_SEGMENTATION_CLIENT_H
#define OBS_VIRTUAL_BACKGROUND_SEGMENTATION_CLIENT_H

#include <stddef.h>
#include <stdint.h>

#define SEGMENTATION_PORT_FILENAME     ".segmentation.port"
#define HEADER_LENGTH                  8
#define CHECK_PORT_INTERVAL            10000
#define MIN_RECONNECT_INTERVAL         5000
#define SEGMENTATION_HOSTNAME          "localhost"



typedef struct {
    char header[HEADER_LENGTH];
    uint32_t length;
    float segmentation_threshold;
    int16_t height;
    int16_t width;
    int16_t blur;
    int16_t growshrink;
} RequestPreamble;


typedef struct {
    int client_port;
    int client_socket;

    uint64_t last_connect_timestamp;
    uint64_t last_port_timestamp;
    RequestPreamble preamble;

    uint8_t * mask;
    size_t mask_size;
} SegmentationClient;

enum SocketError {
    SOCK_SUCCESS = 0,
    SOCK_PREAMBLE_WRITE_FAILURE,
    SOCK_NO_HEADER_READ,
    SOCK_INVALID_RESPONSE_HEADER,
    SOCK_UNDERREAD_MASK,
    SOCK_NEGATIVE_RESPONSE_SIZE,
    SOCK_NO_MASK,
    SOCK_NO_SEGMENTATION_PORT,
    SOCK_NO_SOCKET,
};

SegmentationClient * SegmentationClient_create();
void SegmentationClient_destroy(SegmentationClient *client);

void SegmentationClient_set_dimensions(SegmentationClient *client, int height, int width);
void SegmentationClient_set_parameters(SegmentationClient *client, float segmentation_threshold, int blur, int growshrink);
int SegmentationClient_run_segmentation(SegmentationClient *client, uint64_t timestamp, const uint8_t *frame_bgr, size_t frame_total_size);
const uint8_t * SegmentationClient_get_mask(SegmentationClient *client);
size_t SegmentationClient_get_mask_size(SegmentationClient *client);


#endif //OBS_VIRTUAL_BACKGROUND_SEGMENTATION_CLIENT_H
