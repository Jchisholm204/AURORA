#!/usr/bin/env zsh

function launch_node(){
    # CSV list of nodes to launch the server on
    local NODE_LIST=${(s:,:)1}

    for NODE in $NODE_LIST; do
        echo "Launching Server on $NODE"
    done
}

launch_node $1
