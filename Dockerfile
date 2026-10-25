FROM ubuntu:22.04

RUN echo ">> install distro packages ..." &&  \
    apt update && \
    apt -y install bash-completion sudo binutils-mips-linux-gnu gcc-mips-linux-gnu wget make git python3 build-essential

RUN echo ">> setup workspace ..." &&  \
    echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers && \
    useradd -ms /bin/bash dev && usermod -aG sudo dev
USER dev
WORKDIR /home/dev/project

CMD ["/bin/bash"]
