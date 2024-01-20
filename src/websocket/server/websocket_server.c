#include <libwebsockets.h>
#include <time.h>
#include <string.h>

// static struct lws *wsi;
static char response_buffer[LWS_PRE + 128]; // Buffer for response

static int callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
    switch (reason) {
        case LWS_CALLBACK_RECEIVE:
            // 클라이언트로부터 메시지 수신
            // 받은 메시지가 "TIME"이면 현재 시간을 보냄
            if (strncmp((const char *)in, "TIME", len) == 0) {
                time_t current_time;
                time(&current_time);

                // 현재 시간을 문자열로 변환
                const char *time_str = ctime(&current_time);

                // JSON 형태로 응답 메시지 구성
                snprintf(response_buffer, sizeof(response_buffer), "{\"time\": \"%s\"}", time_str);

                // 클라이언트에게 응답 메시지 전송
                lws_write(wsi, (unsigned char *)response_buffer + LWS_PRE, strlen(response_buffer + LWS_PRE), LWS_WRITE_TEXT);
            }
            break;

        default:
            break;
    }

    return 0;
}

// Structure to store data for each WebSocket session
struct per_session_data {
	int id;
	time_t access_time;
};

int main(void)
{
    // Create the WebSocket protocol
    static struct lws_protocols protocols[] = {
        {
            "demo-protocol", // Protocol name, should match the WebSocket protocol in the frontend code
            callback, // Callback function pointer
            sizeof(struct per_session_data), // Size of data for each session (connection)
            0, // No additional protocol parameters
            0, 
			NULL, 
			0
        },
        { NULL, NULL, 0, 0 } // Protocol list ends with NULL
    };

    // Create the WebSocket context
    struct lws_context_creation_info info = {
		.iface = "wlp4s0",
        .port = 3001, // Listening port number
        .protocols = protocols // Protocol list
    };
    struct lws_context *context = lws_create_context(&info);

    // Check if WebSocket context creation was successful
    if (!context) {
        printf("Failed to create WebSocket context.\n");
        return -1;
    }

    printf("Server started on port %d...\n", 3001);

    while (1) {
        lws_service(context, /* timeout */ 50);
    }

    lws_context_destroy(context);

    return 0;
}
