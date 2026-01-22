FROM gcc:13

# Install basic development tools
RUN apt-get update && apt-get install -y \
    make \
    gdb \
    valgrind \
    && rm -rf /var/lib/apt/lists/*

# Set the working directory
WORKDIR /workspace

# Copy the source code (but don't build it)
COPY . .

# Default to bash shell for development
CMD ["/bin/bash"]