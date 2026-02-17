# Use an official GCC image as a parent image
FROM gcc:latest

# Install CMake and dependencies
RUN apt-get update && apt-get install -y \
    cmake \
    pkg-config \
    libcurl4-openssl-dev \
    libpq-dev \
    gdb \
    && rm -rf /var/lib/apt/lists/*

# Set the working directory in the container
WORKDIR /usr/src/friends_trip_bot

## Copy the current directory contents into the container at /usr/src/friends_trip_bot
#COPY . .
#
## Create a build directory
#RUN mkdir build && cd build && \
#    cmake .. && \
#    make
#
## Run the bot
#CMD ["./build/friends_trip_bot"]
