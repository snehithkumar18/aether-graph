FROM gcr.io/oss-fuzz-base/base-builder
RUN apt-get update && apt-get install -y make cmake
COPY . $SRC/aether_graph
WORKDIR $SRC/aether_graph
COPY .clusterfuzzlite/build.sh $SRC/build.sh
