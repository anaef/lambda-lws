# build-env
FROM public.ecr.aws/amazonlinux/amazonlinux:2023 AS build-env

ARG LUA_VERSION
ARG LUA_ABI
ARG LUAROCKS_VERSION
ARG LUAROCKS
ARG PACKAGES_BUILD
RUN echo "Building with Lua version: ${LUA_VERSION}, Luarocks version: ${LUAROCKS_VERSION}, and rocks: ${LUAROCKS}"

RUN dnf -y groupinstall "Development Tools" \
	&& dnf -y install libcurl-devel cmake ${PACKAGES_BUILD} \
	&& dnf clean all

WORKDIR /build
RUN git clone --branch 0.12.0 --depth 1 https://github.com/ibireme/yyjson.git \
	&& cd yyjson \
	&& mkdir build \
	&& cd build \
	&& cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DYYJSON_BUILD_TESTS=OFF -DYYJSON_BUILD_DOCS=OFF .. \
	&& make \
	&& make install \
	&& ldconfig
RUN curl -R -O https://www.lua.org/ftp/lua-${LUA_VERSION}.tar.gz \
	&& tar xzf lua-${LUA_VERSION}.tar.gz \
	&& cd lua-${LUA_VERSION} \
	&& make linux CC="gcc -fPIC" \
	&& make INSTALL_TOP=/usr/local install \
	&& cd src \
	&& rm -f lua.o luac.o \
	&& gcc -shared -Wl,-soname,liblua${LUA_ABI}.so -o liblua${LUA_ABI}.so *.o -lm -ldl \
 	&& install -Dm755 liblua${LUA_ABI}.so /usr/local/lib/liblua${LUA_ABI}.so
RUN curl -R -O -L https://luarocks.org/releases/luarocks-${LUAROCKS_VERSION}.tar.gz \
	&& tar xzf luarocks-${LUAROCKS_VERSION}.tar.gz \
	&& cd luarocks-${LUAROCKS_VERSION} \
 	&& ./configure --with-lua=/usr/local --prefix=/usr/local \
	&& make build \
	&& make install

RUN [ -z "${LUAROCKS}" ] || for rock in ${LUAROCKS}; do \
	/usr/local/bin/luarocks install "$rock"; \
done

WORKDIR /build/bootstrap
COPY Makefile /build/bootstrap/
COPY src/ /build/bootstrap/src/
RUN make LUA_ABI=${LUA_ABI}

ENTRYPOINT ["/bin/bash"]
CMD ["-c", "while true; do sleep 3600; done"]


# lambda-lws
FROM public.ecr.aws/lambda/provided:al2023 AS lambda-lws

ARG LUA_ABI
ARG PACKAGES_RUN

COPY --from=build-env /usr/local/ /usr/local 
ENV LD_LIBRARY_PATH="/usr/local/lib:/usr/local/lib64:${LD_LIBRARY_PATH}"
ENV LUA_PATH="/usr/local/share/lua/${LUA_ABI}/?.lua;/usr/local/share/lua/${LUA_ABI}/?/init.lua"
ENV LUA_CPATH="/usr/local/lib/lua/${LUA_ABI}/?.so"

COPY --from=build-env /build/bootstrap/bootstrap /var/runtime/bootstrap

RUN [ -z "${PACKAGES_RUN}" ] || microdnf -y install ${PACKAGES_RUN} && microdnf clean all

# RUN microdnf -y install valgrind \
# 	&& mv /var/runtime/bootstrap /var/runtime/bootstrap0 \
# 	&& echo '#!/bin/bash' > /var/runtime/bootstrap \
# 	&& echo 'valgrind --tool=memcheck --leak-check=full --track-origins=yes --read-var-info=yes --error-exitcode=1 /var/runtime/bootstrap0 "$@"' >> /var/runtime/bootstrap \
# 	&& chmod +x /var/runtime/bootstrap \
# 	&& microdnf clean all


# lambda-lws-examples-default
FROM lambda-lws AS lambda-lws-examples-default

ARG LWS_MATCH
ARG LWS_MAIN
ARG LWS_PATH_INFO
ARG LWS_DIAGNOSTIC
ENV LWS_MATCH=${LWS_MATCH}
ENV LWS_MAIN=${LWS_MAIN}
ENV LWS_PATH_INFO=${LWS_PATH_INFO}
ENV LWS_DIAGNOSTIC=${LWS_DIAGNOSTIC}

COPY examples/default/ /var/task
RUN bash -lc 'shopt -s globstar nullglob; for f in /var/task/**/*.lua; do luac -o "$f" "$f"; done'

EXPOSE 8080

CMD ["lambda-lws-examples-default"]


# lambda-lws-examples-raw
FROM lambda-lws AS lambda-lws-examples-raw

ARG LWS_MAIN
ENV LWS_MAIN=${LWS_MAIN}
ENV LWS_RAW="on"

COPY examples/raw/ /var/task
RUN bash -lc 'shopt -s globstar nullglob; for f in /var/task/**/*.lua; do luac -o "$f" "$f"; done'

EXPOSE 8080

CMD ["lambda-lws-examples-raw"]
