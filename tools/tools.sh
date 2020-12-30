#!/bin/bash
WORK_PATH=$(cd $(dirname $0); pwd)
echo "WORK_PATH: ${WORK_PATH}"

sudo apt-get update

cd ${WORK_PATH}
if [ ! -d ".config" ]; then
   mkdir .config
fi
if [ ! -d ".tools" ]; then
   mkdir .tools
fi

cd ${WORK_PATH}/.config
if [ ! -d ".config" ]; then
   mkdir .config
   sudo rm -r ~/.config
   sudo ln -s $PWD/.config ~/.config
fi
if [ ! -d ".vscode" ]; then
   mkdir .vscode
   sudo rm -rf ~/.vscode
   sudo ln -s $PWD/.vscode ~/.vscode
fi
if [ ! -d ".tmux" ]; then
   mkdir .tmux
   sudo rm -r ~/.tmux
   sudo ln -s $PWD/.tmux ~/.tmux
fi

# kicad
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
   software-properties-common
if [ ! -f "${WORK_PATH}/.tools/kicad" ]; then
   cd ${WORK_PATH}
   sudo add-apt-repository --yes ppa:kicad/kicad-5.1-releases
   sudo apt-get update
   sudo DEBIAN_FRONTEND=noninteractive apt-get install -y --install-recommends kicad kicad-locale-zh
   echo "kicad install ok" >> ${WORK_PATH}/.tools/kicad
fi

# vscode
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
   libsecret-common \
   libxkbfile1 \
   libsecret-1-0 \
   libxss1 \
   libwayland-server0 \
   libnspr4 \
   libnss3 \
   libgbm1 \
   libasound2 \
   libgtk-3-0 \
   libx11-xcb-dev \
   libxcb-dri3-dev
if [ ! -f "${WORK_PATH}/.tools/vscode" ]; then
   cd ${WORK_PATH}
   sudo dpkg -i vscode/code_1.51.1-1605051630_amd64.deb
   echo "vscode install ok" >> ${WORK_PATH}/.tools/vscode
fi

# tmux
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
   tmux
if [ ! -f "${WORK_PATH}/.tools/tmux" ]; then
   cd ${WORK_PATH}
   cd tmux
   ln -s $PWD/.tmux.conf ~/.tmux.conf
   echo "export TMUX_PATH=${WORK_PATH}" >> ${HOME}/.bashrc
   echo "tmux install ok" >> ${WORK_PATH}/.tools/tmux
fi

sudo apt-get clean
sudo apt-get autoclean
sudo rm -rf /tmp/*
sudo rm -rf /var/tmp/*
sudo rm -rf /var/cache/*
sudo rm -rf /var/lib/apt/lists/*
