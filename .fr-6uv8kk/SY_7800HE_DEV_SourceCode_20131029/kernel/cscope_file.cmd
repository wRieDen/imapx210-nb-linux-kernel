find . \( -path "*\out" -o -path "*\.svn" \) -prune -o -type f -a \( -iname "*.c" -o -iname "*.cpp" -o -iname "*.h" -o -iname "*.aidl" -o -iname "*.java" -o -iname "*.mk" -o -iname "*.mak" -o -iname "*.py" -o -iname "*.sh" -o -iname "*.pl" -o -iname "*.prop" -o -iname "*.rc" -o -iname "*.conf" -o -iname "*.xml" -o -iname "Makefile" \) -print > cscope.files && sed -i "s#^\.#`pwd`#g" cscope.files && cscope -bq  && ctags --fields=+KSz --extra=+q -L cscope.files