#!/bin/bash

# Find next available space id by checking all current plugins
next_space_id () {
    SPACE_IDS=()
    for i in * ; do
        if [ -d "$i" ] && [ "$i" != "CMakeFiles" ]; then
            cd "$i/include/graphene/$i"
            result=$(grep -rnw '.' -e '#define[[:space:]]*[[:alnum:]]*_SPACE_ID')
            B=$(echo $result | cut -d ' ' -f 3)
            if [[ $B =~ [[:digit:]] ]]; then
                SPACE_IDS+=($B)
            fi
            cd "../../../.."
        fi
    done
    max=$( printf "%d\n" "${SPACE_IDS[@]}" | sort -n | tail -1 )
    next=$(($max + 1))
    return $next
}

## create new plugin
if [ $# -ne 1 ]; then
    echo "Usage: $0 my_new_plugin"
    echo "... where my_new_plugin is the name of the plugin you want to create"
    exit 1
fi

pluginName="$1"

echo "Copying template..."
cp -r template_plugin "$pluginName"

echo "Renaming files/directories..."
mv "$pluginName/include/graphene/template_plugin" "$pluginName/include/graphene/$pluginName"
for file in `find "$pluginName" -type f -name '*template_plugin*'`; do mv "$file" `sed s/template_plugin/"$pluginName"/g <<< $file`; done;
echo "Renaming in files..."
find "$pluginName" -type f -exec sed -i "s/template_plugin/$pluginName/g" {} \;
echo "Assigning next available SPACE_ID..."
next_space_id
find "$pluginName" -type f -exec sed -i "s/@SPACE_ID@/$?/g" {} \;

echo "Done! $pluginName is ready."
echo "Next steps:"
echo "1- Add 'add_subdirectory( $pluginName )' to CmakeLists.txt in this directory."
echo "2- Add 'graphene_$pluginName' to ../../programs/witness_node/CMakeLists.txt with the other plugins."
echo "3- Include plugin header file '#include <graphene/$pluginName/$pluginName.hpp>' to ../../programs/witness_node/main.cpp."
echo "4- Initialize plugin with the others with 'auto ${pluginName}_plug = node->register_plugin<$pluginName::$pluginName>();' in ../../programs/witness_node/main.cpp"
echo "5- cmake and make"
echo "6- Start plugin with './../programs/witness_node/witness_node --plugins \"$pluginName\"'. After the seed nodes are added you start to see see a msgs from the plugin 'onBlock' "
