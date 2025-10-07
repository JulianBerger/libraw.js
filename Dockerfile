# This is a multi-platform build Dockerfile
# Use TARGETARCH and TARGETPLATFORM to handle different architectures

FROM amazonlinux:2023 AS linux-build

ENV NODE_VERSION=24
ENV LIBRAW_VERSION=0.21.4

SHELL ["/bin/bash", "-c"]
RUN yum update -y

RUN yum install -y tzdata

RUN yum install -y openssl-devel git gcc-c++ make autoconf automake m4 perl tar

RUN curl -fsSL https://rpm.nodesource.com/setup_${NODE_VERSION}.x | bash - && yum install -y nodejs

RUN curl -O http://www.ijg.org/files/jpegsrc.v9d.tar.gz

RUN tar xzf jpegsrc.v9d.tar.gz && cd jpeg-9d && ./configure --with-pic && make && make install && cd ../

RUN curl https://www.libraw.org/data/LibRaw-${LIBRAW_VERSION}.tar.gz --output LibRaw-${LIBRAW_VERSION}.tar.gz
RUN tar xzvf LibRaw-${LIBRAW_VERSION}.tar.gz && cd LibRaw-${LIBRAW_VERSION} && ./configure --with-pic --disable-openmp && make && make install && cd ../

RUN npm i -g node-gyp node-gyp-build prebuildify
RUN git clone https://github.com/JulianBerger/libraw.js.git

# Determine the architecture for prebuildify
RUN if [ "$TARGETARCH" = "amd64" ]; then \
        ARCH="x64"; \
    else \
        ARCH="$TARGETARCH"; \
    fi && \
    cd libraw.js && git pull origin master && npm install && npm run format-check && npm run lint && npm run build && prebuildify --napi --platform=linux --arch=$ARCH

# Create the appropriate prebuild directory structure
RUN if [ "$TARGETARCH" = "amd64" ]; then \
        mkdir -p /libraw.js/prebuilds/linux-x64 && \
        cp /libraw.js/prebuilds/*.node /libraw.js/prebuilds/linux-x64/ 2>/dev/null || true; \
    else \
        mkdir -p /libraw.js/prebuilds/linux-$TARGETARCH && \
        cp /libraw.js/prebuilds/*.node /libraw.js/prebuilds/linux-$TARGETARCH/ 2>/dev/null || true; \
    fi

# debug output the files in the prebuilds folder
RUN ls -la /libraw.js/prebuilds
RUN if [ "$TARGETARCH" = "amd64" ]; then ls -la /libraw.js/prebuilds/linux-x64; else ls -la /libraw.js/prebuilds/linux-$TARGETARCH; fi

WORKDIR /libraw.js

FROM scratch AS export-stage
COPY --from=linux-build /libraw.js/prebuilds/linux-*/@julianberger+libraw.js.node .
