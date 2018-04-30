

inputs = {
    { uri="ipc://dns-suricata.ipc", script="eve-input.lua"}
}

-- analyze(name, msg)
analyzers = {
--    { name="dns", input = "ipc://dns-suricata.ipc", script="dns-suricata.lua", output = "file://dns-suricata.log" },
--    { name="flow", input = "ipc://flow-suricata.ipc", script="flow-suricata.lua", output = "file://flow-suricata.log" },
    { name="tls", script="tls-suricata.lua" },
--    
--    { name="ying", input = "ipc://ying.ipc", script="ying.lua", output = "ipc://yang.ipc" },
--    { name="yang", input = "ipc://yang.ipc", script="yang.lua", output = "ipc://ying.ipc" },
}

-- output (name, msg)
outputs = {
    { name="eve", uri="ipc://dns-suricata.ipc"}
}
