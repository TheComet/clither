#include "clither/bot/bot.h"
#include "clither/game/snake.h"
#include "clither/game/world.h"
#include "clither/gfx/gfx.h"
#include "clither/platform/fs.h"
#include "clither/util/log.h"
#include "clither/util/morton.h"
#include "clither/util/str.h"
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

static const char WorldKey;
static const char SnakeKey;
static const char IGfxKey;
static const char GfxKey;

struct bot
{
    lua_State*       L;
    struct str*      script_path;
    struct fs_watch* script_watch;
};

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

static int clither_food_grid_for_each_in_radius(lua_State* L)
{
    const struct food* food;
    struct food_grid*  food_grid;
    struct qwpos       lower_pos, upper_pos, pos;
    uint64_t           lower_morton, upper_morton, morton;
    int64_t            lower_idx, upper_idx;
    int32_t            idx;
    qw                 radius, x, y, dx, dy, r_sq;

    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    luaL_checktype(L, 2, LUA_TTABLE);
    luaL_checknumber(L, 3);
    luaL_checktype(L, 4, LUA_TFUNCTION);

    food_grid = lua_touserdata(L, 1);

    {
        lua_pushstring(L, "x");
        lua_rawget(L, 2);
        x = make_qw(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_pushstring(L, "y");
        lua_rawget(L, 2);
        y = make_qw(lua_tonumber(L, -1));
        lua_pop(L, 1);
    }

    radius = make_qw(lua_tonumber(L, 3));
    r_sq = qw_mul(radius, radius);

    lower_pos.x = qw_sub(x, radius);
    lower_pos.y = qw_sub(y, radius);
    upper_pos.x = qw_add(x, radius);
    upper_pos.y = qw_add(y, radius);
    lower_morton = morton_encode_qwpos(lower_pos);
    upper_morton = morton_encode_qwpos(upper_pos);
    lower_idx = food_bmap_lower_bound(food_grid->morton, lower_morton);
    upper_idx = food_bmap_lower_bound(food_grid->morton, upper_morton);
    bmap_for_each_range (
        food_grid->morton, idx, morton, food, lower_idx, upper_idx)
    {
        pos = morton_decode_qwpos(morton);
        dx = qw_sub(pos.x, x);
        dy = qw_sub(pos.y, y);
        if (qw_mul(dx, dx) + qw_mul(dy, dy) > r_sq)
            continue;

        lua_pushvalue(L, 4);
        lua_newtable(L);
        {
            lua_newtable(L);
            {
                lua_pushnumber(L, qw_to_float(pos.x));
                lua_setfield(L, -2, "x");
                lua_pushnumber(L, qw_to_float(pos.y));
                lua_setfield(L, -2, "y");

                lua_setfield(L, -2, "pos");
            }
            lua_newtable(L);
            {
                lua_pushnumber(L, qw_to_float(food->dir.x));
                lua_setfield(L, -2, "x");
                lua_pushnumber(L, qw_to_float(food->dir.y));
                lua_setfield(L, -2, "y");

                lua_setfield(L, -2, "dir");
            }
            lua_pushnumber(L, qw_to_float(food->value));
            lua_setfield(L, -2, "value");
        }

        if (lua_pcall(L, 1, 0, 0) != LUA_OK)
            return lua_error(L);
    }

    return 0;
}

static int clither_gfx_draw_debug_circle(lua_State* L)
{
    const struct gfx_interface** igfx;
    struct gfx**                 gfx;
    struct qwpos                 pos;
    qw                           radius;
    uint32_t                     rgba;

    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checknumber(L, 2);
    luaL_checkinteger(L, 3);

    {
        lua_pushstring(L, "x");
        lua_rawget(L, 1);
        pos.x = make_qw(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_pushstring(L, "y");
        lua_rawget(L, 1);
        pos.y = make_qw(lua_tonumber(L, -1));
        lua_pop(L, 1);
    }

    radius = make_qw(lua_tonumber(L, 2));
    rgba = (uint32_t)lua_tointeger(L, 3);

    lua_rawgetp(L, LUA_REGISTRYINDEX, &IGfxKey);
    lua_rawgetp(L, LUA_REGISTRYINDEX, &GfxKey);
    igfx = lua_touserdata(L, -2);
    gfx = lua_touserdata(L, -1);
    lua_pop(L, 2);

    if (*igfx && *gfx)
        (*igfx)->draw_debug_circle(*gfx, pos, radius, rgba);

    return 0;
}

static void* alloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
    (void)ud;
    (void)osize;
    if (ptr && nsize == 0)
    {
        mem_free(ptr);
        return NULL;
    }
    else if (nsize > 0)
        return mem_realloc(ptr, nsize);
    return NULL;
}

int luaopen_clither(lua_State* L)
{
    lua_newtable(L);
    {
        lua_newtable(L);
        {
            lua_pushcfunction(L, clither_food_grid_for_each_in_radius);
            lua_setfield(L, -2, "for_each_in_radius");
        }
        lua_setfield(L, -2, "food_grid");

        lua_newtable(L);
        {
            lua_pushcfunction(L, clither_gfx_draw_debug_circle);
            lua_setfield(L, -2, "draw_debug_circle");
        }
        lua_setfield(L, -2, "gfx");
    }
    return 1;
}

static int bot_lua_init(void)
{
    return 0;
}

static void bot_lua_deinit(void)
{
}

static struct bot* bot_lua_create(
    const char*                  script_filepath,
    const struct gfx_interface** igfx,
    struct gfx**                 gfx)
{
    struct bot* bot;

    log_info("Creating Lua bot\n");
    bot = mem_alloc(sizeof(struct bot));
    if (bot == NULL)
        goto alloc_bot_failed;

    bot->L = lua_newstate(alloc, NULL);
    if (bot->L == NULL)
        goto newstate_failed;
    luaL_openlibs(bot->L);

    luaL_requiref(bot->L, "clither", luaopen_clither, 1);
    lua_pop(bot->L, 1);

    if (luaL_dofile(bot->L, script_filepath) != LUA_OK)
    {
        log_err("Failed to load script \"%s\"\n", script_filepath);
        dumpstack(bot->L);
        goto load_script_failed;
    }

    str_init(&bot->script_path);
    if (str_set_cstr(&bot->script_path, script_filepath) != 0)
    {
        log_err("Failed to set script path \"%s\"\n", script_filepath);
        goto store_script_path_failed;
    }

    bot->script_watch = fs_watch_init();
    if (bot->script_watch == NULL)
        goto init_script_watch_failed;
    if (fs_watch_file(bot->script_watch, script_filepath) != 0)
    {
        log_err("Failed to watch script \"%s\"\n", script_filepath);
        goto watch_script_failed;
    }

    lua_pushlightuserdata(bot->L, (void*)&WorldKey);
    lua_newtable(bot->L);
    {
        lua_pushnumber(bot->L, 0);
        lua_setfield(bot->L, -2, "inner_radius");
        lua_pushnumber(bot->L, 0);
        lua_setfield(bot->L, -2, "ring_start");
        lua_pushnumber(bot->L, 0);
        lua_setfield(bot->L, -2, "ring_end");
        lua_pushlightuserdata(bot->L, NULL);
        lua_setfield(bot->L, -2, "food_grid");
    }
    lua_settable(bot->L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(bot->L, (void*)&SnakeKey);
    lua_newtable(bot->L);
    {
        lua_newtable(bot->L);
        {
            lua_newtable(bot->L);
            {
                lua_pushnumber(bot->L, 0);
                lua_setfield(bot->L, -2, "y");
                lua_pushnumber(bot->L, 0);
                lua_setfield(bot->L, -2, "x");
            }
            lua_setfield(bot->L, -2, "pos");
            lua_pushnumber(bot->L, 0);
            lua_setfield(bot->L, -2, "angle");
            lua_pushnumber(bot->L, 0);
            lua_setfield(bot->L, -2, "speed");
        }
        lua_setfield(bot->L, -2, "head");

        lua_newtable(bot->L);
        {
            lua_pushnumber(bot->L, 0);
            lua_setfield(bot->L, -2, "scale");
        }
        lua_setfield(bot->L, -2, "param");
    }
    lua_settable(bot->L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(bot->L, (void*)&IGfxKey);
    lua_pushlightuserdata(bot->L, igfx);
    lua_settable(bot->L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(bot->L, (void*)&GfxKey);
    lua_pushlightuserdata(bot->L, gfx);
    lua_settable(bot->L, LUA_REGISTRYINDEX);

    return bot;

watch_script_failed:
    fs_watch_deinit(bot->script_watch);
init_script_watch_failed:
store_script_path_failed:
    str_deinit(bot->script_path);
load_script_failed:
    lua_close(bot->L);
newstate_failed:
    mem_free(bot);
alloc_bot_failed:
    return NULL;
}

static void bot_lua_destroy(struct bot* bot)
{
    log_info("Destroying Lua bot\n");
    fs_watch_deinit(bot->script_watch);
    str_deinit(bot->script_path);
    lua_close(bot->L);
    mem_free(bot);
}

static int bot_lua_next_cmd(
    struct bot*         bot,
    struct cmd*         next,
    struct cmd          prev,
    const struct world* world,
    const struct snake* snake,
    uint8_t             sim_tick_rate)
{
    float      angle, speed;
    lua_State* L = bot->L;

    if (fs_watch_check(bot->script_watch) != 0)
    {
        struct fs_watch* new_watch;
        log_info("Reloading Lua script \"%s\"\n", str_cstr(bot->script_path));

        /* Have to set up watch again */
        new_watch = fs_watch_init();
        if (new_watch == NULL)
            return -1;
        if (fs_watch_file(new_watch, str_cstr(bot->script_path)) != 0)
        {
            fs_watch_deinit(new_watch);
            return -1;
        }
        fs_watch_deinit(bot->script_watch);
        bot->script_watch = new_watch;

        /* Reload script */
        if (luaL_dofile(L, str_cstr(bot->script_path)) != LUA_OK)
        {
            log_err(
                "Failed to reload script \"%s\"\n", str_cstr(bot->script_path));
            dumpstack(L);
            return -1;
        }
    }

    if (lua_getglobal(bot->L, "clither_next_cmd") != LUA_TFUNCTION)
    {
        log_err(
            "Script \"%s\" does not define the function "
            "\"clither_next_cmd(world, snake, sim_tick_rate)\"\n"
            "Here is a minimal example script:\n"
            "function clither_next_cmd(world, snake, sim_tick_rate)\n"
            "    local angle = 0.0  -- radians\n"
            "    local speed = 1.0  -- [0, 1]\n"
            "    return angle, speed\n"
            "end\n",
            str_cstr(bot->script_path));
        lua_pop(bot->L, 1);
        return -1;
    }

    lua_rawgetp(L, LUA_REGISTRYINDEX, &WorldKey);
    {
        lua_pushstring(L, "inner_radius");
        lua_pushnumber(L, qw_to_float(world->inner_radius));
        lua_rawset(L, -3);

        lua_pushstring(L, "ring_start");
        lua_pushnumber(L, qw_to_float(world->ring_start));
        lua_rawset(L, -3);

        lua_pushstring(L, "ring_end");
        lua_pushnumber(L, qw_to_float(world->ring_end));
        lua_rawset(L, -3);

        lua_pushstring(L, "food_grid");
        lua_pushlightuserdata(L, (void*)&world->food_grid);
        lua_rawset(L, -3);
    }

    lua_rawgetp(L, LUA_REGISTRYINDEX, &SnakeKey);
    lua_pushstring(L, "head");
    lua_rawget(L, -2);
    {
        lua_pushstring(L, "angle");
        lua_pushnumber(L, qa_to_float(snake->head.angle));
        lua_rawset(L, -3);

        lua_pushstring(L, "speed");
        lua_pushnumber(L, (float)snake->head.speed / 255.0f);
        lua_rawset(L, -3);

        lua_pushstring(L, "pos");
        lua_rawget(L, -2);
        {
            lua_pushstring(L, "x");
            lua_pushnumber(L, qw_to_float(snake->head.pos.x));
            lua_rawset(L, -3);

            lua_pushstring(L, "y");
            lua_pushnumber(L, qw_to_float(snake->head.pos.y));
            lua_rawset(L, -3);

            lua_pop(L, 2);
        }

        lua_pushstring(L, "param");
        lua_rawget(L, -2);
        {
            lua_pushstring(L, "scale");
            lua_pushnumber(L, qw_to_float(snake_scale(&snake->param)));
            lua_rawset(L, -3);

            lua_pop(L, 1);
        }
    }

    lua_pushinteger(L, sim_tick_rate);

    if (lua_pcall(L, 3, 2, 0) != LUA_OK)
    {
        log_err("Error in Lua script: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return -1;
    }

    angle = lua_tonumber(L, -2);
    speed = lua_tonumber(L, -1);
    *next = cmd_make(prev, angle, speed, CMD_ACTION_NONE);
    lua_pop(L, 2);

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
