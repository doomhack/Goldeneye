#!/bin/bash

# $1: BUILD_DIR
# $2: country code

echo "creating required build directories"

BUILD_DIR=$1
COUNTRYCODE=$2

if [ -z ${BUILD_DIR} ]; then echo "$0: missing argument: BUILD_DIR"; exit 1; fi
if [ -z ${COUNTRYCODE} ]; then echo "$0: missing argument: COUNTRYCODE"; exit 1; fi

dirs=(
    rsp
    src
    src/game
	assets assets/obseg
	assets/obseg/brief
    assets/obseg/chr
    assets/obseg/gun
    assets/obseg/prop
	assets/obseg/text
    assets/obseg/bg
    assets/obseg/setup
    assets/obseg/setup/${COUNTRYCODE}
    assets/obseg/stan
	assets/music
    assets/ramrom
    assets/images
    assets/images/combined
    assets/font
	assets/embedded
    assets/embedded/skeletons
    assets/embedded/player_gait_object
)

mkdir -p "${BUILD_DIR}"

for dir in "${dirs[@]}"; do
    mkdir -p "${BUILD_DIR}/${dir}"
done

exit 0