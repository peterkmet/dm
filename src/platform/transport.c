/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "transport.h"
#include "control.h"
#include "json.h"
#include "turn.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

/* Longest request line. */
#define INPUT_SIZE 65536

#ifdef MSG_NOSIGNAL
/* A closed client does not raise SIGPIPE (Linux). */
#define SEND_FLAGS MSG_NOSIGNAL
#else
/* SO_NOSIGPIPE does the same (macOS). */
#define SEND_FLAGS 0x0
#endif

static int listen_fd = -1;
static int client_fd = -1;
static char socket_path[sizeof(((struct sockaddr_un *)0)->sun_path)];
static char input[INPUT_SIZE];
static size_t input_length;

/* Closes the client connection and forgets its input; a pending turn ends without a reply. */
static void close_client(void)
{
    if (client_fd >= 0)
        close(client_fd);
    client_fd = -1;
    input_length = 0;
    control_drop_client();
}

/* Sends the reply and a line end, then releases the reply. */
static void send_line(Json *j)
{
    json_raw(j, "\n");
    for (size_t sent = 0; client_fd >= 0 && sent < j->len;) {
        ssize_t n = send(client_fd, j->s + sent, j->len - sent, SEND_FLAGS);
        if (n <= 0)
            close_client();
        else
            sent += (size_t)n;
    }
    json_free(j);
}

/* Setup */

bool transport_is_available(void)
{
    return true;
}

bool transport_open(const char *path)
{
    struct sockaddr_un address = {.sun_family = AF_UNIX};
    char dir[sizeof socket_path];
    if (strlen(path) >= sizeof address.sun_path)
        return false;
    snprintf(dir, sizeof dir, "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash && slash != dir) {
        *slash = 0;
        mkdir(dir, 0700);
    }
    snprintf(address.sun_path, sizeof address.sun_path, "%s", path);
    snprintf(socket_path, sizeof socket_path, "%s", path);
    unlink(path);
    turn_reset();
    listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd < 0)
        return false;
    if (bind(listen_fd, (struct sockaddr *)&address, sizeof address) || listen(listen_fd, 1) ||
        fcntl(listen_fd, F_SETFL, O_NONBLOCK)) {
        transport_close();
        return false;
    }
    return true;
}

void transport_close(void)
{
    close_client();
    if (listen_fd >= 0) {
        close(listen_fd);
        unlink(socket_path);
    }
    listen_fd = -1;
}

/* Commands */

/* Accepts a waiting client: false when none waits. */
static bool accept_client(void)
{
    client_fd = accept(listen_fd, NULL, NULL);
    if (client_fd < 0)
        return false;
    fcntl(client_fd, F_SETFL, 0); /* blocking writes; reads use MSG_DONTWAIT */
#ifdef SO_NOSIGPIPE
    int on = 1;
    setsockopt(client_fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof on);
#endif
    return true;
}

/* Appends what the client sent to the input: false when the client is gone. */
static bool read_input(void)
{
    ssize_t n = recv(client_fd, input + input_length, sizeof input - 1 - input_length, MSG_DONTWAIT);
    if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
        close_client();
        return false;
    }
    if (n > 0)
        input_length += (size_t)n;
    input[input_length] = 0;
    return true;
}

/* Answers the complete request lines of the input while no turn is pending; a full input without a line end is an error. */
static void answer_lines(void)
{
    char *end;
    while (client_fd >= 0 && !control_is_turn_pending() && (end = strchr(input, '\n'))) {
        *end = 0;
        if (end > input) {
            Json reply = {0};
            if (control_handle_line(input, &reply))
                send_line(&reply);
            json_free(&reply);
        }
        if (client_fd < 0)
            return;
        size_t used = (size_t)(end + 1 - input);
        memmove(input, end + 1, input_length - used + 1);
        input_length -= used;
    }
    if (input_length == sizeof input - 1) {
        Json reply = {0};
        json_open(&reply, '{');
        json_key_bool(&reply, "ok", false);
        json_key_str(&reply, "error", "request line too long");
        json_close(&reply, '}');
        send_line(&reply);
        input_length = 0;
    }
}

void transport_poll(void)
{
    if (listen_fd < 0)
        return;
    if (control_is_turn_pending()) {
        Json reply = {0};
        if (control_end_turn(&reply))
            send_line(&reply);
        json_free(&reply);
    }
    if (client_fd < 0 && !accept_client())
        return;
    if (!read_input())
        return;
    answer_lines();
}
