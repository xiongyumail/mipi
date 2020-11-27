KICAD_IMAGE_VERSION="kicad:5.1.8"

if [[ "$(sudo docker images -q ${KICAD_IMAGE_VERSION} 2> /dev/null)" == "" ]]; then
  cd tools/kicad-docker/
  sudo ./install.sh
  cd ../..
fi

./tools/kicad-docker/start.sh
