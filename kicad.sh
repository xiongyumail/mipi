#!/bin/bash
IMAGE_VERSION="kicad:5.1.8"

if [[ "$(sudo docker images -q ${IMAGE_VERSION} 2> /dev/null)" == "" ]]; then
  ./tools/kicad-docker/install.sh
fi

./tools/kicad-docker/start.sh '/bin/bash /home/kicad/workspace/sessions.sh'
