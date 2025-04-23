#include "clither/tests/LogHelper.hpp"

#include "gmock/gmock.h"

extern "C" {
#include "clither/platform/net.h"
}

#define NAME test_ws

using namespace testing;

struct NAME : Test, LogHelper
{
};

TEST_F(NAME, handshake)
{
    const char* request_header =
        "GET / HTTP/1.1\r\n"
        "Host: localhost:5555\r\n"
        "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:136.0) "
        "Gecko/20100101\r\n"
        "Firefox/136.0\r\n"
        "Accept: */*\r\n"
        "Accept-Language: en-US,en;q=0.5\r\n"
        "Accept-Encoding: gzip, deflate, br, zstd\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "Origin: http://localhost:8000\r\n"
        "Sec-WebSocket-Extensions: permessage-deflate\r\n"
        "Sec-WebSocket-Key: bPz9VcoHQPOfG69pDTW6PA==\r\n"
        "DNT: 1\r\n"
        "Sec-GPC: 1\r\n"
        "Connection: keep-alive, Upgrade\r\n"
        "Sec-Fetch-Dest: empty\r\n"
        "Sec-Fetch-Mode: websocket\r\n"
        "Sec-Fetch-Site: same-site\r\n"
        "Pragma: no-cache\r\n"
        "Cache-Control: no-cache\r\n"
        "Upgrade: websocket\r\n\r\n";
    const char* expected_response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: gnUJtWYWOuwXW8inLMRzLT1qZ9I=\r\n\r\n";

    struct sockfd_vec* sockfds;
    sockfd_vec_init(&sockfds);
    struct net_server* server = net_ws_server.create("", "5555");
    ASSERT_THAT(server, NotNull()) << log().text;
    ASSERT_THAT(net_connect_tcp(&sockfds, "localhost", "5555"), Eq(0))
        << log().text;

    EXPECT_THAT(
        net_send(*vec_first(sockfds), request_header, strlen(request_header)),
        Gt(0))
        << log().text;

    struct net_addr   addr;
    struct net_packet packet;
    net_ws_server.receive(server, &addr, &packet);

    packet.len =
        net_recv(*vec_first(sockfds), packet.data, sizeof(packet.data));
    EXPECT_THAT(packet.len, Gt(0)) << log().text;
    packet.data[packet.len] = '\0';
    EXPECT_THAT((const char*)packet.data, StrEq(expected_response))
        << log().text;

    int* fd;
    vec_for_each (sockfds, fd)
        net_close(*fd);
    sockfd_vec_deinit(sockfds);
    net_ws_server.destroy(server);
}
