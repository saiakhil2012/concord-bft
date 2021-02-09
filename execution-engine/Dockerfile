FROM ubuntu:18.04

# Prepare the image
RUN apt-get -y update
RUN apt-get -y install default-jdk
RUN apt -y install maven
RUN apt -y install curl

# Build Execution Engine module.
COPY ./execution-engine ./execution-engine
WORKDIR execution-engine
RUN mvn install

CMD ["java", "-jar", "target/execution-engine-0.1.jar"]