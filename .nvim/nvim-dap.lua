-- WIP by hondana
-- debugger: lldb
-- dap interface: lldb-vscode (available thru homebrew)

local dap = require('dap');
local lldb_adapter = 'lldb-vscode'
local unpack = table.unpack or _G.unpack

local handler = io.popen("brew --prefix 2>&1; echo $?")
if handler then
    local lines = {}
    for line in handler:lines() do
        table.insert(lines,line)
    end
    io.close(handler)
    local output, status = unpack(lines)
    if status == "0" then
        lldb_adapter = output .. '/opt/llvm/bin/' .. lldb_adapter
    end
end

dap.adapters.lldb = {
    type = 'executable',
    command = lldb_adapter,
    env = {
        LLDB_LAUNCH_FLAG_LAUNCH_IN_TTY = "YES"
    },
    name = 'lldb'
}

dap.configurations.cpp = {
    {
        name = 'Launch',
        type = 'lldb',
        request = 'launch',
        program = function()
            return vim.fn.input('Path to executable: ', vim.fn.getcwd() .. '/', 'file')
        end,
        cwd = '${workspaceFolder}',
        stopOnEntry = false,
        args = {},
        runInTerminal = true,
    },
}

dap.configurations.c = dap.configurations.cpp

-- :lua require("nvim-dap-projects").search_project_config()
