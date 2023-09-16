#!/bin/bash

export LANG=en_US.UTF-8

USER=user
IMAGENAME=build-supermez80
CONTAINERNAME=${IMAGENAME}-container

main()
{
    if [ "$1" != "" ]; then
        case $1 in
        clean)
            remove_image
            exit 0
            ;;
        *)
            exit 1
            ;;
        esac
    fi

    IMGID=$(image_id)
    if [ "$IMGID" == "" ]; then
        build_image
    fi
    CNTRID=$(container_id)
    if [ "$CNTRID" == "" ]; then
        run_image
    else
        docker start $CNTRID
        docker attach $CNTRID
    fi
}

cleanup_containers()
{
    id=$( docker ps -a --filter ancestor=${IMAGENAME} | grep -v -e '^CONTAINER ID' | awk '{ print $1 }' )
    if [ "${id}" != "" ]; then
        docker stop ${id}
        docker rm ${id}
    fi
}

remove_image()
{
    cleanup_containers
    docker rmi ${IMAGENAME}
}

image_id()
{
    docker image ls | grep -e "^${IMAGENAME}" | awk '{print $3}'
}

container_id()
{
    docker ps -a --filter name=${CONTAINERNAME} | grep -v "^CONTAINER ID" | awk '{print $1}'
}

build_image()
{
    docker build --network=host --file Dockerfile \
           --build-arg="UID=$(id -u)" \
           --build-arg="GID=$(id -g)" \
           --build-arg="USER=${USER}" \
           --tag ${IMAGENAME} .
}

run_image()
{
    docker run  --privileged \
       --name ${CONTAINERNAME} -it \
       --volume="$HOME/.netrc:/home/user/.netrc:ro" \
       --volume="$HOME/.ssh:/home/user/.ssh:ro" \
       --volume="$(pwd)/dot_bashrc:/home/user/.bashrc:rw" \
       --volume="$(pwd)/run.sh:/home/user/run.sh:rw" \
       --volume="$(pwd)/..:/home/user/workspace/github/SuperMEZ80:rw" \
       ${IMAGENAME} \
       /bin/bash -c ./run.sh
}

main $*
