gcc -w src/main.c src/helper.c `pkg-config libcurl fuse3 --cflags --libs` -o bin/main

# debug mode
$run bin/main -f -s mnt config.txt

# non-debug mode
# $run bin/main -s mnt config.txt

## can change the mnt location as well