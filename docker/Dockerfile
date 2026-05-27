FROM ubuntu:26.04

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive
ENV TRACKER_HOME=/opt/tracker-server

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    curl \
    wget \
    libboost-all-dev \
    libcurl4-openssl-dev \
    libmysqlclient-dev \
    mysql-client \
    nodejs \
    npm \
    supervisor \
    && rm -rf /var/lib/apt/lists/*

# Create tracker directory
WORKDIR ${TRACKER_HOME}

# Copy source code
COPY . .

# Build tracker server
RUN mkdir build && \
    cd build && \
    cmake .. && \
    make -j$(nproc) && \
    cd ..

# Install dashboard dependencies
WORKDIR ${TRACKER_HOME}/dashboard
RUN npm install

# Build dashboard
RUN npm run build

# Create logs directory
RUN mkdir -p ${TRACKER_HOME}/logs

# Create supervisor configuration
RUN mkdir -p /etc/supervisor/conf.d

COPY docker/supervisor.conf /etc/supervisor/conf.d/tracker.conf

# Expose ports
EXPOSE 6969 8080 3000

# Start services
CMD ["/usr/bin/supervisord", "-c", "/etc/supervisor/supervisord.conf"]