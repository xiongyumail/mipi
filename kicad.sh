KICAD_IMAGE_VERSION="kicad:5.1.8"

if [[ "$(sudo docker images -q ${KICAD_IMAGE_VERSION} 2> /dev/null)" == "" ]]; then
  sudo ./tools/kicad-docker/install.sh
fi

./tools/kicad-docker/start.sh
