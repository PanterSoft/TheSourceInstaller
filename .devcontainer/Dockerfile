# Use Alpine Linux as base image
FROM alpine:latest

# Remove the package manager
#RUN rm -rf /sbin/apk /sbin/apk.static /usr/bin/apk /usr/bin/apk.static /etc/apk

# Remove package metadata
#RUN rm -rf /var/cache/apk /etc/periodic /etc/update-exclude

# Remove other package-related files
#RUN rm -rf /lib/apk /usr/share/apk /usr/local/share/apk /usr/local/bin

# Set up a minimal root filesystem
#RUN mkdir -p /rootfs/bin /rootfs/etc /rootfs/lib /rootfs/usr/bin /rootfs/usr/lib /rootfs/usr/share

# Set the entry point to /bin/sh
CMD ["/bin/sh"]
