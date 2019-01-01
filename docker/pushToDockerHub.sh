#!/usr/bin/env bash

echo "${DOCKER_PASSWORD}" | docker login -u "${DOCKER_USERNAME}" --password-stdin

imageName="${DOCKER_USERNAME}/overwatch:${TRAVIS_TAG:-latest}-py${DOCKER_PYTHON_VERSION}"
echo "Pushing image ${imageName} to Docker Hub"
docker push "${imageName}"
