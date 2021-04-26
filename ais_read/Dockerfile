FROM gcc:4.9
COPY . /usr/src/ais_utils
WORKDIR /usr/src/ais_utils

RUN gcc -o ais_read -Wall -O main.c

FROM alpine:latest

RUN apk --no-cache add ca-certificates

WORKDIR /app/
ENV SERIAL_DEVICE=/dev/ttyS0
ENV SERIAL_SPEED=38400
ENV REMOTE_HOST=data.aishub.net
ENV REMOTE_PORT=2501

RUN apk --no-cache add gcompat

COPY --from=0 /usr/src/ais_utils/ais_read /app
COPY start.sh /app
CMD ["/app/start.sh"]
