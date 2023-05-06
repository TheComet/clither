#include "gmock/gmock.h"
#include "clither/net.h"
#include "clither/protocol.h"

#define NAME protocol

using namespace testing;

class NAME : public Test
{
public:
    void SetUp() override
    {
        net_init();
        server_init(&server, "", "5555");
        client_init(&client, "127.0.01", "5555");
    }
    void TearDown() override
    {
        client_deinit(&client);
        server_deinit(&server);
        net_deinit();
    }

protected:
    struct server server;
    struct client client;
};

TEST_F(NAME, server_can_receive_join_message)
{
    protocol_join_game_request(&client.pending_reliable, "username");
    client_send_pending_data(&client);

    server_recv(&server);
}
