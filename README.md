

<center><img src="https://github.com/counterflow-ai/dragonfly-mle/blob/master/doc/images/dragonflymle.png" width="450"></center>

#### NAME
dragonfly-mle - Dragonfly Machine Learning Engine (MLE)

#### SYNOPSIS

dragonfly-mle [-p ] [ -v] [ -r root directory ] [ -c chroot directory ] 

#### DESCRIPTION

A scalable, scriptable, streaming application engine for network threat detection built on Redis and LuaJIT.   MLE provides a powerful framework for operationalizing anomaly detection algorithms, threat intelligence lookups, and machine learning predictions with trained models.  MLE is lightweight, fast, and flexible.  It is designed to run in tandem with a deep packet inspection engine like Suricata.  Executing user-defined analyzers implemented in Lua, it can process hundreds of thousands of events per second.  

#### OPTIONS
-p
	Drop privilege
-v
	Verbose mode

-r root directory
	The base directory for dragonfly 

-c chroot directory
	Change root directory by invoking the chroot() system call

#### FEATURES
- Designed to integrate with Suricata
- Implemented in C with scalable multi-threaded execution paths
- User-defined LuaJIT scripting with native support for json and redis
- Native support for Redis ML operations ( https://oss.redislabs.com/redisml/ )
- Able to run as a Dockerize an application

#### ARCHITECTURE
The MLE pipeline implemented as a user-configurable system of queues with three types of event processors:
1.	*Input processor* - pulls messages out of a source, normalizes the data into JSON format, and routes it to the appropriate analyzer queue for processing.  Message sources are either files, Unix sockets, or kafka brokers.  Normalization and ETL operations are performed by a user-defined Lua script.
2.	*Analysis processor* - pulls messages out of the queue, analyzes the event, and routes results to the appropriate output queue for processing.  Analyzers are implemented as user-defined Lua scripts.
3.	*Output processor* - pulls messages out of the queue and delivers it to the appropriate sink.  Message sinks are either file, Unix sockets, or Kafka brokers.

#### CONFIGURATION
The MLE pipeline is defined in a file named config.lua, which is located in the scripts sub directory under the dragonfly root directory.

`${DRAGONFLY_ROOT}/scripts/config.lua`

This file requires three constructs implemented as Lua tables. 

The input table contains configuration for message sources.   Messages can be alerts and/or network security monitoring events.  Valid source types include file, tails, kafka, and ipc.  

    inputs = {
      { tag="eve", uri="tail:///var/log/suricata/eve.json", script="eve-etl-lua"},
      { tag="flow", uri="file:///var/log/suricata/flow.json", script="flow-etl-lua"},
      { tag="dns", uri="ipc:///opt/var/log/suricata/dns.json", script="dns-etl-lua"}
    }

The analyzer table contains configuration for user-definable analyzers.

    analyzers = {
       {tag="flow", script="example-flow.lua"},
       {tag="http", script="example-http.lua"},
       {tag="tls", script="example-tls.lua"}
    }

The output table contains configuration for output sinks.  Valid sink types include file, kafka, and ipc.  

    outputs = {
       {tag="eve", uri="file://eve-alerts.log"},
       {tag="tls", uri="ipc://tls-alerts.log"},
    }

#### DIRECTORY STRUCTURE
To operate successfully, MLE requires a root directory that includes the following structure:


| Directory  | Description |
| ------------- | ------------- |
| ${DRAGONFLY_ROOT}  | base directory  |
| ${DRAGONFLY_ROOT}/scripts  | location of scripts used by input and analyzer processors  |
| ${DRAGONFLY_ROOT}/logs  | directory used by the output processor  |
| ${DRAGONFLY_ROOT}/lib  | location of Redis loadable modules  |


#### QUICK START

Using Docker, this example assumes there is an instance of Suricata already insatlled and running on the host and it is logging to eve.json in directory /var/log/suricata/log.

     $ git clone https://github.com/counterflow-ai/dragonfly-mle.git
     $ cd dragonfly-mle
     $ docker build -t dragonfly .
     $ docker run -it -v /var/log/suricata/log:/opt/dragonfly/log dragonfly

For better grasp on how things function, be sure to study Dockerfile, config.lua and the example scripts referenced.  Remember to rebuild the Docker image whenever any changes are made to any of the scripts.

#### EXAMPLES

1. DNS processing
2. 	Flow processing
3. 	TLS processing

#### BUILTIN MLE LUA FUNCTIONS

	analyzer_event ()
	output_event ()
	http_get ( )

#### TODO
- Implement Kafka consumer and producer
- Implement Parquet output/index
- Instrumentation of performance counters
- documentation @ https://readme.io 

#### LICENSE

GNU General Public License, version 2
