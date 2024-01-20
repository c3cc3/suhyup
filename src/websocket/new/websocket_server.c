#include <libwebsockets.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Structure to store data for each WebSocket session
struct per_session_data {
};

// Callback function for WebSocket server messages
int callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED: // Handle new connection event
            // Add a timer to send a timestamp every second
            lws_callback_on_writable(wsi);
            break;
        case LWS_CALLBACK_SERVER_WRITEABLE: // Handle send data event
            // Send the timestamp
            time_t t = time(NULL);
            char timestamp[20];
            sprintf(timestamp, "%ld", t);
            lws_write(wsi, timestamp, strlen(timestamp), LWS_WRITE_TEXT);
            lws_callback_on_writable(wsi);
            break;
        // Other events omitted
    }
    return 0;
}

int main(int argc, char **argv) {
    // Create the WebSocket protocol
    static struct lws_protocols protocols[] = {
        {
            "demo-protocol", // Protocol name, should match the WebSocket protocol in the frontend code
            callback, // Callback function pointer
            sizeof(struct per_session_data), // Size of data for each session (connection)
            0, // No additional protocol parameters
            NULL, 
			NULL, 
			NULL
        },
        { NULL, NULL, 0, 0 } // Protocol list ends with NULL
    };

    // Create the WebSocket context
    struct lws_context_creation_info info = {
        .port = 3001, // Listening port number
        .protocols = protocols // Protocol list
    };
    struct lws_context *context = lws_create_context(&info);

    // Check if WebSocket context creation was successful
    if (!context) {
        printf("Failed to create WebSocket context.\n");
        return -1;
    }

    // Enter the loop and wait for WebSocket connections
    while (1) {
        lws_service(context, 50);
    }

    // Clean up and close the WebSocket context
    lws_context_destroy(context);

    return 0;
}
