#!/bin/bash
WORK_PATH=$(cd $(dirname $0); pwd)
echo "WORK_PATH: ${WORK_PATH}"

export PROJECTS_PATH=${WORK_PATH}/projects

session="kicad"

tmux has-session -t $session >/dev/null 2>&1
if [ $? = 0 ];then
    tmux attach-session -t $session
    exit
fi

tmux new-session -d -s $session -n chip_test
tmux split-window -t $session:0 -h

tmux send-keys -t $session:0.0 'kicad' C-m
tmux send-keys -t $session:0.1 'cd ${PROJECTS_PATH};code .' C-m

tmux select-pane -t $session:0.1
tmux attach-session -t $session