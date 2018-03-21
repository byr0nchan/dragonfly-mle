FROM debian:stable-slim
LABEL  maintainer="rc@counterflowai.com" domain="counterflow.ai"
RUN apt-get update
RUN apt-get install -y libluajit-5.1 libevent-dev libatlas-base-dev git make luarocks ca-certificates --no-install-recommends
RUN /usr/bin/luarocks install lua-cjson
#
# Build dragonfly
#
ADD dragonfly /opt/dragonfly/
ADD src src
RUN cd src; make clean; make; cp dragonfly /opt/dragonfly/bin/; cd ..; rm -rf src
#
# Build redis
#
RUN git clone https://github.com/antirez/redis.git; \
    cd redis/src; \
    make ; \
    make install ; rm -rf redis 
#
# Build redis ML
#
RUN git clone https://github.com/RedisLabsModules/redis-ml.git; \
    cd redis-ml/src; make; cp redis-ml.so /opt/dragonfly/lib/; \
    rm -rf redis-ml
#
# Build lua-hiredis
#
RUN git clone https://github.com/agladysh/lua-hiredis.git; \
    cd lua-hiredis; \
    gcc -O2 -fPIC -I/usr/include/lua5.1 -c src/lua-hiredis.c -o /dev/null -Isrc/ -Ilib/ -Wall --pedantic -Werror --std=c99 ; \
    /usr/bin/luarocks make rockspec/lua-hiredis-scm-1.rockspec ; \
    cd ..; rm -rf lua-hiredis
#
#
#
RUN apt-get purge -y build-essential git make ca-certificates libatlas-base-dev ; apt-get autoremove -y ; apt-get autoclean
#
#
#
WORKDIR /opt/dragonfly
#
#
#
ENTRYPOINT redis-server --loadmodule /opt/dragonfly/lib/redis-ml.so --daemonize yes && /opt/dragonfly/bin/dragonfly 

