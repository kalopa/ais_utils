FROM gcc:4.9
COPY . /usr/src/ais_utils
WORKDIR /usr/src/ais_utils

RUN gcc -o ais_relay -Wall -O main.c

FROM alpine:latest

RUN apk --no-cache add ca-certificates

ENV SRC_HOST=0.0.0.0:4321
ENV DST_HOST=data.aishub.net:2501

EXPOSE 4321
WORKDIR /app/

RUN apk --no-cache add gcompat

COPY --from=0 /usr/src/ais_utils/ais_relay /app
COPY start.sh /app
CMD ["/app/start.sh"]
