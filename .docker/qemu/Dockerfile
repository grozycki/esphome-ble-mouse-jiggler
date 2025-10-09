FROM ubuntu:latest

RUN apt-get update && apt-get install -y qemu-system-xtensa && rm -rf /var/lib/apt/lists/*

# Copy the script to a location in the PATH and make it executable
COPY run_qemu.sh /usr/local/bin/run_qemu.sh
RUN chmod +x /usr/local/bin/run_qemu.sh

# Set the entrypoint to the script.
ENTRYPOINT ["run_qemu.sh"]
