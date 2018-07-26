-- ----------------------------------------------
-- Copyright 2018, CounterFlow AI, Inc. 
-- 
-- Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following 
-- conditions are met:
--
-- 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
-- 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer 
--    in the documentation and/or other materials provided with the distribution.
-- 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products 
--    derived from this software without specific prior written permission.
--
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, 
-- BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
-- SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
-- DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
-- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
-- OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
--
-- author: Andrew Fast <af@counterflowai.com>
-- ----------------------------------------------



-- -----------------------------------------------------------
-- redis parameters
-- -----------------------------------------------------------
redis_host = "127.0.0.1"
redis_port = "6379"

-- -----------------------------------------------------------
-- Input queues/processors
-- -----------------------------------------------------------
inputs = {
   { tag="eve", uri="tail:///var/log/suricata/eve.json", script="suricata-filter.lua"}, --Split messages based on type
   --{ tag="flow2", uri="ipc://flow-ipc.log", script="passthrough-filter.lua"}
}

-- -----------------------------------------------------------
-- Analyzer queues/processors
-- -----------------------------------------------------------
analyzers = {
   -- ---------------------------------------------------------
   -- General examples 
   -- ---------------------------------------------------------
   { tag="alert", script="example-alert.lua" }, -- No-op alert analyzer
   { tag="nsm", script="example-nsm.lua" }, -- Simple JSON annotator example
   -- { tag="tls", script="example-tls.lua" }, -- Third-party lookup
   -- { tag="dns", script="example-dns.lua" }, -- Third-party lookup
   -- { tag="flow", script="example-flow.lua" }, -- Third-party lookup
   
   -- ---------------------------------------------------------
   -- Advanced Analytics examples using Redis
   -- ---------------------------------------------------------
   -- { tag="flow1", script="example-hll.lua" }, -- Counting Unique Connections with HyperLogLog
   -- { tag="flow2", script="example-mad.lua" }, -- Flow Outliers using Median Absolute Deviation (MAD)

   -- ---------------------------------------------------------
   -- Machine learning examples using Redis-ML
   -- ---------------------------------------------------------
   -- { tag="dga", script="dga/dga-lr-mle.lua" }, --DGA detector w/ Logistic Regression
   -- { tag="dga", script="dga/dga-rf-mle.lua" }, --DGA detector w/ Random Forest

}

-- -----------------------------------------------------------
-- Output queues/processors
-- -----------------------------------------------------------
outputs = {
    { tag="log", uri="file://dragonfly-example.log"},
    -- { tag="flow2", uri="ipc://flow-ipc.log"},
    -- { tag="tls", uri="file://tls-alerts.log"},
    -- { tag="dns", uri="file://dns-alerts.log"},
    -- { tag="flow", uri="file://flow-alerts.log"},
}

