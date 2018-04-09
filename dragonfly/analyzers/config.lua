
analyzers = {
--    { name="dns", input = "ipc://dns-suricata.ipc", script="dns-suricata.lua", output = "file://dns-suricata.log" },
--    { name="flow", input = "ipc://flow-suricata.ipc", script="flow-suricata.lua", output = "file://flow-suricata.log" },
    { name="tls", input = "ipc://tls-suricata.ipc", script="tls-suricata.lua", output = "file://tls-suricata.log" },
--    
--    { name="ying", input = "ipc://ying.ipc", script="ying.lua", output = "ipc://yang.ipc" },
--    { name="yang", input = "ipc://yang.ipc", script="yang.lua", output = "ipc://ying.ipc" },
}
