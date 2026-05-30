# Stage 1: Build React dashboard
FROM node:20-alpine AS dashboard-build
WORKDIR /build
COPY dashboard/package.json ./
RUN npm install
COPY dashboard/ ./
ENV VITE_API_URL=http://localhost:8080
RUN npm run build

# Stage 2: Build tracker server
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libboost-all-dev \
    libcurl4-openssl-dev \
    libmysqlclient-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .
COPY --from=dashboard-build /build/dist ./dashboard/dist

RUN mkdir build && cd build \
    && cmake .. \
    && make -j$(nproc)

# Stage 3: Runtime
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV TRACKER_HOME=/opt/tracker-server

RUN apt-get update && apt-get install -y \
    libboost-system1.74.0 \
    libboost-thread1.74.0 \
    libcurl4 \
    libmysqlclient21 \
    supervisor \
    && rm -rf /var/lib/apt/lists/*

WORKDIR ${TRACKER_HOME}

COPY --from=builder /src/build/tracker-server ./build/tracker-server
COPY --from=builder /src/dashboard/dist ./dashboard/dist
COPY config/tracker.conf ./config/tracker.conf
COPY sql/ ./sql/
COPY docker/supervisor.conf /etc/supervisor/conf.d/tracker.conf

RUN mkdir -p logs \
    && sed -i 's|static_path=dashboard/dist|static_path=/opt/tracker-server/dashboard/dist|' config/tracker.conf

EXPOSE 6969 8080 3000

CMD ["/usr/bin/supervisord", "-c", "/etc/supervisor/supervisord.conf"]
