FROM public.ecr.aws/amazonlinux/amazonlinux:2023 AS linux-build
SHELL ["/bin/bash", "-c"]
RUN yum update -y

RUN yum install -y tzdata

RUN yum install -y openssl-devel git gcc-c++ make autoconf automake tar

RUN curl -fsSL https://rpm.nodesource.com/setup_18.x | bash - && yum install -y nodejs

RUN curl -O http://www.ijg.org/files/jpegsrc.v9d.tar.gz

RUN tar xzf jpegsrc.v9d.tar.gz && cd jpeg-9d && ./configure --with-pic && make && make install && cd ../

RUN curl https://www.libraw.org/data/LibRaw-0.21.2.tar.gz --output LibRaw-0.21.2.tar.gz
RUN tar xzvf LibRaw-0.21.2.tar.gz && cd LibRaw-0.21.2 && ./configure --with-pic --disable-openmp && touch * && make && make install && cd ../

RUN npm i -g node-gyp node-gyp-build prebuildify
RUN git clone https://github.com/JulianBerger/libraw.js.git
RUN cd libraw.js && git pull origin master && npm install && npm run format-check && npm run lint && npm run build && prebuildify --napi --platform=linux --arch=x64

# debug output the files in the prebuilds folder
RUN ls -la /libraw.js/prebuilds
RUN ls -la /libraw.js/prebuilds/linux-x64

WORKDIR /libraw.js

FROM scratch AS export-stage
COPY --from=linux-build /libraw.js/prebuilds/linux-x64/@julianberger+libraw.js.node .
