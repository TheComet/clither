#include "gmock/gmock.h"

extern "C" {
#include "clither/client.h"
#include "clither/net.h"
#include "clither/server.h"
}

#define NAME protocol

using namespace testing;

class NAME : public Test
{
public:
    void SetUp() override
    {
        ASSERT_THAT(net_init(), Eq(0));
        ASSERT_THAT(server_init(&server, "", "5555"), Eq(0));
        client_init(&client);
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
}
