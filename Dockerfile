FROM debian:stretch-slim
LABEL  maintainer="mle@counterflowai.com" domain="counterflow.ai"
RUN apt-get update
RUN apt-get install -y libluajit-5.1 liblua5.1-dev lua-socket libcurl4-openssl-dev libatlas-base-dev libhiredis-dev git make
#
#
#
RUN git clone https://github.com/counterflow-ai/dragonfly-mle; \
    cd dragonfly-mle/src; make 
RUN cp -rp dragonfly-mle/dragonfly /opt/
RUN cp dragonfly-mle/src/dragonfly /opt/dragonfly/bin; \
    mkdir /opt/dragonfly/log; mkdir /opt/dragonfly/run
RUN rm -rf dragonfly-mle
#
# Build redis
#
RUN git clone https://github.com/antirez/redis.git; \
    cd redis/src; make ; make install
RUN rm -rf redis
#
# Build redis ML
#
RUN git clone https://github.com/RedisLabsModules/redis-ml.git; \
    cd redis-ml/src; \
    make ; \
    mkdir /opt/dragonfly/lib ; \
    cp redis-ml.so /opt/dragonfly/lib
RUN rm -rf redis-ml
#
#
#
RUN mkdir /opt/suricata/; mkdir /opt/suricata/var
RUN apt-get purge -y build-essential git make; apt-get autoremove ; apt-get autoclean
#
#
#
WORKDIR /opt/dragonfly
ENTRYPOINT redis-server --loadmodule /opt/dragonfly/lib/redis-ml.so --daemonize yes && /opt/dragonfly/bin/dragonfly 

