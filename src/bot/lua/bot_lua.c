#include "clither/bot/bot.h"
#include "clither/util/log.h"
#include <inttypes.h>

#if defined(__clang__)
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wlong-long"
#elif defined(__GNUC__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wlong-long"
#endif

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#if defined(__clang__)
#    pragma clang diagnostic pop
#elif defined(__GNUC__)
#    pragma GCC diagnostic pop
#endif

static void dumpstack(lua_State* L)
{
    int i;
    int top = lua_gettop(L);
    for (i = 1; i <= top; i++)
    {
        log_raw("%d\t%s\t", i, luaL_typename(L, i));
        switch (lua_type(L, i))
        {
            case LUA_TNUMBER:
                if (lua_isinteger(L, i))
                    log_raw("%d\n", (int)lua_tointeger(L, i));
                else
                    log_raw("%g\n", lua_tonumber(L, i));
                break;
            case LUA_TSTRING: log_raw("%s\n", lua_tostring(L, i)); break;
            case LUA_TBOOLEAN:
                log_raw("%s\n", (lua_toboolean(L, i) ? "true" : "false"));
                break;
            case LUA_TNIL: log_raw("%s\n", "nil"); break;
            default: log_raw("%p\n", lua_topointer(L, i)); break;
        }
    }
}

static void print_error_and_stack(lua_State* L)
{
    log_err("Lua error\n");
    dumpstack(L);
}

static int bot_lua_init(void)
{
    return 0;
}

static void bot_lua_deinit(void)
{
}

static struct bot* bot_lua_create(const char* script_filepath)
{
    lua_State* L = luaL_newstate();
    if (L == NULL)
        goto newstate_failed;
    luaL_openlibs(L);

    if (luaL_dofile(L, script_filepath) != LUA_OK)
    {
        print_error_and_stack(L);
        goto load_script_failed;
    }
    lua_pop(L, -1);

    lua_getglobal(L, "clither_next_cmd");
    if (!lua_isfunction(L, -1))
        log_warn(
            "Script \"%s\" does not define the function "
            "\"clither_next_cmd(world, snake)\"\n",
            script_filepath);
    lua_pop(L, -1);

    return (struct bot*)L;

load_script_failed:
    lua_close(L);
newstate_failed:
    return NULL;
}

static void bot_lua_destroy(struct bot* bot)
{
    lua_close((lua_State*)bot);
}

static int bot_lua_next_cmd(
    const struct bot*   bot,
    struct cmd*         next,
    struct cmd          prev,
    const struct world* world,
    uint8_t             sim_tick_rate)
{
    lua_State* L = (lua_State*)bot;
    *next = prev;

    lua_getglobal(L, "clither_next_cmd");
    if (lua_isfunction(L, -1))
    {
        lua_pushinteger(L, 2);
        lua_pushinteger(L, 3);
        lua_pushinteger(L, sim_tick_rate);
        if (lua_pcall(L, 3, 2, 0) == LUA_OK)
        {
            float angle = lua_tonumber(L, -2);
            float speed = lua_tonumber(L, -1);
            *next = cmd_make(prev, angle, speed, CMD_ACTION_NONE);
            lua_pop(L, -3);
        }
        else
        {
            print_error_and_stack(L);
            lua_pop(L, -1);
        }
    }
    lua_pop(L, -1);

    return 0;
}

struct bot_interface bot_lua = {
    "Lua",
    bot_lua_init,
    bot_lua_deinit,
    bot_lua_create,
    bot_lua_destroy,
    bot_lua_next_cmd,
};
