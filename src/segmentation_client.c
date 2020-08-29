#include <obs-module.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>

#include "segmentation_client.h"


const char REQUEST_HEADER[] =          {-18, 97, -66, -60, 56, -46, 86, -87};
const char RESPONSE_HEADER[] =         {80, 119, 61, -38, -56, 125, 93, -105};


// utility methods
int get_segmentation_port(SegmentationClient *client, uint64_t current_timestamp);
int get_client_socket(SegmentationClient *client, uint64_t current_timestamp);
void invalidate_connection(SegmentationClient *client);
int write_request(SegmentationClient *client, int sock_fd, const uint8_t *frame_bgr, size_t frame_total_size);
int read_response(SegmentationClient *client, int sock_fd);
uint8_t * get_mask(SegmentationClient *client, size_t mask_size);


SegmentationClient * SegmentationClient_create()
{
    SegmentationClient * client = (SegmentationClient *)bzalloc(sizeof(SegmentationClient));
    if (client == NULL) {
        return NULL;
    }
    client->client_socket = -1;
    client->client_port = -1;
    client->last_port_timestamp = 0;
    client->mask = NULL;
    client->mask_size = 0;
    client->preamble.growshrink = 0;
    client->preamble.width = 0;
    client->preamble.height = 0;
    client->preamble.segmentation_threshold = 0.5f;
    client->preamble.blur = 0;
    client->preamble.length = 0;
    memcpy(client->preamble.header, REQUEST_HEADER, HEADER_LENGTH);
    return client;
}

void SegmentationClient_destroy(SegmentationClient *client)
{
    if (client != NULL) {
        if (client->mask != NULL) {
            bfree(client->mask);
            client->mask = NULL;
        }
        invalidate_connection(client);
        bfree(client);
    }
}

void SegmentationClient_set_dimensions(SegmentationClient *client, int height, int width)
{
    client->preamble.height = (int16_t)height;
    client->preamble.width = (int16_t)width;
}

void SegmentationClient_set_parameters(SegmentationClient *client, float segmentation_threshold, int blur, int growshrink)
{
    client->preamble.segmentation_threshold = segmentation_threshold;
    client->preamble.blur = (int16_t)blur;
    client->preamble.growshrink = (int16_t)growshrink;
}

int SegmentationClient_run_segmentation(SegmentationClient *client, uint64_t timestamp, const uint8_t *frame_bgr, size_t frame_total_size)
{
    int rc, sock_fd;

    sock_fd = get_client_socket(client, timestamp);
    if (sock_fd < 0) {
        return -1;
    }

    rc = write_request(client, sock_fd, frame_bgr, frame_total_size);
    if (rc != 0) {
        fprintf(stderr, "Error writing to segmentation service: %d\n", rc);
        return rc;
    }

    rc = read_response(client, sock_fd);
    if (rc != 0) {
        fprintf(stderr, "Error reading from segmentation service: %d\n", rc);
        return rc;
    }
    return 0;
}

const uint8_t * SegmentationClient_get_mask(SegmentationClient *client)
{
    return client->mask;
}

size_t SegmentationClient_get_mask_size(SegmentationClient *client)
{
    return client->mask_size;
}


int write_request(SegmentationClient *client, int sock_fd, const uint8_t *frame_bgr, size_t frame_total_size)
{
    int written;

    client->preamble.length = sizeof(client->preamble) + frame_total_size;

    written = write(sock_fd, &(client->preamble), sizeof(client->preamble));
    if (written != sizeof(client->preamble)) {
        invalidate_connection(client);
        return SOCK_PREAMBLE_WRITE_FAILURE;
    }

    size_t current_offset = 0;
    while (current_offset < frame_total_size) {
        written = write(sock_fd, frame_bgr + current_offset, frame_total_size - current_offset);
        current_offset += written;
    }
    return 0;
}

