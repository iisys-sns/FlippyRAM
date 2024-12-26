# Dockerfile
FROM docker.io/library/debian:bullseye-slim

RUN apt-get update \
 && </dev/null DEBIAN_FRONTEND=noninteractive \
    apt-get --yes install --no-install-recommends git cmake build-essential psmisc libc6-dev cron dmidecode bc python3 python3-numpy i2c-tools kmod dialog inetutils-ping locales locales-all zip curl network-manager dbus upower openssl ca-certificates qrencode \
 && apt-get clean \
 && rm -rf /var/lib/apt/lists/*

RUN echo "en_US.UTF-8 UTF-8" >> /etc/locale.gen && locale-gen
ENV LANG=en_US.UTF-8
ENV LANGUAGE=en_US.UTF-8
ENV LC_CTYPE=en_US.UTF-8
ENV LC_NUMERIC=en_US.UTF-8
ENV LC_TIME=en_US.UTF-8
ENV LC_COLLATE=en_US.UTF-8
ENV LC_MONETARY=en_US.UTF-8
ENV LC_MESSAGES=en_US.UTF-8
ENV LC_PAPER=en_US.UTF-8
ENV LC_NAME=en_US.UTF-8
ENV LC_ADDRESS=en_US.UTF-8
ENV LC_TELEPHONE=en_US.UTF-8
ENV LC_MEASUREMENT=en_US.UTF-8
ENV LC_IDENTIFICATION=en_US.UTF-8
ENV LC_ALL=en_US.UTF-8

RUN mkdir -p /arhe/ARHE

WORKDIR /arhe/ARHE

# Add the source code to app
COPY ./ /arhe/ARHE

RUN chmod +x ./run.sh
RUN chmod +x ./main.sh

CMD "./run.sh"
