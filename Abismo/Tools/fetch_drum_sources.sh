#!/usr/bin/env bash
# Fetches the CC0 recordings that build_drum_library.py reads, pinned to the
# commits the shipped library was built from. Only the needed folders are
# checked out (sparse), so the download is a few hundred MB, not gigabytes.
# Usage: fetch_drum_sources.sh <destination folder>
set -euo pipefail
dest="${1:?destination folder}"
mkdir -p "$dest"

fetch() { # repo commit folder paths...
 local repo=$1 commit=$2 dir=$3; shift 3
 if [ ! -d "$dir/.git" ]; then
  git clone -q --filter=blob:none --no-checkout "https://github.com/$repo" "$dir"
 fi
 git -C "$dir" sparse-checkout init --no-cone
 printf '%s\n' "$@" > "$dir/.git/info/sparse-checkout"
 git -C "$dir" fetch -q --depth 1 origin "$commit"
 git -C "$dir" checkout -q "$commit"
}

fetch sgossner/VCSL c1ea7bcc3c7309650ab0da9d15c9cd1fbc4a4c7e "$dest/vcsl" /LICENSE \
 "/Aerophones/Free Aerophones/Siren/" \
 "/Chordophones/Composite Chordophones/Strumstick/" \
 "/Idiophones/Friction Idiophones/Wine Glasses/" \
 "/Idiophones/Plucked Idiophones/Kalimba, Tanzania/" \
 "/Idiophones/Struck Idiophones/Agogo Bells/" \
 "/Idiophones/Struck Idiophones/Anvil/" \
 "/Idiophones/Struck Idiophones/Bell Tree/" \
 "/Idiophones/Struck Idiophones/Brake Drum/" \
 "/Idiophones/Struck Idiophones/Cabasa/" \
 "/Idiophones/Struck Idiophones/Cajon/" \
 "/Idiophones/Struck Idiophones/Clash Cymbals 1/" \
 "/Idiophones/Struck Idiophones/Clash Cymbals 2/" \
 "/Idiophones/Struck Idiophones/Claves/" \
 "/Idiophones/Struck Idiophones/Cowbells/" \
 "/Idiophones/Struck Idiophones/Finger Cymbals/" \
 "/Idiophones/Struck Idiophones/Flexatone/" \
 "/Idiophones/Struck Idiophones/Gong 1/" \
 "/Idiophones/Struck Idiophones/Gong 2/" \
 "/Idiophones/Struck Idiophones/Guiro/" \
 "/Idiophones/Struck Idiophones/Hand Bells, Nepalese/" \
 "/Idiophones/Struck Idiophones/Hi-Hat Cymbal/" \
 "/Idiophones/Struck Idiophones/Mark Trees/" \
 "/Idiophones/Struck Idiophones/Ratchet/" \
 "/Idiophones/Struck Idiophones/Shaker, Large/" \
 "/Idiophones/Struck Idiophones/Shaker, Small/" \
 "/Idiophones/Struck Idiophones/Slapstick/" \
 "/Idiophones/Struck Idiophones/Sleigh Bells/" \
 "/Idiophones/Struck Idiophones/Slit Drum/" \
 "/Idiophones/Struck Idiophones/Suspended Cymbal 1/" \
 "/Idiophones/Struck Idiophones/Suspended Cymbal 2/" \
 "/Idiophones/Struck Idiophones/Tambourine 1/" \
 "/Idiophones/Struck Idiophones/Tambourine 2/" \
 "/Idiophones/Struck Idiophones/Triangles/" \
 "/Idiophones/Struck Idiophones/Tubular Bells 1/" \
 "/Idiophones/Struck Idiophones/Tubular Bells 2/" \
 "/Idiophones/Struck Idiophones/Vibraslap/" \
 "/Idiophones/Struck Idiophones/Woodblock/" \
 "/Membranophones/Other Membranophones/Ocean Drum/" \
 "/Membranophones/Struck Membranophones/Bass Drum 1/" \
 "/Membranophones/Struck Membranophones/Bass Drum 2/" \
 "/Membranophones/Struck Membranophones/Bass Drum 3 - Legacy/" \
 "/Membranophones/Struck Membranophones/Darbuka/" \
 "/Membranophones/Struck Membranophones/Frame Drum/" \
 "/Membranophones/Struck Membranophones/Legacy Toms/" \
 "/Membranophones/Struck Membranophones/Snare Drum, Rope Tension/" \
 "/Membranophones/Struck Membranophones/Timpani 1/" \
 "/Membranophones/Struck Membranophones/Tom 1/"

fetch sfzinstruments/body_percussion 4ac9d8966679c648b62fa10a188179e186b97f24 "$dest/body_percussion" /LICENSE* /Samples/

fetch sfzinstruments/karoryfer.big-rusty-drums f07ce00df34a46b6b08375be56fe116cf15782bc "$dest/big-rusty-drums" /LICENSE* \
 "/Samples/stack_3_layer/*/oh/" "/Samples/china_18/*/oh/" "/Samples/crash_sizzle_17/*/oh/" "/Samples/ride_sizzle_19/*/oh/" \
 "/Samples/hihat_14/*/oh/" "/Samples/kick_24/*/oh/" "/Samples/kick_24_nodamp/kick/oh/" "/Samples/snare_14/*/oh/" \
 "/Samples/tom_22/*/oh/"
echo "sources ready in $dest"
