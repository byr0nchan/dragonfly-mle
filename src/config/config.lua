
-- -----------------------------------------------------------
-- redis parameters
-- -----------------------------------------------------------
redis_host = "127.0.0.1"
redis_port = "6379"

-- -----------------------------------------------------------
-- Input queues/processors
-- -----------------------------------------------------------
inputs = {
   { tag="eve", uri="tail:///var/log/suricata/eve.json", script="suricata-filter.lua"}
}

-- -----------------------------------------------------------
-- Analyzer queues/processors
-- -----------------------------------------------------------
analyzers = {
   -- ---------------------------------------------------------
   -- Examples similar to Bro Frameworks 
   -- See https://www.bro.org/sphinx-git/frameworks/index.html
   -- ---------------------------------------------------------
   -- { tag="file", script="example-file.lua" },
   -- { tag="intel", script="example-intel.lua" },
   -- { tag="control", script="example-control.lua" },
   -- { tag="summary", script="example-summary.lua" },   

   -- ---------------------------------------------------------
   -- General examples 
   -- ---------------------------------------------------------
   -- { tag="tls", script="example-tls.lua" },
   -- { tag="dns", script="example-dns.lua" },
   -- { tag="flow", script="example-flow.lua" },
   -- { tag="ja3", script="example-ja3.lua" },
   { tag="alert", script="example-alert.lua" },
   { tag="nsm", script="example-nsm.lua" },
   -- ---------------------------------------------------------
   -- Machine learning examples using redis
   -- ---------------------------------------------------------
   -- { tag="flow", script="example-flow-ml.lua" },
   -- { tag="dns", script="example-dns-ml.lua" },

}

-- -----------------------------------------------------------
-- Output queues/processors
-- -----------------------------------------------------------
outputs = {
    { tag="log", uri="file://dragonfly-example.log"},
   -- { tag="tls", uri="file://tls-alerts.log"},
   -- { tag="dns", uri="file://dns-alerts.log"},
   -- { tag="flow", uri="file://flow-alerts.log"},
}

