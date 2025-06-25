FROM gcc:11.3 as build

RUN echo "Start installing dependencies..." && \
    apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip install conan==1.*  && \
    echo "Completing the installation of dependencies."

RUN mkdir -p /usr/srs/app
WORKDIR /usr/srs/app

COPY conanfile.txt ./
RUN echo "Launching Conan..." && \
    mkdir build && cd build && \
    conan install .. -s compiler.libcxx=libstdc++11 -s build_type=Release --build=missing && \
    echo "Conan installation complete."

COPY ./src ./src
COPY ./tests ./tests
COPY CMakeLists.txt ./

RUN echo "Start of build..." && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . && \
    echo "Build completed."


FROM ubuntu:22.04 as run

RUN groupadd -r www && useradd -r -g www www

USER root

RUN mkdir -p /usr/srs/app

USER www

WORKDIR /usr/srs/app

COPY --from=build /usr/srs/app/build/game_server_tests ./
COPY --from=build /usr/srs/app/build/game_server ./
COPY ./data ./data
COPY ./static ./static
COPY --chown=www:www ./saves ./saves
COPY --chown=www:www ./scripts ./scripts

RUN chmod +x ./scripts/start.sh

ENTRYPOINT ["./scripts/start.sh"]