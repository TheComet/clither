local ls = require("luasnip")
local extras = require("luasnip.extras")
local fmt = require("luasnip.extras.fmt").fmt
local s = ls.snippet
local sn = ls.snippet_node
local c = ls.choice_node
local i = ls.insert_node
local r = ls.restore_node
local t = ls.text_node
local f = ls.function_node
local k = require("luasnip.nodes.key_indexer").new_key
local d = ls.dynamic_node
local rep = extras.rep

c_snippets = {}
cpp_snippets = {}
c_and_cpp_snippets = {
    s({ trig = "log", docstring = "Logging function" }, {
        c(1, { i(1, "log_dbg"), i(2, "log_info"), i(3, "log_warn"), i(4, "log_err") }),
        t('("'), i(2, ""), t('\\n"'),
        d(3, function(values)
            local fmt_string = values[1][1]
            local nodes = {}
            for _ in fmt_string:gmatch("%%[^%%]") do
                local idx = #nodes / 2 + 1
                table.insert(nodes, t(", "))
                table.insert(nodes, r(idx, "arg" .. idx, i(nil, "arg" .. idx)))
            end
            return sn(1, nodes)
        end, { 2 }),
        t(");"),
    }, {
      stored = {},
    }),
}

ls.add_snippets("c", c_snippets, { key = "clither-c" })
ls.add_snippets("cpp", cpp_snippets, { key = "clither-cpp" })
ls.add_snippets("c", c_and_cpp_snippets, { key = "clither-c" })
ls.add_snippets("cpp", c_and_cpp_snippets, { key = "clither-cpp" })
