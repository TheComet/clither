#include "gmock/gmock.h"
#include "clither/net.h"
#include "clither/msg.h"

#define NAME protocol

using namespace testing;

class NAME : public Test
{
public:
    void SetUp() override
    {
        ASSERT_THAT(net_init(), Eq(0));
        ASSERT_THAT(server_init(&server, "", "5555", ""), Eq(0));
        ASSERT_THAT(client_init(&client, "127.0.0.1", "5555"), Eq(0));
    }
    void TearDown() override
    {
        client_deinit(&client);
        server_deinit(&server, "");
        net_deinit();
    }

protected:
    struct server server;
    struct client client;
};

TEST_F(NAME, server_can_receive_join_message)
{
    msg* m = msg_join_request(0x0000, "username");
    vector_push(&client.pending_reliable, &m);
    client_send_pending_data(&client);
    server_recv(&server);

    server_send_pending_data(&server);
    client_recv(&client);
}
