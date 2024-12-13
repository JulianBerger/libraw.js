FROM public.ecr.aws/amazonlinux/amazonlinux:2023 AS linux-build

ENV NODE_VERSION=22
ENV LIBRAW_VERSION=0.21.3


SHELL ["/bin/bash", "-c"]
RUN yum update -y

RUN yum install -y tzdata

RUN yum install -y openssl-devel git gcc-c++ make autoconf automake tar

RUN curl -fsSL https://rpm.nodesource.com/setup_${NODE_VERSION}.x | bash - && yum install -y nodejs

RUN curl -O http://www.ijg.org/files/jpegsrc.v9d.tar.gz

RUN tar xzf jpegsrc.v9d.tar.gz && cd jpeg-9d && ./configure --with-pic && make && make install && cd ../

RUN curl https://www.libraw.org/data/LibRaw-${LIBRAW_VERSION}.tar.gz --output LibRaw-${LIBRAW_VERSION}.tar.gz
RUN tar xzvf LibRaw-${LIBRAW_VERSION}.tar.gz && cd LibRaw-${LIBRAW_VERSION} && ./configure --with-pic --disable-openmp && touch * && make && make install && cd ../

RUN npm i -g node-gyp node-gyp-build prebuildify
RUN git clone https://github.com/JulianBerger/libraw.js.git
RUN cd libraw.js && git pull origin master && npm install && npm run format-check && npm run lint && npm run build && prebuildify --napi --platform=linux --arch=x64

# debug output the files in the prebuilds folder
RUN ls -la /libraw.js/prebuilds
RUN ls -la /libraw.js/prebuilds/linux-x64

WORKDIR /libraw.js

FROM scratch AS export-stage
COPY --from=linux-build /libraw.js/prebuilds/linux-x64/@julianberger+libraw.js.node .
