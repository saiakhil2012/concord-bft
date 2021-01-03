FROM ubuntu:18.04

# Prepare the image
RUN apt-get -y update
RUN apt-get -y install default-jdk
RUN apt -y install maven
RUN apt -y install curl

# Build Execution Engine module.
COPY ./simplekvbc ./simplekvbc
WORKDIR simplekvbc
RUN mvn install

CMD ["java", "-jar", "target/simplekvbc-0.1.jar"]