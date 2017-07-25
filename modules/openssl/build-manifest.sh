#!/bin/sh

canonical_library_name() {
    # Use perl regexp because of non greedy capture expression
    echo $1|perl -pe 's|(.*\.so).*|\1|'
}

build_manifest()  {
base=$(basename $2)
# Get the canonical .so library name
canonical=$(canonical_library_name $base)

echo $canonical

# Bake all version of the library and all
# it's symlink into the OSv image
ls $1/$canonical*|while read file; do
    base=$(basename $file)
    echo "/usr/lib/$base: $1/$base separator"
done

# Recurse on the library dependencies avoiding those baked in core OSv
ldd $1/$canonical|grep "=>"|grep -v "libc\."|grep -v "libdl"|grep -v "pthread"|grep -v "vdso"|while read line;
do
    path=$(echo "$line"|cut -f 3 -d ' ')
    libdir=$(dirname $path)
    libname=$(basename $path)
    result=$(build_manifest $libdir $libname)
    echo $result
done
}

openssl_libdir=$(pkg-config --variable libdir openssl)

result=$(build_manifest $openssl_libdir "libssl.so")
echo $result|sed s/separator/'\n'/g|sort|uniq|awk '{$1=$1};1'
