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

local function var_to_node_name(value)
    local special_cases = {
        ["ident"] = "identifier",
        ["ass"] = "assignment",
        ["default_case"] = "case_",
        ["decl1"] = "var_decl1",
        ["decl2"] = "var_decl2",
        ["cmd"] = "command",
    }
    return special_cases[value] or value
end

ls.add_snippets("c", {
    s({ trig = "log", docstring = "Logging function" }, {
        c(1, { i(1, "log_err"), i(2, "log_warn"), i(3, "log_dbg"), i(4, "log_info") }),
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
    }),
    s({ trig = "fprintf", docstring = "fprintf" }, {
        t("fprintf("),
        c(1, { i(1, "fp"), i(2, "stderr"), i(3, "stdout") }),
        t(', "'), i(2, ""), t('\\n"'),
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
    }),
}, { key = "clither-c" })

ls.add_snippets("cpp", {
    s({ trig = "log", docstring = "Logging function" }, {
        c(1, { i(1, "log_err"), i(2, "log_warn"), i(3, "log_dbg"), i(4, "log_info") }),
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
    }),
}, { key = "clither-cpp" })
