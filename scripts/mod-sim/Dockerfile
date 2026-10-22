FROM debian:bookworm

ARG MOD_HOST_COMMIT=754070f94bffafb51c7f3d86df47bc04b00f1151
ARG MOD_UI_COMMIT=9b052548b7bead11517df6104a2705145ee39210

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build git ca-certificates pkg-config \
        jackd2 libjack-jackd2-dev liblilv-dev lv2-dev libreadline-dev libfftw3-dev libasound2-dev \
        python3 python3-venv python3-dev libjpeg-dev zlib1g-dev libfreetype-dev \
        jack-capture sox lilv-utils valgrind \
    && rm -rf /var/lib/apt/lists/*

RUN git clone https://github.com/mod-audio/mod-host.git /opt/mod-host \
    && git -C /opt/mod-host checkout ${MOD_HOST_COMMIT} \
    && make -C /opt/mod-host -j"$(nproc)"

RUN git clone https://github.com/mod-audio/mod-ui.git /opt/mod-ui \
    && git -C /opt/mod-ui checkout ${MOD_UI_COMMIT} \
    && make -C /opt/mod-ui/utils -j"$(nproc)" \
    && python3 -m venv /opt/mod-ui-env \
    && /opt/mod-ui-env/bin/pip install --no-cache-dir -r /opt/mod-ui/requirements.txt \
    && sed -i -e 's/collections.MutableMapping/collections.abc.MutableMapping/' \
        /opt/mod-ui-env/lib/python3.*/site-packages/tornado/httputil.py

COPY entrypoint.sh /usr/local/bin/mod-sim
ENTRYPOINT ["/usr/local/bin/mod-sim"]
