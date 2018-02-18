#!/bin/bash

set -e

sudo docker build --build-arg HOST_USER_ID="$UID" --tag "gitlab-glib" \
    --file "Dockerfile" .
sudo docker run --rm \
    --volume "$(pwd)/..:/home/user/app" --workdir "/home/user/app" \
    --tty --interactive "gitlab-glib" bash
