FROM debian:stable-slim
LABEL  maintainer="rc@counterflowai.com" domain="counterflow.ai"
RUN apt-get update
RUN apt-get install -y libluajit-5.1 liblua5.1-0-dev libevent-dev lua-socket libatlas-base-dev git make ca-certificates --no-install-recommends
#
# Build redis
#
RUN git clone https://github.com/antirez/redis.git; \
    cd redis/src; \
    make ; \
    make install ; rm -rf redis
RUN rm -rf redis
#
# Build hiredis
#
RUN git clone https://github.com/redis/hiredis.git; \
    cd hiredis \
    make ; make install 
RUN rm -rf hiredis
#
# Build dragonfly
#
ADD dragonfly /opt/dragonfly/
ADD src src
RUN cd src; make clean; make; cp dragonfly /opt/dragonfly/bin/; cd ..
RUN rm -rf src
#
# Build redis ML
#
RUN git clone https://github.com/RedisLabsModules/redis-ml.git; \
    cd redis-ml/src; make; cp redis-ml.so /opt/dragonfly/lib/
RUN rm -rf redis-ml
#
#
#
RUN apt-get purge -y build-essential git make ca-certificates liblua5.1-0-dev libatlas-base-dev ; apt-get autoremove -y ; apt-get autoclean
#
#
#
WORKDIR /opt/dragonfly
#
#
#
ENTRYPOINT redis-server --loadmodule /opt/dragonfly/lib/redis-ml.so --daemonize yes && /opt/dragonfly/bin/dragonfly 

