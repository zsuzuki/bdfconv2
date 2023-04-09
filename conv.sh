#!/bin/sh

function help () {
    if [ "$1" != "" ]; then
        echo "** $1"
        echo
    fi
    echo "convert to .fnt from .ttf"
    echo
    echo "conv.sh <ttf-file> <bdf-name> point [-ascii] [-f]"
    echo "  ttf-file: .ttf(.otf) file full path"
    echo "  bdf-name: bdf(not path) name"
    echo "  point: font size"
    echo
    echo "  -ascii: use ascii.txt(default uselist.txt)"
    echo "  -f: force output"
    echo
    echo "example:"
    echo "  conv.sh font/FontFile.ttf ibm 16 -f"
    echo
}

if [ "$1" == "" ]; then
    help "no ttf(otf) file name"
    exit
fi
ttfname="$1"
shift

if [ "$1" == "" ]; then
    help "no bdf name"
    exit
fi
bdfbasename="$1"
shift

if [ "$1" == "" ]; then
    help "no font size"
    exit
fi
point="$1"
shift

bdfname="bdf/$bdfbasename$point.bdf"
fntname="data/$bdfbasename$point.fnt"

listfile="uselist.txt"
force="false"
for arg in "$@"; do
    if [ "$arg" == "-ascii" ]; then
        listfile="alpha.txt"
    elif [ "$arg" == "-f" ]; then
        force="true"
    fi
done

if [ ! -e "$bdfname" -o "$force" == "true" ]; then
    otf2bdf -p $point -r 72 -o $bdfname $ttfname
fi
if [ "$bdfname" -nt "$fntname" -o "$force" == "true" ]; then
    ./build/bdfconv $bdfname $fntname $listfile
fi

echo "done."
