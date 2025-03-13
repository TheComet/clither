local dap = require("dap")
local last_dba = vim.fn.getcwd() .. '/dba-sources/'

dap.set_log_level("TRACE")

dap.adapters.lldb = {
  type = "executable",
  command = "/usr/bin/lldb-dap",
  name = "lldb",
}

dap.configurations.cpp = {
  {
    name = "Host game",
    type = "lldb",
    request = "launch",
    program = "${workspaceFolder}/build-Debug/bin/clither",
    cwd = "${workspaceFolder}/build-Debug/bin/",
    args = { "--host" },
    stopOnEntry = false,
  },
  {
    name = "Start server",
    type = "lldb",
    request = "launch",
    program = "${workspaceFolder}/build-Debug/bin/clither",
    cwd = "${workspaceFolder}/build-Debug/bin/",
    args = { "--server" },
    stopOnEntry = false,
  },
  {
    name = "Join localhost",
    type = "lldb",
    request = "launch",
    program = "${workspaceFolder}/build-Debug/bin/clither",
    cwd = "${workspaceFolder}/build-Debug/bin/",
    args = { "--ip", "localhost" },
    stopOnEntry = false,
  },
  {
    name        = "Unit Tests",
    type        = "lldb",
    request     = "launch",
    program     = "${workspaceFolder}/build-Debug/bin/clither",
    cwd         = "${workspaceFolder}/build-Debug/bin/",
    args        = { "--tests" },
    stopOnEntry = false,
  },
  {
    name        = "Unit Tests current Suite",
    type        = "lldb",
    request     = "launch",
    program     = "${workspaceFolder}/build-Debug/bin/clither",
    cwd         = "${workspaceFolder}/build-Debug/bin/",
    stopOnEntry = false,
    args        = function()
      local buf = vim.api.nvim_get_current_buf()
      local cursor = vim.api.nvim_win_get_cursor(0)
      local row = cursor[1]

      for i = row, 1, -1 do
        local line = vim.api.nvim_buf_get_lines(buf, i - 1, i, false)[1]
        local suite = line:match("#define%s+NAME%s+(%S+)")
        if suite then
          arg = "--gtest_filter=" .. suite .. ".*"
          print("Running with " .. arg)
          return { "--tests", arg }
        end
      end
      error("No test found")
    end,
  },
  {
    name        = "Unit Test under Cursor",
    type        = "lldb",
    request     = "launch",
    program     = "${workspaceFolder}/build-Debug/bin/clither",
    cwd         = "${workspaceFolder}/build-Debug/bin/",
    stopOnEntry = false,
    args        = function()
      local buf = vim.api.nvim_get_current_buf()
      local cursor = vim.api.nvim_win_get_cursor(0)
      local row = cursor[1]

      local test = nil
      for i = row, 1, -1 do
        -- TEST or TEST_F
        local line = vim.api.nvim_buf_get_lines(buf, i - 1, i, false)[1]
        test = line:match("TEST%s*%(%s*NAME%s*,%s*(%S+)%s*%)")
        if test then
          break
        end
        test = line:match("TEST_F%s*%(%s*NAME%s*,%s*(%S+)%s*%)")
        if test then
          break
        end
      end

      local suite = nil
      for i = row, 1, -1 do
        local line = vim.api.nvim_buf_get_lines(buf, i - 1, i, false)[1]
        suite = line:match("#define%s+NAME%s+(%S+)")
        if suite then
          break
        end
      end

      if suite and test then
        arg = "--gtest_filter=" .. suite .. "." .. test
        print("Running with " .. arg)
        dap.set_breakpoint()
        return { "--tests", arg }
      end

      error("No test found")
    end,
  },
}

dap.configurations.c = dap.configurations.cpp
