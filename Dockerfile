FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get --yes update
RUN apt-get --yes install bash make clang vim tree

WORKDIR /pyro
COPY cmd ./cmd
COPY embed ./embed
COPY etc ./etc
COPY lib ./lib
COPY src ./src
COPY tests ./tests
COPY makefile ./makefile

RUN make release
RUN make install

WORKDIR /root
RUN echo 'echo "hello world";' > hello.pyro

CMD ["pyro"]
