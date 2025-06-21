ARG BASE_IMAGE=none
FROM ${BASE_IMAGE}

ENV LC_ALL=C.UTF-8
ENV LANG=C.UTF-8

RUN git clone https://github.com/espressif/svd.git /opt/esp_debug && \
    cd /opt/esp_debug && \
    git checkout be20aa12560889d6125d144cdb48cf615ac17628 && \
    rm -rf .git

RUN git clone https://github.com/MaJerle/c-code-style.git /opt/c_lang_format && \
    cd /opt/c_lang_format && \
    git checkout 69d750f17b8ed80b14c5983f2291868e4e0cacaf && \
    rm -rf .git

RUN apt-get update -y && apt install -y \
    socat\
    udev\
    && rm -rf /var/lib/apt/lists/*

RUN curl -Ls https://astral.sh/uv/install.sh | bash && \
    install -Dm755 /root/.local/bin/uv /usr/local/bin/uv

# Hack for empty user in ubuntu < 24.04 (used by IDF < v5.4) 
# ENV USERNAME=ubuntu
# ENV USER_UID=1000
# ENV USER_GID=1000

# RUN groupadd --gid ${USER_GID} ${USERNAME} \
#     && useradd --uid ${USER_UID} --gid ${USER_GID} -m ${USERNAME} \
#     && usermod -aG dialout,plugdev ${USERNAME}

# Hack usdip to container (WSL host)  
# RUN update-alternatives --install /usr/local/bin/usbip usbip `ls /usr/lib/linux-tools/*/usbip | tail -n1` 20

RUN echo 'source /opt/esp/idf/export.sh > /dev/null 2>&1' >> /etc/bash.bashrc