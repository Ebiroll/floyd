# run with:
# docker build -t floyd/arch -f ./Dockerfile .
# run image with:
# docker run -v ~/shared-vm:/home/src -ti floyd/arch /bin/bash
# where ~/shared-vm is a shared folder, and /home/src is where you work in this docker container.
#
# https://solarianprogrammer.com/2017/12/14/clang-in-docker-container-cpp-17-development/
# https://blobfolio.com/2018/12/using-docker-as-a-build-environment/


FROM archlinux/base

MAINTAINER "Olof Åstrand" <olof.astrand@gmail.com>

ENV container docker

RUN pacman -Sy --noconfirm && pacman -S --noconfirm git gcc make cmake llvm 


#RUN useradd floyd

#work dir

RUN mkdir /home/src

#VOLUME ["/home/src"]

CMD ["/bin/bash"]