int read_response(SegmentationClient *client, int sock_fd)
{
    char response_header[HEADER_LENGTH];
    int read_bytes;

    read_bytes = recv(sock_fd, &response_header, HEADER_LENGTH, MSG_WAITALL);
    if (read_bytes != HEADER_LENGTH) {
        invalidate_connection(client);
        fprintf(stderr, "read_bytes: %d. header_length: %d. %d\n", read_bytes, HEADER_LENGTH, sock_fd);
        return SOCK_NO_HEADER_READ;
    }

    if (strncmp(response_header, RESPONSE_HEADER, HEADER_LENGTH) != 0) {
        invalidate_connection(client);
        return SOCK_INVALID_RESPONSE_HEADER;
    }

    int32_t mask_length;
    read_bytes = recv(sock_fd, &mask_length, sizeof(mask_length), 0);
    if (read_bytes != sizeof(mask_length)) {
        invalidate_connection(client);
        return SOCK_UNDERREAD_MASK;
    }

    if (mask_length < 0) {
        invalidate_connection(client);
        return SOCK_NEGATIVE_RESPONSE_SIZE;
    }

    uint8_t * mask = get_mask(client, mask_length);
    if (mask == NULL) {
        invalidate_connection(client);
        return SOCK_NO_MASK;
    }

    int total_read = 0;
    while (total_read < mask_length) {
        read_bytes = recv(sock_fd, mask + total_read, mask_length - total_read, 0);
        total_read += read_bytes;
    }
    return 0;
}



int get_client_socket(SegmentationClient *client, uint64_t current_timestamp)
{
    if (client->client_socket != -1) {
        return client->client_socket;
    }
    if ((current_timestamp - client->last_connect_timestamp) < MIN_RECONNECT_INTERVAL) {
        return client->client_socket;
    }
    
    int port = get_segmentation_port(client, current_timestamp);
    if (port == -1) {
        return SOCK_NO_SEGMENTATION_PORT;
    }

    // always mark an attempt so we're not thrashing
    client->last_connect_timestamp = current_timestamp;

    struct sockaddr_in server_addr, client_addr;
    client->client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client->client_socket == -1) {
        return SOCK_NO_SOCKET;
    }

    int trueval = 1;
    setsockopt(client->client_socket, SOL_SOCKET, SO_REUSEADDR, &trueval ,sizeof(int));
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    if (setsockopt(client->client_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        close(client->client_socket);
        client->client_socket = -1;
        return -1;
    }

    if (setsockopt(client->client_socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        close(client->client_socket);
        client->client_socket = -1;
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(client->client_port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    struct hostent *server = gethostbyname(SEGMENTATION_HOSTNAME);

    bcopy((char *)server->h_addr, (char *)&server_addr.sin_addr.s_addr, server->h_length);

    if (connect(client->client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(client->client_socket);
        client->client_socket = -1;
        return -1;
    }
    return client->client_socket;
}


int get_segmentation_port(SegmentationClient *client, uint64_t current_timestamp)
{
    if (client->client_port != -1 && (current_timestamp - client->last_port_timestamp) < CHECK_PORT_INTERVAL) {
        return client->client_port;
    }
    char* tmpdir = getenv("TMPDIR");
    if (tmpdir == NULL) {
        tmpdir = "/tmp";
    }
    char* segmentation_path = (char *)malloc(strlen(tmpdir) + 1 + strlen(SEGMENTATION_PORT_FILENAME) + 1);
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
        port = -1;
    }
    fclose(file);
    client->client_port = port;
    client->last_port_timestamp = current_timestamp;
    return port;
}

uint8_t * get_mask(SegmentationClient *client, size_t mask_size)
{
    if (client->mask == NULL || client->mask_size != mask_size) {
        if (client->mask != NULL) {
            bfree(client->mask);
        }
        client->mask_size = mask_size;
        client->mask = bzalloc(mask_size);
    }
    return client->mask;
}

void invalidate_connection(SegmentationClient *client)
{
    if (client->client_socket != -1) {
        close(client->client_socket);
        client->client_socket = -1;
    }
}