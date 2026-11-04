FROM ubuntu:22.04

RUN echo ">> install distro packages ..." &&  \
    apt update && \
    apt -y install bash-completion sudo build-essential make git python3 curl xz-utils \
        bison flex texinfo libgmp-dev libmpfr-dev libmpc-dev

# The game needs GCC 15 for mips-linux-gnu with LTO (docs/Building.md), built from source
COPY tools/toolchain/build-gcc.sh /tmp/build-gcc.sh
RUN echo ">> build the GCC 15 toolchain (about half an hour) ..." && \
    bash /tmp/build-gcc.sh /opt/ge-gcc15 && rm /tmp/build-gcc.sh
ENV PATH=/opt/ge-gcc15/bin:$PATH

RUN echo ">> setup workspace ..." &&  \
    echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers && \
    useradd -ms /bin/bash dev && usermod -aG sudo dev
USER dev
WORKDIR /home/dev/project

CMD ["/bin/bash"]
